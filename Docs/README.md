# MicroCPProjectSTM32 文档导航

本目录只保留当前 `master` 分支维护所需的实现文档与协作规范。

当前事实基线以源码和 `MicroCPProjectSTM32.ioc` 为准。阅读顺序建议如下：

1. [当前状态](./Status.md)
2. [API 接口参考](./API.md)
3. [设计与开发规范](./Specification.md)

## 当前工程摘要

- 当前运行链路：`AppController` -> `Aht20Bsp` / `Bmp280Bsp` / `LcdBsp` / `TouchBsp`
- 当前传感器总线：`HardwareI2cBsp` + `I2C2` (`PB10` / `PB11`)
- 当前显示链路：`SPI1` + `LcdBsp` + `GuiEngine`
- 当前状态灯：`PB0 / TIM3_CH3`
- 当前输入模型：触摸屏 + 3 个物理按键
- 当前调度模型：`SysTick` 1ms 时基 + 10ms 应用层任务标志
- 当前 FPGA 维护范围：LCD 控制链路、状态灯链路、底板按键回送链路

## 文档清单

| 文档 | 作用 | 适合谁看 |
| :--- | :--- | :--- |
| [Status.md](./Status.md) | 当前可运行工程的硬件映射、FPGA 透传/回送范围、调度节奏、日志状态和系统核对项。 | 所有维护者 |
| [API.md](./API.md) | 当前抽象接口、关键实现类、对象装配和显示/输入/采样数据流。 | 编码人员 |
| [Specification.md](./Specification.md) | 架构规范、CubeMX/BSP/App 边界、调度约束和文档维护规则。 | 新加入项目的开发者 |

## 当前硬件摘要

### 显示与触摸

| 功能 | 引脚 | 说明 |
| :--- | :--- | :--- |
| `SPI1_SCK` | `PA5` | LCD SPI 时钟 |
| `SPI1_MISO` | `PA6` | LCD 当前不读回数据 |
| `SPI1_MOSI` | `PA7` | LCD SPI 数据输出 |
| `LCD_CS` | `PB5` | LCD 片选 |
| `LCD_LED` | `PB6` | 背光控制 |
| `LCD_DC` | `PB7` | 数据/命令选择 |
| `LCD_RST` | `PB8` | LCD 复位 |
| `TOUCH_PEN` | `PA0` | 触摸按下检测 / 释放中断输入 |
| `TOUCH_DOUT` | `PA1` | 触摸串行输出 |
| `TOUCH_TCLK` | `PA8` | 触摸时钟 |
| `TOUCH_TDIN` | `PB3` | 触摸串行输入 |
| `TOUCH_TCS` | `PB4` | 触摸片选 |

### 其他外设

| 功能 | 引脚 | 说明 |
| :--- | :--- | :--- |
| `I2C2_SCL` | `PB10` | AHT20 / BMP280 共用硬件 I2C |
| `I2C2_SDA` | `PB11` | AHT20 / BMP280 共用硬件 I2C |
| `TIM3_CH3` | `PB0` | PWM 状态灯输出 |
| `KEY_PAGE` | `PA2` | 物理按键输入，对应底板 `S0` 经 FPGA 回送 |
| `KEY_CONFIRM` | `PA3` | 物理按键输入，对应底板 `S2` 经 FPGA 回送 |
| `KEY_BACK` | `PA4` | 物理按键输入，对应底板 `S3` 经 FPGA 回送 |
| `USART1_TX/RX` | `PA9` / `PA10` | 调试日志串口 |

## 阅读约定

- 需要核对“当前工程到底怎么接、怎么跑”，先看 [Status.md](./Status.md)
- 需要修改 `.ioc`、GPIO、DMA、NVIC 或外设初始化，先看 [Specification.md](./Specification.md)
- 需要接驱动、改 `AppController`、补显示/输入逻辑，先看 [API.md](./API.md)
- 需要理解架构约束、CubeMX/BSP/App 边界和文档维护规则，先看 [Specification.md](./Specification.md)

研究型文档和实验资料不再保留在 `master/Docs`。`test_LCD` 分支继续保留 LCD/GUI/FPGA 方向的试验文档快照。
