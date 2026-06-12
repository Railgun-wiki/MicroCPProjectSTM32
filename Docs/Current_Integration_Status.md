# 当前集成状态

基线文档：本文件描述当前可运行工程的真实状态。与研究方案或历史设计冲突时，以本文件和源码为准。

相关文档：

- [文档导航](./README.md)
- [CubeMX 与 BSP 边界](./CubeMX_BSP_Boundary.md)
- [API 接口参考](./API.md)
- [调度架构方案对比与路线](./Scheduling_Architecture.md)

本文档用于记录 LCD、触摸与 I2C2 清理后的当前工程真实状态，作为后续维护时的快速核对基准。

## 当前硬件映射

- `SPI1` 驱动 LCD，工作模式为 mode 0：`CPOL_LOW`、`CPHA_1EDGE`，分频 `/2`
- LCD 控制引脚：
  - `PB6` -> `LCD_LED`
  - `PB7` -> `LCD_DC`
  - `PB8` -> `LCD_RST`
  - `PB5` -> `LCD_CS`
- 状态灯引脚：
  - `PB0` -> `TIM3_CH3` -> 状态指示 LED（正常呼吸 / 异常闪烁）
- 触摸引脚：
  - `PA0` -> `TOUCH_PEN`
  - `PA1` -> `TOUCH_DOUT`
  - `PA8` -> `TOUCH_TCLK`
  - `PB3` -> `TOUCH_TDIN`
  - `PB4` -> `TOUCH_TCS`
- 物理按键引脚：
  - `PA2` -> `KEY_PAGE`（底板 `S0`）
  - `PA3` -> `KEY_CONFIRM`（底板 `S2`）
  - `PA4` -> `KEY_BACK`（底板 `S3`）
- 传感器总线：
  - `PB10` -> `I2C2_SCL`
  - `PB11` -> `I2C2_SDA`

## 职责边界

- CubeMX 负责引脚模式、上下拉、速度、默认电平、复用功能、DMA 和外设时钟配置
- `LcdBsp` 负责 LCD 复位时序、命令/数据传输与绘制原语
- `TouchBsp` 负责触摸采样、滤波与校准计算
- `HardwareI2cBsp` 是当前默认传感器总线实现，基于 `HAL_I2C_*`。目前在 `app_entry.cpp` 中会在启动时执行 I2C 地址扫描并将诊断结果输出到 LCD。
- `SoftI2cBsp` 仍保留在仓库中，作为备选实现（SDA 引脚已优化为推挽/输入动态切换模式以提升稳定性），但不是当前运行路径。

## 输入模型

- `PA0/PA1` 为触摸专用引脚，不得再绑定到 `ButtonBsp`
- 当前工程注入了 3 个真实物理按键：
  - `KEY_PAGE`：切页
  - `KEY_CONFIRM`：确认/抑制当前告警展示
  - `KEY_BACK`：返回默认页并恢复告警展示
- 触摸输入仍保留：点击右半屏可切页

## 当前调度模型

- `SysTick_Handler()` 调用 `HAL_IncTick()` 和 `HAL_SYSTICK_IRQHandler()`，保持 HAL 1ms 时基有效
- `HAL_SYSTICK_Callback()` 每 10ms 触发 `App_Timer_10ms_ISR()`，应用层 tick 只设置任务标志
- `main()` 主循环持续调用 `App_Loop()`，不再用 `HAL_Delay(100)` 节流
- `App_Loop()` 在主循环上下文消费任务标志，执行 LED、按键、触摸、传感器、状态机、LCD 和健康日志任务
- AHT20 采样使用协作式状态机等待转换完成，运行期不再用 `HAL_Delay(80)` 阻塞系统
- 当前调度实施细节见 [Scheduling_Architecture.md](./Scheduling_Architecture.md)

## 调试与重映射要求

- `PB3/PB4` 被触摸占用，因此调试口必须保持 SWD-only
- `MicroCPProjectSTM32.ioc` 中保留 `SYS.Debug=SYS_DEBUG_SERIAL_WIRE`
- `MX_GPIO_Init()` 用户代码段中保留 `__HAL_AFIO_REMAP_SWJ_NOJTAG()`

## CubeMX 重新生成后的核对项

重新生成代码后，至少应满足：

- `HAL_DMA_MODULE_ENABLED` 已开启
- `SPI1` 的 RX/TX DMA 仍连接 `DMA1_Channel2` / `DMA1_Channel3`
- `TOUCH_PEN`、`TOUCH_DOUT`、`KEY_PAGE`、`KEY_CONFIRM`、`KEY_BACK` 仍为上拉输入
- LCD/触摸输出引脚仍为高速输出，且默认电平符合当前设计
- `PB0` 仍保持 `TIM3_CH3` 供 `PwmLedBsp` 使用
- `SysTick_Handler()` 仍调用 `HAL_IncTick()` 和 `HAL_SYSTICK_IRQHandler()`，确保 HAL 时基和 10ms 周期任务链路有效
