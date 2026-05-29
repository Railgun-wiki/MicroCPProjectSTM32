# F103C8 触屏 GUI 架构方案对比

## 约束条件

| 资源 | F103C8 可用量 |
|------|--------------|
| Flash | 64 KB |
| RAM | 20 KB |
| CPU | 72 MHz Cortex-M3 |
| SPI | 18 MHz（可提至 36 MHz） |

**目标**：480×320 ST7796S LCD 触屏菜单交互（页面切换、按钮、参数调节）。

**LVGL 不可行**：框架本体 ~40KB Flash + 字体 ~10KB + 运行时 ~16KB RAM，已超限。

---

## 方案 A：FPGA 图形协处理器

### 架构

```
┌─────────┐   指令协议    ┌──────────────────────────┐   SPI   ┌─────────┐
│  STM32  │ ──────────> │  FPGA                    │ ──────> │  LCD    │
│         │              │                          │         │ 480×320 │
│ 业务逻辑  │              │  ┌────────────────────┐ │         └─────────┘
│ 指令发送  │              │  │ 字模 ROM (16x8)     │ │
│ 触屏回调  │ <────────── │  │ 菜单/按钮模板       │ │
│           │  触屏事件    │  │ 帧缓冲 (RGB332)    │ │         ┌─────────┐
│           │              │  │ LCD 刷新状态机     │ │ <─────> │  Touch  │
│           │              │  │ 触摸屏 SPI 驱动    │ │         └─────────┘
└─────────┘              │  └────────────────────┘ │
                          └──────────────────────────┘
```

### STM32 侧资源占用

| 项目 | Flash | RAM |
|------|-------|-----|
| 指令发送驱动 | ~1 KB | ~100 B |
| 触屏事件处理 | ~1 KB | ~200 B |
| 菜单状态机 | ~2 KB | ~500 B |
| **合计** | **~4 KB** | **~1 KB** |

### FPGA 侧模块

| 模块 | 功能 | 资源估算 |
|------|------|---------|
| SPI Slave（指令接收） | 解析 STM32 发来的绘图指令 | ~200 LUT |
| 命令解析器 | 识别指令类型，分发到渲染引擎 | ~300 LUT |
| 渲染引擎 | 矩形填充、画线、字符渲染 | ~800 LUT |
| 字模 ROM | 16×8 ASCII 字模，1520 字节 | 1× BRAM18K |
| 帧缓冲 | 480×320×8bit (RGB332) = 150KB | ~8× BRAM18K |
| LCD 刷新状态机 | 读取帧缓冲，生成 SPI 时序 | ~400 LUT |
| 触摸屏 SPI 驱动 | 读取 ADS7846 坐标数据 | ~200 LUT |
| **合计** | | **~1900 LUT + 9 BRAM18K** |

适合 Xilinx Spartan-7 / Artix-7 等中等规模 FPGA。

### 指令协议设计

STM32 通过 SPI 向 FPGA 发送指令帧：

```
帧格式: [CMD] [LEN] [PAYLOAD...] 

CMD 字段 (8bit):
  0x01 CLEAR        [COLOR_H] [COLOR_L]                    -- 全屏清色
  0x02 FILL_RECT    [X0][Y0][X1][Y1][COLOR_H][COLOR_L]     -- 矩形填充
  0x03 DRAW_TEXT    [X][Y][FC_H][FC_L][BC_H][BC_L][LEN][STRING...] -- 文本
  0x04 DRAW_LINE    [X0][Y0][X1][Y1][COLOR_H][COLOR_L]     -- 画线
  0x05 DRAW_RECT    [X0][Y0][X1][Y1][COLOR_H][COLOR_L]     -- 矩形边框
  0x06 DRAW_CIRCLE  [XC][YC][R][COLOR_H][COLOR_L][FILL]    -- 画圆
  0x07 DRAW_BUTTON  [X][Y][W][H][FC][BC][LEN][TEXT...]      -- 按钮模板
  0x08 SET_PIXEL    [X][Y][COLOR_H][COLOR_L]                -- 画点
  0x09 REFRESH                                           -- 提交帧缓冲到 LCD

触屏事件 (FPGA -> STM32):
  0x80 TOUCH_DOWN   [X_H][X_L][Y_H][Y_L]
  0x81 TOUCH_UP
  0x82 TOUCH_MOVE   [X_H][X_L][Y_H][Y_L]
```

### MCU 侧代码示例

```cpp
// 菜单界面构建 -- 纯指令序列，无需 GUI 框架
void drawMainMenu(FpgaLcd& lcd) {
    lcd.clear(0x0000);                              // 黑色背景
    lcd.fillRect(0, 0, 479, 30, 0x001F);            // 蓝色标题栏
    lcd.drawText(10, 8, "=== SENSOR DASHBOARD ===", 0xFFFF, 0x001F);
    lcd.drawButton(20, 60, 200, 60, 0xFFFF, 0x07E0, "Temperature");
    lcd.drawButton(20, 140, 200, 60, 0xFFFF, 0x07E0, "Pressure");
    lcd.drawButton(20, 220, 200, 60, 0xFFFF, 0x07E0, "Settings");
    lcd.refresh();
}

// 触屏事件处理
void onTouchEvent(uint16_t x, uint16_t y) {
    if (isInRect(x, y, 20, 60, 220, 120)) {
        switchToPage(PAGE_TEMPERATURE);
    } else if (isInRect(x, y, 20, 140, 220, 200)) {
        switchToPage(PAGE_PRESSURE);
    }
}
```

### 色深折中：RGB332

480×320×16bit = 300KB，超出低成本 FPGA BRAM 容量。使用 RGB332（8bit）：

| 色深 | 帧缓冲大小 | BRAM18K 消耗 | 色彩表现 |
|------|-----------|-------------|---------|
| RGB565 (16bit) | 300 KB | ~17 块 | 全彩 |
| RGB332 (8bit) | 150 KB | ~9 块 | 256 色，够用 |
| RGB222 (6bit) | 112 KB | ~7 块 | 64 色，勉强 |

菜单 UI 对色彩要求不高，RGB332 完全够用。

---

## 方案 B：STM32 轻量自绘 GUI

### 架构

```
┌──────────────────────────┐   SPI   ┌─────────┐
│  STM32                   │ ──────> │  LCD    │
│                          │         │ 480×320 │
│  ┌────────────────────┐ │         └─────────┘
│  │ TinyGUI (~8KB)     │ │
│  │  - fillRect        │ │         ┌─────────┐
│  │  - drawText        │ │ <─────> │  Touch  │
│  │  - drawButton      │ │         └─────────┘
│  │  - menuFSM         │ │
│  └────────────────────┘ │
│                          │
│  ┌────────────────────┐ │
│  │ LcdBsp (已有)      │ │
│  │ TouchBsp (新增)    │ │
│  └────────────────────┘ │
└──────────────────────────┘

FPGA: 仅透传
```

### 资源占用

| 项目 | Flash | RAM |
|------|-------|-----|
| LcdBsp（已有） | ~4 KB | ~200 B |
| TouchBsp（触屏 SPI 读取） | ~2 KB | ~100 B |
| TinyGUI 引擎 | ~5 KB | ~500 B |
| 菜单/页面逻辑 | ~3 KB | ~1 KB |
| 字体数据 (16×8) | ~1.5 KB | 0（const） |
| **合计** | **~16 KB** | **~2 KB** |

剩余 Flash ~48KB、RAM ~18KB 给业务逻辑，充裕。

### TinyGUI 设计

不使用帧缓冲，直接写 LCD GRAM（与当前 LcdBsp 相同的方式）：

```cpp
// 最小化的控件系统
namespace TinyGUI {

// 控件类型
enum class WidgetType : uint8_t {
    Label,    // 静态文本
    Value,    // 动态数值（温度、湿度等）
    Button,   // 可点击按钮
    BarChart  // 简单柱状图/进度条
};

// 控件描述符（固定大小，存 Flash）
struct Widget {
    WidgetType type;
    uint16_t x, y, w, h;
    uint16_t fgColor, bgColor;
    char text[20];          // 静态文本
    float* valuePtr;        // 动态值指针（Value 类型用）
    const char* format;     // 显示格式（如 "%.1f C"）
    uint8_t page;           // 所属页面
    void (*onClick)();      // 点击回调
};

// 页面描述
struct Page {
    const Widget* widgets;
    uint8_t widgetCount;
    const char* title;
};

// 引擎
class Engine {
public:
    void addPage(const Page& page);
    void switchPage(uint8_t index);
    void handleTouch(uint16_t x, uint16_t y);
    void update();  // 刷新当前页面（仅更新变化的 Value 控件）

private:
    void drawWidget(const Widget& w, bool forceRedraw);
    void drawButton(const Widget& w, bool pressed);
    void drawBarChart(const Widget& w, float value, float max);

    ILcdDisplay& m_lcd;
    uint8_t m_currentPage{0};
    float m_lastValues[16]{};  // 缓存，用于局部刷新
};

} // namespace TinyGUI
```

### 使用示例

```cpp
// 定义页面（全部存 Flash）
static const Widget tempPageWidgets[] = {
    { WidgetType::Label, 20, 50, 200, 20, WHITE, BLACK, "Temperature", nullptr, nullptr, 0, nullptr },
    { WidgetType::Value, 20, 80, 200, 24, GREEN, BLACK, "", &g_temp, "%.1f C", 0, nullptr },
    { WidgetType::BarChart, 20, 110, 200, 20, CYAN, DARKGRAY, "", &g_temp, nullptr, 0, nullptr },
    { WidgetType::Button, 20, 260, 100, 40, WHITE, BLUE, "MUTE", nullptr, nullptr, 0, onMuteClick },
};

static const Page tempPage = { tempPageWidgets, 4, "Temperature" };

// 主循环
void loop() {
    gui.update();  // 局部刷新变化的数值
}
```

### 触屏驱动（TouchBsp）

```cpp
class TouchBsp {
public:
    TouchBsp(SPI_HandleTypeDef* hspi,
             GPIO_TypeDef* csPort, uint16_t csPin,
             GPIO_TypeDef* irqPort, uint16_t irqPin);

    bool isTouched();                    // IRQ 引脚检测
    bool readPosition(uint16_t& x, uint16_t& y);  // SPI 读取坐标

private:
    uint16_t readChannel(uint8_t cmd);   // 读取 ADS7846 单通道
};
```

---

## 两方案对比

| 维度 | 方案 A：FPGA 协处理器 | 方案 B：STM32 自绘 |
|------|---------------------|-------------------|
| **MCU Flash** | ~4 KB | ~16 KB |
| **MCU RAM** | ~1 KB | ~2 KB |
| **FPGA 开发量** | 大（~2000 LUT + 9 BRAM + Verilog 开发） | 无（仅透传） |
| **视觉效果** | 好（硬件加速、可做动画/抗锯齿） | 一般（矩形/文字/柱状图） |
| **开发周期** | 2~4 周（FPGA + 协议 + MCU 驱动） | 3~5 天（纯 MCU 代码） |
| **调试难度** | 高（FPGA 逻辑 + 跨芯片协议） | 低（单芯片，串口调试） |
| **可维护性** | 中（Verilog 修改成本高） | 高（纯 C++，改 UI 很快） |
| **触屏响应** | 快（FPGA 硬件 SPI） | 够用（MCU 软件轮询） |
| **升级灵活性** | 低（改 FPGA 需重新综合） | 高（改代码重新烧录） |

---

## 建议

**先用方案 B，未来按需升级到方案 A。**

理由：

1. **方案 B 的视觉效果对调试场景完全够用** -- 按钮、数值、柱状图、颜色状态指示，不需要动画和抗锯齿
2. **开发速度快** -- 3~5 天可出原型，方案 A 的 FPGA 开发可能卡在时序调试上
3. **资源有余量** -- 16KB Flash + 2KB RAM 的 GUI 占用，剩余 48KB Flash / 18KB RAM 给业务逻辑
4. **后续可渐进升级** -- 方案 B 的 `ILcdDisplay` 接口不变，未来可以把 `LcdBsp` 替换为 `FpgaLcdBsp`（发指令给 FPGA），上层代码零改动
