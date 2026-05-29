# FPGA 逻辑扩展设计思路

## 当前架构（纯透传）

```
STM32 --[SPI 18MHz]--> FPGA (直连) --[SPI]--> ST7796S LCD
```

FPGA 只做信号电平转换和引脚路由，所有逻辑（初始化、像素写入、绘图）均由 STM32 完成。

---

## 方向一：帧缓冲 + 自动刷新

### 思路

FPGA 内部维护一块显存（Framebuffer），STM32 只需向 FPGA 写入像素数据，FPGA 负责以固定时序持续刷新 LCD。

```
STM32 --[写像素]--> FPGA 显存 --[自动刷新]--> LCD
```

### 架构

```
┌─────────┐   SPI/自定义协议   ┌──────────────────────┐   SPI Master   ┌─────────┐
│  STM32  │ ─────────────────> │  FPGA                │ ─────────────> │  LCD    │
│         │                    │  ┌──────────────┐    │               │ ST7796S │
│         │                    │  │ Block RAM     │    │               │         │
│         │                    │  │ 480×320×16bit │    │               │         │
│         │                    │  │ (300KB)       │    │               │         │
│         │                    │  └──────┬───────┘    │               │         │
│         │                    │         │ 读取        │               │         │
│         │                    │  ┌──────▼───────┐    │               │         │
│         │                    │  │ LCD 控制状态机 │───>│               │         │
│         │                    │  └──────────────┘    │               │         │
└─────────┘                    └──────────────────────┘               └─────────┘
```

### FPGA 内部模块

| 模块 | 功能 |
|------|------|
| SPI Slave | 接收 STM32 的像素写入命令和数据 |
| 命令解析器 | 解析地址+颜色数据，写入显存 |
| Block RAM | 480×320×16bit = 307,200 字节（FPGA BRAM 容量需足够） |
| LCD 刷新状态机 | 按 ST7796S 时序循环读取显存，生成 SPI 写入序列 |
| SPI Master | 向 LCD 发送像素数据 |

### 通信协议（STM32 -> FPGA）

```
[CMD_WRITE_PIXEL] [X_H] [X_L] [Y_H] [Y_L] [COLOR_H] [COLOR_L]
[CMD_FILL_RECT]   [X0_H][X0_L][Y0_H][Y0_L][X1_H][X1_L][Y1_H][Y1_L][COLOR_H][COLOR_L]
[CMD_CLEAR]       [COLOR_H] [COLOR_L]
```

### 优缺点

- ✅ STM32 写入后即可做其他事，LCD 由 FPGA 自动刷新
- ✅ 消除 STM32 的 SPI 阻塞等待
- ✅ 可实现双缓冲（前台/后台显存切换）
- ❌ 需要大容量 BRAM（480×320×16bit ≈ 300KB，超出多数低成本 FPGA）
- ❌ 刷新逻辑较复杂

### BRAM 不足时的折中

- 使用外部 SRAM/SDRAM 作为显存
- 降低色深（如 RGB332 = 8bit，只需 150KB）
- 使用部分刷新（只更新变化区域）

---

## 方向二：硬件图形加速

### 思路

FPGA 内部实现绘图硬件原语（画线、填矩形、画圆、字符渲染），STM32 只需发送高层绘图命令，FPGA 执行底层像素操作。

```
STM32: "在(10,10)到(100,50)画红色矩形"
  ↓ 发送命令
FPGA: 执行像素级填充循环
  ↓ 写入
LCD: 显示结果
```

### 硬件绘图指令集

| 指令 | 参数 | 说明 |
|------|------|------|
| `DRAW_PIXEL` | x, y, color | 画单点 |
| `DRAW_HLINE` | x0, x1, y, color | 水平线（单次 setAddressWindow + 循环写入） |
| `DRAW_VLINE` | x, y0, y1, color | 垂直线 |
| `FILL_RECT` | x0, y0, x1, y1, color | 矩形填充 |
| `DRAW_LINE` | x0, y0, x1, y1, color | Bresenham 画线（硬件状态机实现） |
| `DRAW_CIRCLE` | xc, yc, r, color, fill | 中点画圆 |
| `DRAW_CHAR` | x, y, code, fc, bc, size | 字符渲染（FPGA 内置字模 ROM） |
| `DRAW_STRING` | x, y, *str, fc, bc, size | 字符串（多字符的 DRAW_CHAR 序列） |

### FPGA 模块架构

```
┌─────────────────────────────────────────┐
│  FPGA                                   │
│                                         │
│  ┌──────────┐    ┌─────────────────┐    │
│  │ SPI Slave │───>│ 命令 FIFO       │    │
│  │ (from STM32)│  └────────┬────────┘    │
│  └──────────┘           │              │
│                    ┌────▼─────┐         │
│                    │ 绘图引擎  │         │
│                    │ FSM       │         │
│                    └────┬─────┘         │
│                         │              │
│              ┌──────────┼──────────┐   │
│              ▼          ▼          ▼   │
│         ┌────────┐ ┌────────┐ ┌─────┐ │
│         │Bresenham│ │矩形填充│ │字模  │ │
│         │画线 FSM │ │ FSM    │ │ROM  │ │
│         └────────┘ └────────┘ └─────┘ │
│                         │              │
│                    ┌────▼─────┐         │
│                    │ SPI Master│──> LCD │
│                    └──────────┘         │
└─────────────────────────────────────────┘
```

### Bresenham 画线硬件实现要点

```verilog
// 画线状态机伪代码
module draw_line (
    input  clk,
    input  start,
    input  [15:0] x0, y0, x1, y1, color,
    output busy,
    // LCD SPI 接口
    output reg lcd_cs, lcd_dc, lcd_sck, lcd_sdi
);
    reg [15:0] x_err, y_err, distance;
    reg [15:0] u_row, u_col;
    reg [1:0]  state;

    localparam IDLE    = 0;
    localparam CALC    = 1;  // 计算 delta/inc
    localparam DRAW    = 2;  // 逐像素绘制
    localparam DONE    = 3;

    always @(posedge clk) begin
        case (state)
            DRAW: begin
                // 输出当前像素 (u_row, u_col, color) 到 SPI
                // 更新 x_err, y_err
                // 判断是否到达终点
                if (pixels_remaining == 0)
                    state <= DONE;
            end
        end
    end
endmodule
```

### 字模 ROM

将 `LcdFont.hpp` 中的 `kAsc2_1608` 数组烧入 FPGA 的 Block ROM：

```verilog
// 95 个 ASCII 字符 × 16 字节 = 1520 字节
reg [7:0] font_rom [0:1519];
initial $readmemh("font_16x8.hex", font_rom);
```

### 优缺点

- ✅ 大幅降低 STM32 的 CPU 负载（发送几字节命令 vs 发送数千字节像素）
- ✅ 画线/填充速度由 FPGA 时钟决定（100MHz 级别），远快于 STM32 的 18MHz SPI
- ✅ STM32 代码简化为纯业务逻辑
- ❌ FPGA 逻辑开发复杂度高
- ❌ 仍需通过 SPI 逐像素写入 LCD（除非配合方向一的帧缓冲）

---

## 方向三：SPI 桥接 / 速率转换

### 思路

FPGA 作为 SPI 中间层，接收 STM32 的数据后以更高速率或不同协议转发给 LCD。

### 模式 A：速率提升

```
STM32 --[SPI 18MHz]--> FPGA --[SPI 36MHz 或更高]--> LCD
```

FPGA 内部用 FIFO 缓冲数据，输入侧以 18MHz 接收，输出侧以更高速率发送。

```verilog
module spi_bridge (
    // STM32 侧 (Slave)
    input  sck_arm,
    input  mosi_arm,
    input  cs_arm,
    // LCD 侧 (Master)
    output reg sck_lcd,
    output reg mosi_lcd,
    output reg cs_lcd
);
    // 异步 FIFO 做时钟域转换
    // 写入侧: sck_arm 域
    // 读取侧: 高速内部时钟域 (如 100MHz)
endmodule
```

### 模式 B：并行转串行

STM32 通过 8 位并行端口（GPIO）发送数据，FPGA 接收后转为 SPI 发送给 LCD。并行写入比 SPI 快得多。

```
STM32 --[8bit 并行 GPIO]--> FPGA --[SPI]--> LCD
```

STM32 侧只需一次 GPIO 写操作（~2 个时钟周期）就能传 1 字节，比 SPI 的 8 个时钟周期快 4 倍。

### 模式 C：命令队列

STM32 将多条 SPI 命令批量写入 FPGA 的 FIFO，FPGA 依次执行，STM32 无需逐条等待完成。

```
STM32: 写入 100 条命令到 FPGA FIFO
STM32: 继续执行其他任务
FPGA:  依次执行 FIFO 中的命令，驱动 LCD
```

### 优缺点

- ✅ 降低 STM32 的 SPI 等待时间
- ✅ 可透明集成，STM32 代码改动小
- ❌ 收益相对有限（瓶颈可能在 LCD 本身而非 SPI 速率）
- ❌ 增加信号延迟（FPGA 转发延迟约 1-2 个时钟周期）

---

## 方向四：其他扩展思路

### 4.1 触摸屏控制器替代

用 FPGA 实现电阻触摸屏的 SPI 读取（替代 `touch.c` 中的 bit-bang），释放 STM32 的 GPIO 和 CPU 时间。

### 4.2 DMA 引擎

FPGA 实现一个 DMA 引擎，直接从 STM32 的内存读取像素数据（通过 FSMC 并行接口），无需 CPU 干预即可完成全屏刷新。

```
STM32 FSMC --[并行]--> FPGA DMA --> LCD SPI
```

### 4.3 多外设仲裁

FPGA 作为 SPI 总线仲裁器，让 STM32 通过一组 SPI 引脚同时控制 LCD、触摸屏、Flash 等多个 SPI 设备。

```
STM32 --[SPI]--> FPGA --[SPI CS0]--> LCD
                  │
                  ├──[SPI CS1]--> Touch
                  └──[SPI CS2]--> Flash
```

### 4.4 图层叠加

FPGA 内部实现两层图层（前景 + 背景），支持透明度混合。STM32 分别更新两个图层，FPGA 混合后输出到 LCD。

---

## 各方向对比

| 方向 | STM32 负载降低 | FPGA 复杂度 | 对 STM32 代码改动 | 适用场景 |
|------|---------------|-------------|-------------------|---------|
| 帧缓冲 + 自动刷新 | ★★★★★ | ★★★★☆ | 小（改写入方式） | 需要高帧率动画 |
| 硬件图形加速 | ★★★★☆ | ★★★★★ | 中（改绘图调用） | 频繁绘制复杂图形 |
| SPI 桥接/加速 | ★★☆☆☆ | ★★☆☆☆ | 小 | 瓶颈在 SPI 速率时 |
| DMA 引擎 | ★★★★☆ | ★★★☆☆ | 中 | 全屏像素批量刷新 |
| 多外设仲裁 | ★★☆☆☆ | ★★☆☆☆ | 小 | SPI 引脚资源紧张 |

---

## 推荐起步方案

对于当前项目（调试显示温度/湿度/气压），推荐**方向一（帧缓冲）+ 方向三-A（SPI 桥接）**的组合：

1. FPGA 用 Block RAM 建一个小型显存（可降低色深到 RGB332 以节省容量）
2. STM32 通过 SPI 发送像素写入命令，FPGA 写入显存
3. FPGA 以固定速率自动将显存内容刷新到 LCD
4. STM32 不再需要等待 SPI 传输完成，CPU 时间释放给传感器采集

这样改动最小，收益最明显，且后续可在 FPGA 中叠加图形加速模块。
