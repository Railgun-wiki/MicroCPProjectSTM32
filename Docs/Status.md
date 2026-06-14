# 当前状态

基线文档：本文件描述当前 `master` 分支可运行工程的真实状态。与历史方案或研究资料冲突时，以本文件和源码为准。

相关文档：

- [文档导航](./README.md)
- [API 接口参考](./API.md)
- [设计与开发规范](./Specification.md)

## 当前硬件映射

- `SPI1` 驱动 LCD，工作模式为 mode 0：`CPOL_LOW`、`CPHA_1EDGE`，分频 `/2`
- LCD 控制引脚：
  - `PB5` -> `LCD_CS`
  - `PB6` -> `LCD_LED`
  - `PB7` -> `LCD_DC`
  - `PB8` -> `LCD_RST`
- 状态灯引脚：
  - `PB0` -> `TIM3_CH3` -> 状态指示 LED（正常呼吸 / 告警闪烁）
- 触摸引脚：
  - `PA0` -> `TOUCH_PEN`
  - `PA1` -> `TOUCH_DOUT`
  - `PA8` -> `TOUCH_TCLK`
  - `PB3` -> `TOUCH_TDIN`
  - `PB4` -> `TOUCH_TCS`
- 物理按键引脚：
  - `PA2` -> `KEY_PAGE`（底板 `S0` 经 FPGA 回送）
  - `PA3` -> `KEY_CONFIRM`（底板 `S2` 经 FPGA 回送）
  - `PA4` -> `KEY_BACK`（底板 `S3` 经 FPGA 回送）
- 传感器总线：
  - `PB10` -> `I2C2_SCL`
  - `PB11` -> `I2C2_SDA`

## FPGA 透传与约束维护

- 当前工程中，凡是 MCU 与外设/底板之间经过 FPGA 的信号，都在本文件维护摘要和核对要求。
- 当前已知需要按 FPGA 透传/回送视角维护的信号范围包括：
  - LCD 控制链路：`LCD_CS`、`LCD_LED`、`LCD_DC`、`LCD_RST` 以及 LCD SPI 相关信号
  - 状态灯链路：`PB0 / TIM3_CH3` 对应的状态灯输出路径
  - 底板按键回送链路：`S0 -> KEY_PAGE`、`S2 -> KEY_CONFIRM`、`S3 -> KEY_BACK`
- 当前 MCU 固件侧事实：
  - `PB5/PB6/PB7/PB8` 对应 LCD 控制
  - `PA2/PA3/PA4` 为 MCU 当前收到的按键输入，来源于底板按键经 FPGA 回送
  - `PB0 TIM3_CH3` 为当前状态灯输出链路，若 FPGA 透传约束变更，需与 LCD/按键链路一并核对
- `master` 只保留 MCU 固件维护所需的 FPGA 约束摘要，不保留 Verilog/XDC 长篇实现文档。
- 若后续重新验证或修改 FPGA 透传/回送约束，请在 `test_LCD` 分支维护对应研究或配置文档，并同步本文件中的摘要结论。

## 职责边界摘要

- CubeMX 负责引脚模式、上下拉、速度、复用功能、DMA、NVIC、外设时钟和句柄初始化。
- `LcdBsp` 负责 LCD 复位时序、命令/数据传输、字符绘制和调试页刷新。
- `GuiEngine` 提供几何绘图原语，不直接承担业务状态机。
- `TouchBsp` 负责触摸采样、滤波和校准映射。
- `HardwareI2cBsp` 是当前默认传感器总线实现，启动时会在 `app_entry.cpp` 中执行 I2C 地址扫描并输出结果。
- `SoftI2cBsp` 仍保留在仓库中作为备选实现，但不是当前运行路径。

## 输入模型

- `PA0/PA1` 为触摸专用引脚，不得再绑定到 `ButtonBsp`。
- 当前工程注入了 3 个真实物理按键：
  - `KEY_PAGE`：切页
  - `KEY_CONFIRM`：确认/抑制当前告警展示
  - `KEY_BACK`：返回默认页并恢复告警展示
- 触摸输入仍保留：任意一次有效触摸释放事件均可切页。
- `TOUCH_PEN` 使用上升沿 EXTI 只置待处理标志，触摸切页动作在主循环下一个任务周期完成。

## 当前调度模型

- `SysTick_Handler()` 调用 `HAL_IncTick()` 和 `HAL_SYSTICK_IRQHandler()`，保持 HAL 1ms 时基有效。
- `HAL_SYSTICK_Callback()` 每 10ms 触发 `App_Timer_10ms_ISR()`，应用层 tick 只设置任务标志。
- `main()` 主循环持续调用 `App_Loop()`，不再通过 `HAL_Delay(100)` 节流。
- `App_Loop()` 在非中断上下文消费任务标志，执行 LED、按键、触摸、传感器、状态机、LCD 和健康日志任务。
- 当前任务节奏：
  - `10ms`：LED 动画、按键扫描、AHT20 协作式推进
  - `20ms`：输入事件消费
  - `50ms`：触摸兜底轮询
  - `100ms`：状态机更新、LCD 刷新
  - `500ms`：启动新一轮传感器采样
  - `1000ms`：健康日志输出
- 触摸 EXTI 回调只设置待处理标志，并在下一个 10ms tick 转换为触摸事件任务。
- 50ms 触摸轮询保留为兜底路径，用于漏中断时补做按下/释放状态观测。
- AHT20 运行期采样使用协作式状态机等待转换完成，不再用 `HAL_Delay(80)` 阻塞系统。

## 日志与可观测性

- `SYSTEM/sys.hpp` 提供 `SYS_LOG(...)` 调试日志宏，当前通过 `USART1` 串口输出。
- 启动阶段会输出：
  - C++ 应用入口启动
  - 触摸 BSP 初始化结果
  - I2C 启动扫描汇总（组号、AHT20/BMP280 是否发现）
- 运行期会输出低频边沿日志：
  - 触摸按下、IRQ 释放事件、polling fallback 释放
  - AHT20 / BMP280 通信失败与恢复
  - 告警切换、页面切换、健康状态摘要
- ISR 内不输出日志。

## 调试与重映射要求

- `PB3/PB4` 被触摸占用，因此调试口必须保持 SWD-only。
- `MicroCPProjectSTM32.ioc` 中保留 `SYS.Debug=SYS_DEBUG_SERIAL_WIRE`。
- `MX_GPIO_Init()` 用户代码段中保留 `__HAL_AFIO_REMAP_SWJ_NOJTAG()`。

## CubeMX 重新生成后的系统核对项

重新生成代码后，至少应满足：

- `HAL_DMA_MODULE_ENABLED` 已开启。
- `SPI1` 的 RX/TX DMA 仍连接 `DMA1_Channel2` / `DMA1_Channel3`。
- `TOUCH_PEN` 保持上拉输入并启用上升沿 EXTI；`TOUCH_DOUT`、`KEY_PAGE`、`KEY_CONFIRM`、`KEY_BACK` 仍为上拉输入。
- LCD/触摸输出引脚仍为高速输出，且默认电平符合当前设计。
- `PB0` 仍保持 `TIM3_CH3` 供 `PwmLedBsp` 使用。
- `PA2/PA3/PA4` 的按键输入链路仍与底板按键和 FPGA 回送设计一致。
- `SysTick_Handler()` 仍调用 `HAL_IncTick()` 和 `HAL_SYSTICK_IRQHandler()`，确保 HAL 时基和 10ms 周期任务链路有效。
