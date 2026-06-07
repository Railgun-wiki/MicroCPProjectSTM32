# MicroCPProjectSTM32 文档导航

本目录维护当前 STM32 固件工程的实现说明、接口文档、硬件边界和设计研究资料。

当前工程的事实基线以源码和 `MicroCPProjectSTM32.ioc` 为准。阅读文档时，建议优先按以下顺序：

1. [当前集成状态](./Current_Integration_Status.md)
2. [CubeMX 与 BSP 边界](./CubeMX_BSP_Boundary.md)
3. [API 接口参考](./API.md)
4. [设计与开发规范](./Specification.md)

## 当前工程摘要

- 当前运行链路：`AppController` -> `Aht20Bsp` / `Bmp280Bsp` / `LcdBsp` / `TouchBsp`
- 当前传感器总线：`HardwareI2cBsp` + `I2C2` (`PB10` / `PB11`)
- 当前显示链路：`SPI1` + `LcdBsp` + `GuiEngine`
- 当前输入模型：触摸屏为主输入；`PA0` / `PA1` 已用于触摸，不绑定物理 `ButtonBsp`
- 当前按钮注入：`app_entry.cpp` 中使用 `NullButton` 满足 `AppController` 的抽象依赖

## 文档清单

| 文档 | 作用 | 适合谁看 |
| :--- | :--- | :--- |
| [Current_Integration_Status.md](./Current_Integration_Status.md) | 当前可运行工程的硬件映射、输入模型、DMA/SWD 约束。 | 所有维护者 |
| [CubeMX_BSP_Boundary.md](./CubeMX_BSP_Boundary.md) | 约束哪些内容归 CubeMX，哪些内容归 BSP 和 App。 | 维护硬件初始化和驱动的开发者 |
| [API.md](./API.md) | 当前抽象接口、驱动实现、关键数据结构和对象装配方式。 | 编码人员 |
| [Specification.md](./Specification.md) | 目录职责、依赖倒置、编码和构建规范。 | 新加入项目的开发者 |
| [FPGA_LCD_Setup.md](./FPGA_LCD_Setup.md) | FPGA 透传接线与约束说明。非当前 MCU 代码实现文档。 | 联调硬件人员 |
| [FPGA_LCD_Logic_Expansion.md](./FPGA_LCD_Logic_Expansion.md) | FPGA 逻辑扩展方向研究。 | 架构预研 |
| [Lightweight_GUI_Frameworks.md](./Lightweight_GUI_Frameworks.md) | 轻量 GUI 方案调研。 | GUI 预研 |
| [Touch_GUI_Architecture.md](./Touch_GUI_Architecture.md) | 触摸 GUI 方向方案分析。 | GUI/交互预研 |

## 当前硬件映射

### 显示与触摸

| 功能 | 引脚 | 说明 |
| :--- | :--- | :--- |
| `SPI1_SCK` | `PA5` | LCD SPI 时钟 |
| `SPI1_MISO` | `PA6` | LCD 当前不读回数据，MISO 可保留 |
| `SPI1_MOSI` | `PA7` | LCD SPI 数据输出 |
| `LCD_LED` | `PB6` | 背光控制 |
| `LCD_DC` | `PB7` | 数据/命令选择 |
| `LCD_RST` | `PB8` | LCD 复位 |
| `LCD_CS` | `PB9` | LCD 片选 |
| `TOUCH_PEN` | `PA0` | 触摸按下检测 |
| `TOUCH_DOUT` | `PA1` | 触摸串行输出 |
| `TOUCH_TCLK` | `PA8` | 触摸时钟 |
| `TOUCH_TDIN` | `PB3` | 触摸串行输入 |
| `TOUCH_TCS` | `PB4` | 触摸片选 |

### 传感器与其他外设

| 功能 | 引脚 | 说明 |
| :--- | :--- | :--- |
| `I2C2_SCL` | `PB10` | AHT20 / BMP280 共用硬件 I2C |
| `I2C2_SDA` | `PB11` | AHT20 / BMP280 共用硬件 I2C |
| `TIM3_CH3` | `PB0` | `PwmLedBsp` 使用的 PWM 指示灯输出 |
| `USART1_TX/RX` | `PA9` / `PA10` | 调试日志串口 |

## 当前软件结构

```mermaid
graph TD
    AppInit["App_Init()"]
    AppLoop["App_Loop()"]
    TimerISR["App_Timer_10ms_ISR()"]

    AppInit --> I2C["HardwareI2cBsp"]
    AppInit --> LCD["LcdBsp"]
    AppInit --> Touch["TouchBsp"]
    AppInit --> AppCtl["AppController"]

    AppCtl --> TempHum["Aht20Bsp"]
    AppCtl --> Pressure["Bmp280Bsp"]
    AppCtl --> Indicator["PwmLedBsp"]
    AppCtl --> PageBtn["NullButton : IButton"]
    AppCtl --> MuteBtn["NullButton : IButton"]
    AppCtl --> Display["ILcdDisplay / LcdBsp"]
    AppCtl --> TouchIf["ITouch / TouchBsp"]

    AppLoop --> AppCtl
    TimerISR --> Indicator
    TimerISR --> PageBtn
    TimerISR --> MuteBtn
```

## 构建方式

工程当前使用 CMake Presets 和 `cmake/gcc-arm-none-eabi.cmake`。

```bash
cmake --preset Debug
cmake --build --preset Debug
```

如需发布构建：

```bash
cmake --preset Release
cmake --build --preset Release
```

默认输出目录为 `build/Debug` 或 `build/Release`，产物包含 `MicroCPProjectSTM32.elf` 以及 CubeMX 规则生成的镜像文件。

## 阅读约定

- 需要核对“当前工程到底怎么接、怎么跑”，先看 [Current_Integration_Status.md](./Current_Integration_Status.md)
- 需要修改 `.ioc`、GPIO、DMA 或外设初始化，先看 [CubeMX_BSP_Boundary.md](./CubeMX_BSP_Boundary.md)
- 需要接驱动或改 `AppController` 依赖，先看 [API.md](./API.md)
- 研究文档均不是当前实现说明；它们保留为备选方案或历史分析，使用前必须先对照当前状态文档
