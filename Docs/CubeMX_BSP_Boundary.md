# CubeMX 与 BSP 边界

本文件定义当前工程中哪些内容由 CubeMX 维护，哪些内容由 BSP 和 App 手写维护。

相关文档：

- [文档导航](./README.md)
- [当前集成状态](./Current_Integration_Status.md)
- [调度架构方案对比与路线](./Scheduling_Architecture.md)

本项目采用“CubeMX 管配置、BSP 管行为”的边界约定。BSP 可以驱动引脚、访问设备，但不能再次配置已经在 `MicroCPProjectSTM32.ioc` 中声明过的 GPIO 模式、上下拉、速度、复用功能和外设时钟。

## 由 CubeMX 负责的配置

- LCD SPI 总线：`SPI1`，mode 0（`CPOL_LOW`、`CPHA_1EDGE`），分频 `/2`
- LCD 控制引脚：`PB6 LCD_LED`、`PB7 LCD_DC`、`PB8 LCD_RST`、`PB5 LCD_CS`，均为高速推挽输出
- 状态灯输出：`PB0 TIM3_CH3`，由 `PwmLedBsp` 作为 PWM 状态指示灯使用
- 触摸 bit-bang 引脚：`PA8 TOUCH_TCLK`、`PB3 TOUCH_TDIN`、`PB4 TOUCH_TCS`，均为高速推挽输出
- 触摸输入引脚：`PA0 TOUCH_PEN` 为上拉输入 + 上升沿 EXTI，`PA1 TOUCH_DOUT` 为上拉输入
- 物理按键输入：`PA2 KEY_PAGE`、`PA3 KEY_CONFIRM`、`PA4 KEY_BACK`，均为上拉输入
- `PA0/PA1` 在本工程中为触摸专用，不再绑定 `ButtonBsp`
- 传感器总线：`PB10/PB11` 为 `I2C2_SCL/I2C2_SDA`，启用硬件 I2C2 后不得再复用为软件 I2C GPIO
- 调试口：保持 SWD-only。由于 `PB3/PB4` 被触摸占用，必须关闭 JTAG，仅保留 `PA13/PA14`
- 系统节拍：`SysTick` 是当前 10ms 软件任务节拍来源，生成后必须保留 `SysTick_Handler()` 到 `HAL_SYSTICK_Callback()` 的回调链

## 由 BSP 负责的行为

- `LcdBsp` 负责 ST7796S 的复位时序、命令/数据写入、地址窗口和绘制原语
- `TouchBsp` 负责 XPT2046/ADS7846 的采样时序、滤波和校准计算
- BSP 的 `init()` 可以让设备进入默认空闲态或复位态，但不能再对 CubeMX 已拥有的引脚调用 `HAL_GPIO_Init()`

## 重新生成代码后的核对清单

重新生成 CubeMX 代码后，应确认：

- `MX_GPIO_Init()` 将 `TOUCH_PEN_Pin` 配置为 `GPIO_MODE_IT_RISING + GPIO_PULLUP`
- `MX_GPIO_Init()` 将 `TOUCH_DOUT_Pin` 配置为 `GPIO_PULLUP`
- `MX_GPIO_Init()` 将 `KEY_PAGE_Pin|KEY_CONFIRM_Pin|KEY_BACK_Pin` 配置为 `GPIO_PULLUP`
- `MX_GPIO_Init()` 将 LCD/触摸输出引脚配置为 `GPIO_SPEED_FREQ_HIGH`，且默认电平正确
- `MX_SPI1_Init()` 保持 `SPI_POLARITY_LOW`、`SPI_PHASE_1EDGE`、`SPI_BAUDRATEPRESCALER_2`
- `MX_TIM3_Init()` 仍保留 `TIM3_CH3` 输出，供 `PwmLedBsp` 控制状态灯
- `MX_GPIO_Init()` 或其用户代码段仍保留 `__HAL_AFIO_REMAP_SWJ_NOJTAG()`，确保 `PB3/PB4` 可用于触摸
- `EXTI0_IRQn` 已启用，供 `TOUCH_PEN` 使用
- `SysTick_Handler()` 仍调用 `HAL_SYSTICK_IRQHandler()`，否则 `HAL_SYSTICK_Callback()` 不会触发 `App_Timer_10ms_ISR()`
- BSP 文件没有重新配置已经由 `.ioc` 描述的 GPIO
