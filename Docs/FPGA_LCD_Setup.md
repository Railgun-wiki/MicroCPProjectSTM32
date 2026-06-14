# FPGA LCD 透传配置说明

> 配置说明文档，不是当前 MCU 固件行为的完整说明。
>
> 本文只覆盖 FPGA 透传接线与约束。若需确认 STM32 固件当前如何使用 LCD、触摸和 I2C2，请先参考 [Current_Integration_Status.md](./Current_Integration_Status.md)。

## 概述

开发板上的 FPGA 作为 STM32 ARM 芯片与 LCD 底板连接器之间的**纯信号透传**模块。FPGA 内部没有任何逻辑、缓存或信号变换 -- ARM 侧的每个信号都直接连线到对应的 LCD 侧引脚。

由于 LCD 物理连接器的走线经过 FPGA 的 I/O 引脚，因此必须配置 FPGA 透传才能使 STM32 的 SPI 控制信号到达 LCD 模组。

## 信号映射

| 信号 | 方向 | ARM 侧 FPGA 引脚 | LCD 侧 FPGA 引脚 | 说明 |
|------|------|-------------------|-------------------|------|
| LED  | ARM -> LCD | AB19 | P22 | 背光控制 |
| DC   | ARM -> LCD | AB20 | M21 | 数据/命令选择 |
| CS   | ARM -> LCD | Y21  | N19 | 片选（低电平有效） |
| RST  | ARM -> LCD | Y18  | N20 | 硬件复位（低电平有效） |
| SCK  | ARM -> LCD | W22  | N22 | SPI 时钟 |
| SDI  | ARM -> LCD | V19  | M22 | SPI MOSI（ARM 向 LCD 写数据） |

所有信号均为单向传输（ARM -> LCD）。LCD 的 MISO（SDO）线未连接 -- 本项目只向 LCD 写入数据，不读取。

## Verilog 模块

透传模块由一组直接连线赋值构成：

```verilog
module LCD(
    input  LED_ARM,
    input  DC_ARM,
    input  CS_ARM,
    input  RST_ARM,
    input  SCK_ARM,
    input  SDI_ARM,

    output LED_BASE,
    output DC_BASE,
    output CS_BASE,
    output RST_BASE,
    output SCK_BASE,
    output SDI_BASE
);

assign LED_BASE = LED_ARM;
assign DC_BASE  = DC_ARM;
assign CS_BASE  = CS_ARM;
assign RST_BASE = RST_ARM;
assign SCK_BASE = SCK_ARM;
assign SDI_BASE = SDI_ARM;

endmodule
```

## XDC 约束文件

所有引脚均使用 LVCMOS33 I/O 电平标准：

```tcl
# I/O 电平标准
set_property IOSTANDARD LVCMOS33 [get_ports LED_ARM]
set_property IOSTANDARD LVCMOS33 [get_ports DC_ARM]
set_property IOSTANDARD LVCMOS33 [get_ports CS_ARM]
set_property IOSTANDARD LVCMOS33 [get_ports RST_ARM]
set_property IOSTANDARD LVCMOS33 [get_ports SCK_ARM]
set_property IOSTANDARD LVCMOS33 [get_ports SDI_ARM]
set_property IOSTANDARD LVCMOS33 [get_ports LED_BASE]
set_property IOSTANDARD LVCMOS33 [get_ports DC_BASE]
set_property IOSTANDARD LVCMOS33 [get_ports CS_BASE]
set_property IOSTANDARD LVCMOS33 [get_ports RST_BASE]
set_property IOSTANDARD LVCMOS33 [get_ports SCK_BASE]
set_property IOSTANDARD LVCMOS33 [get_ports SDI_BASE]

# ARM 侧引脚绑定
set_property PACKAGE_PIN AB19 [get_ports LED_ARM]
set_property PACKAGE_PIN AB20 [get_ports DC_ARM]
set_property PACKAGE_PIN Y21  [get_ports CS_ARM]
set_property PACKAGE_PIN Y18  [get_ports RST_ARM]
set_property PACKAGE_PIN W22  [get_ports SCK_ARM]
set_property PACKAGE_PIN V19  [get_ports SDI_ARM]

# LCD 侧引脚绑定
set_property PACKAGE_PIN P22  [get_ports LED_BASE]
set_property PACKAGE_PIN M21  [get_ports DC_BASE]
set_property PACKAGE_PIN N19  [get_ports CS_BASE]
set_property PACKAGE_PIN N20  [get_ports RST_BASE]
set_property PACKAGE_PIN N22  [get_ports SCK_BASE]
set_property PACKAGE_PIN M22  [get_ports SDI_BASE]
```

## Vivado 构建步骤

1. 新建 Vivado 工程，选择目标 FPGA 器件
2. 添加 Verilog 源文件（`LCD.v`）
3. 添加 XDC 约束文件（`LCD.xdc`）
4. 依次执行：综合 -> 实现 -> 生成比特流
5. 下载比特流到 FPGA

## STM32 侧引脚映射

当前工程优先按 `LAB16_SPI` 例程直连 LCD 模块调试，不使用本节 FPGA 透传接线。生效的 LCD 接线如下：

| STM32 引脚 | 功能 |
|------------|------|
| PA5 | SPI1_SCK |
| PA6 | SPI1_MISO（可保留，LCD 不读取也可不接） |
| PA7 | SPI1_MOSI |
| PB5 | LCD_CS |
| PB7 | LCD_DC/RS |
| PB8 | LCD_RST |
| PB6 | LCD_LED（背光） |

注意：STM32 的 SPI 引脚（PA5/PA7）由 CubeMX 配置为复用推挽输出。GPIO 控制引脚（PB5/PB7/PB8/PB6）由 `LcdBsp::init()` 方法初始化。

## SPI 配置

STM32 的 SPI1 配置参数：
- 主模式，8 位数据宽度
- CPOL = Low，CPHA = 1 Edge（SPI Mode 0）
- 软件 NSS
- 波特率分频：/2（72 MHz APB2 时钟下为 36 MHz）
- MSB 先发送

当前工程与参考例程一致，使用分频 /2（36 MHz）。
