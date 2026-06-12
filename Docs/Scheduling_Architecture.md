# SysTick + 标志表非阻塞调度

> 当前实施文档。
>
> 工程采用 `SysTick` 作为 HAL 1ms 时基，并在 `HAL_SYSTICK_Callback()` 中分频生成应用层 10ms tick。中断只设置任务标志，主循环持续调用 `App_Loop()` 并在非中断上下文消费任务。

## 背景

当前工程已经具备几个不同节奏的任务，重构后统一由 10ms tick 标志表调度：

| 任务 | 当前位置 | 当前节奏 | 特点 |
| :--- | :--- | :--- | :--- |
| LED 呼吸/闪烁 | 主循环任务 | 10ms | 周期性、轻量 |
| 按键扫描/消抖 | 主循环任务 | 10ms | 周期性、轻量 |
| 输入事件消费 | 主循环任务 | 20ms | 按键事件处理 |
| 触摸检测/坐标读取 | 主循环任务 | 50ms | 输入事件 + 较重同步读取 |
| 传感器采样 | 主循环任务 | 500ms | AHT20 协作式转换，BMP280 短事务读取 |
| LCD 刷新 | 主循环任务 | 100ms | 周期性、较重 |
| 串口日志 | 多处 `SYS_LOG` | 事件触发或调试周期 | 调试辅助 |

该结构移除运行期主动等待式 `HAL_Delay`，避免传感器转换等待阻塞按键、触摸和 UI 刷新。

## 方案一：Tick + 标志表

该方案使用固定系统节拍，在中断中递增计数并设置任务标志，主循环根据标志执行任务。

### 基本模型

```c
typedef struct {
    volatile uint8_t ledUpdate;
    volatile uint8_t keyScan;
    volatile uint8_t touchProcess;
    volatile uint8_t appStep;
    volatile uint8_t sensorSample;
    volatile uint8_t uiRefresh;
    volatile uint8_t heartbeatLog;
} AppTaskFlags;
```

中断里只做轻量计数与置位：

```c
void App_Timer_10ms_ISR(void)
{
    g_taskFlags.ledUpdate = 1;
    g_taskFlags.keyScan = 1;

    if (++touchDiv >= 5) {
        touchDiv = 0;
        g_taskFlags.touchProcess = 1;
    }

    if (++uiDiv >= 10) {
        uiDiv = 0;
        g_taskFlags.uiRefresh = 1;
    }
}
```

主循环不再阻塞：

```c
while (1) {
    if (g_taskFlags.keyScan) {
        g_taskFlags.keyScan = 0;
        App_KeyScanTask();
    }

    if (g_taskFlags.touchProcess) {
        g_taskFlags.touchProcess = 0;
        App_TouchTask();
    }

    if (g_taskFlags.uiRefresh) {
        g_taskFlags.uiRefresh = 0;
        App_UiRefreshTask();
    }
}
```

### 适合的任务

- LED 动画推进
- 按键扫描和消抖
- 周期触摸检查
- 传感器采样
- LCD 刷新限频
- 心跳日志

### 优点

- 实现简单，适合当前代码结构
- 周期任务频率清晰可控
- 调试直观，串口日志容易对应到任务周期
- 不要求引入动态分配或复杂队列
- 很适合当前 `AppController` 逐步拆分

### 缺点

- 标志位只能表示“任务到期”或“事件发生过”
- 如果同类事件在一次消费前出现多次，标志位无法保留次数和顺序
- 如果某个任务执行过久，会拖慢后续任务

## 方案二：纯中断事件驱动

该方案让外设事件直接驱动系统行为。事件来了就进入中断，并在中断回调中完成主要处理。

### 适合的场景

- UART 接收
- DMA 完成
- 高实时性边沿捕获
- 极低功耗唤醒
- ISR 内能快速完成的动作

### 不适合当前工程的原因

当前触摸读取和 LCD 刷新都不是轻量操作：

- `TouchBsp::readPosition()` 包含 bit-bang 时序、微秒延时、5 次采样、排序和坐标换算
- `LcdBsp::update()` 会执行较多 SPI 写入与绘图操作
- AHT20/BMP280 采样涉及总线访问和设备等待

这些逻辑放进 ISR 会让中断过长，容易影响 `SysTick`、DMA、串口和其他外设响应。

### 优点

- 事件响应最快
- 空闲时可配合 `WFI`/`WFE` 做低功耗
- 对真正异步外设很自然

### 缺点

- ISR 复杂度高
- 共享状态更容易出错
- 调试比时间驱动困难
- 不适合执行触摸读取、传感器采样和 LCD 刷新这类重任务

## 方案三：中断 + FIFO 任务队列

该方案让 ISR 只投递事件，主循环从队列中取事件并处理。它比纯标志位更能保留事件顺序，比纯中断处理更容易控制复杂度。

### 基本模型

```c
typedef enum {
    APP_EVENT_NONE = 0,
    APP_EVENT_TOUCH_IRQ,
    APP_EVENT_KEY_PAGE,
    APP_EVENT_KEY_CONFIRM,
    APP_EVENT_KEY_BACK,
    APP_EVENT_UART_RX_READY,
    APP_EVENT_TIMER_UI_REFRESH,
    APP_EVENT_TIMER_SENSOR_SAMPLE,
} AppEventType;

typedef struct {
    AppEventType type;
    uint32_t tick;
} AppEvent;
```

ISR 或轻量周期任务只入队：

```c
void HAL_GPIO_EXTI_Callback(uint16_t pin)
{
    if (pin == TOUCH_PEN_Pin) {
        App_EventQueuePush(APP_EVENT_TOUCH_IRQ);
    }
}
```

主循环负责消费：

```c
while (1) {
    AppEvent event;
    if (App_EventQueuePop(&event)) {
        App_DispatchEvent(event);
    }
}
```

### 适合的任务

- 触摸按下边沿
- 串口命令包到达
- 需要保留顺序的多个输入事件
- 后续 UI 控件事件，例如点击、确认、返回

### 优点

- 能保留事件顺序
- ISR 仍然很轻
- 更适合表达连续输入、串口命令和 UI 事件
- 可以把“周期任务”和“异步事件”统一到一个调度入口

### 缺点

- 实现复杂度高于标志表
- 需要处理队列满、重复事件、事件合并和溢出统计
- 对纯周期任务不如标志表直接
- 如果滥用队列，主循环会变成难以分析的事件堆积点

## 三方案对比

| 维度 | Tick + 标志表 | 纯中断事件驱动 | 中断 + FIFO 任务队列 |
| :--- | :--- | :--- | :--- |
| 实现复杂度 | 低 | 高 | 中 |
| 周期任务表达 | 很适合 | 不适合 | 可行但偏重 |
| 异步事件表达 | 一般 | 很直接 | 很适合 |
| 事件顺序保留 | 不保留 | 取决于 ISR 设计 | 保留 |
| ISR 负担 | 很轻 | 容易变重 | 很轻 |
| 调试难度 | 低 | 高 | 中 |
| 当前工程适配度 | 高 | 低 | 中高 |
| 适合当前阶段 | 是 | 否 | 可作为下一阶段增强 |

## 当前实施方案

当前工程采用混合策略：

1. 主框架使用 `tick + 标志表`
2. 触摸、串口等真正异步输入按需使用 `中断 + FIFO`
3. 所有重任务在主循环执行
4. 不采用纯中断事件驱动作为整体架构
5. 调度 tick 继续使用 `SysTick`，不新增硬件定时器中断

当前任务周期：

| 周期 | 任务 |
| :--- | :--- |
| 10ms | LED 动画、按键扫描、传感器状态机推进 |
| 20ms | 按键事件消费、轻量交互状态推进 |
| 50ms | 触摸检测与触摸坐标读取 |
| 100ms | UI 刷新、应用状态机推进 |
| 500ms | 启动新一轮传感器采样 |
| 1000ms | 心跳日志、健康状态输出 |

标志表采用 level-style 语义：任务未被主循环消费前，重复到期只合并为一次，不累积队列长度。复制并清除标志时使用极短临界区，任务本身始终在主循环上下文执行。

## AppController 拆分建议

`AppController::run()` 只保留兼容入口，主路径拆分为多个协作式任务：

| 函数 | 建议周期 | 职责 |
| :--- | :--- | :--- |
| `updateLed()` | 10ms | 推进 LED 呼吸/闪烁物理状态 |
| `scanKeys()` | 10ms | 扫描物理按键并消抖 |
| `processInputs()` | 20ms 或事件触发 | 消费按键和触摸事件 |
| `pollTouch()` | 50ms | 检查触摸并读取坐标 |
| `startSensorSample()` | 500ms | 触发新一轮传感器采样 |
| `stepSensors()` | 10ms | 推进 AHT20 协作式转换并读取 BMP280 |
| `updateStateMachine()` | 100ms | 根据最新遥测和输入更新告警状态 |
| `refreshDisplay()` | 100ms 或按需 | 生成 `RenderData` 并刷新 LCD |
| `logHealth()` | 1000ms | 输出调试心跳和关键状态 |

这样可以保持 App 层逻辑清晰，同时让 BSP 层继续负责具体硬件行为。

## AHT20 非阻塞采样

AHT20 触发测量后需要约 80ms 转换时间。运行期不得使用 `HAL_Delay(80)` 等待转换完成，而应使用状态机：

| 状态 | 行为 |
| :--- | :--- |
| `Idle` | 等待 500ms 采样周期触发 |
| `Triggered` | 发送 `0xAC 0x33 0x00` 测量命令并记录完成 deadline |
| `WaitingConversion` | 周期检查 `nowMs` 是否到达 deadline，未到返回 `ERROR_BUSY` |
| `ReadResult` | 到期后读取 6 字节结果并解析温湿度 |
| `Fault` | 通信失败后标记断连，等待后续采样周期重试 |

BMP280 当前使用 normal mode，无固定运行期转换等待，本阶段保留同步短事务读取。

## Touch 处理建议

触摸不建议把 `readPosition()` 放进中断。推荐两种渐进方式：

| 阶段 | 做法 | 说明 |
| :--- | :--- | :--- |
| 第一阶段 | 50ms 周期检查 `isTouched()` | 最小改动，改善 100ms 主循环带来的迟滞 |
| 第二阶段 | `TOUCH_PEN` EXTI 下降沿置位 | 中断只表示有触摸发生 |
| 第三阶段 | 主循环读取坐标并投递 UI 事件 | 保持 `readPosition()` 在非 ISR 环境执行 |

## 实施边界

- ISR 内只允许做计数、置位、入队、清中断标志等轻量动作
- 不在 ISR 中调用 `readPosition()`、`LcdBsp::update()`、I2C 传感器读取或长时间 `printf`
- 不在运行期任务中使用主动等待式 `HAL_Delay`
- 不引入动态分配
- 队列容量固定，溢出要有计数或日志
- CubeMX 仍负责 GPIO/EXTI/NVIC 配置，BSP 和 App 不重新配置已经归 `.ioc` 管理的引脚

## 验证标准

1. `main()` 主循环不再包含 `HAL_Delay(100)`
2. AHT20 运行期采样不再包含 `HAL_Delay(80)`
3. `App_Timer_10ms_ISR()` 不执行 LCD、I2C、触摸坐标读取或日志输出
4. `SysTick_Handler()` 仍调用 `HAL_IncTick()`，保证 HAL 时基正常
5. 按键、触摸、LED、LCD、AHT20/BMP280 和报警逻辑上板无回归
