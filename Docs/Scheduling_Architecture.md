# 调度架构方案对比与路线

> 架构设计文档，非当前实现说明。
>
> 当前工程仍以 `SysTick` 驱动 10ms 周期维护任务，并在主循环中通过 `HAL_Delay(100)` 节流执行 `App_Loop()`。本文用于指导后续从阻塞式主循环演进到非阻塞调度结构。

## 背景

当前工程已经具备几个不同节奏的任务：

| 任务 | 当前位置 | 当前节奏 | 特点 |
| :--- | :--- | :--- | :--- |
| LED 呼吸/闪烁 | `App_Timer_10ms_ISR()` | 10ms | 周期性、轻量 |
| 按键扫描/消抖 | `App_Timer_10ms_ISR()` | 10ms | 周期性、轻量 |
| 触摸检测/坐标读取 | `AppController::run()` | 约 100ms | 输入事件 + 较重同步读取 |
| 传感器采样 | `AppController::run()` | 约 100ms | 周期性、相对较慢 |
| LCD 刷新 | `AppController::run()` | 约 100ms | 周期性、较重 |
| 串口日志 | 多处 `SYS_LOG` | 事件触发或调试周期 | 调试辅助 |

现有结构简单直接，但 `HAL_Delay(100)` 会让触摸和 UI 响应被主循环周期限制。后续若继续增加设置页、触摸控件、串口命令或更复杂的告警逻辑，建议改为非阻塞调度。

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

## 推荐路线

当前工程推荐采用混合策略：

1. 主框架使用 `tick + 标志表`
2. 触摸、串口等真正异步输入按需使用 `中断 + FIFO`
3. 所有重任务在主循环执行
4. 不采用纯中断事件驱动作为整体架构

建议任务周期：

| 周期 | 任务 |
| :--- | :--- |
| 10ms | LED 动画、按键扫描 |
| 20ms | 按键事件消费、轻量交互状态推进 |
| 50ms | 触摸检测或触摸事件消费 |
| 100ms | UI 刷新、应用状态机推进 |
| 500ms | 传感器采样 |
| 1000ms | 心跳日志、健康状态输出 |

## AppController 拆分建议

当前 `AppController::run()` 同时承担采样、交互、状态机和显示刷新。若要进入非阻塞调度，建议拆分为：

| 函数 | 建议周期 | 职责 |
| :--- | :--- | :--- |
| `sampleSensors()` | 500ms | 读取 AHT20/BMP280，更新遥测数据 |
| `processInputs()` | 20ms 或事件触发 | 消费按键和触摸事件 |
| `updateStateMachine()` | 100ms | 根据最新遥测和输入更新告警状态 |
| `refreshDisplay()` | 100ms 或按需 | 生成 `RenderData` 并刷新 LCD |
| `logHealth()` | 1000ms | 输出调试心跳和关键状态 |

这样可以保持 App 层逻辑清晰，同时让 BSP 层继续负责具体硬件行为。

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
- 不引入动态分配
- 队列容量固定，溢出要有计数或日志
- CubeMX 仍负责 GPIO/EXTI/NVIC 配置，BSP 和 App 不重新配置已经归 `.ioc` 管理的引脚

## 推荐演进顺序

1. 去掉 `main()` 中的 `HAL_Delay(100)`，改为主循环轮询任务标志
2. 将 `App_Timer_10ms_ISR()` 改成只推进时间、设置标志和调用必要的轻量扫描
3. 将 `AppController::run()` 拆为多个按周期调用的小步骤
4. 将触摸处理从 100ms 主循环轮询调整为 50ms 周期任务
5. 如触摸响应仍不够，再为 `TOUCH_PEN` 增加 EXTI 置位
6. 未来如增加串口命令或复杂 UI 控件，再引入固定容量事件 FIFO

该路线能先解决当前交互迟滞和阻塞问题，同时避免过早引入完整事件队列的复杂度。
