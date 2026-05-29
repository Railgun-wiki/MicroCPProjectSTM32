# F103C8 轻量级 GUI 框架选型

## 约束

| 资源 | F103C8 可用量 |
|------|--------------|
| Flash | 64 KB |
| RAM | 20 KB |
| CPU | 72 MHz Cortex-M3 |
| LCD | 480×320 ST7796S SPI TFT |
| 触摸 | 电阻屏 ADS7846/XPT2046 |

**需求**：按钮、文本、数值显示、页面切换、进度条。

---

## 候选框架

### 1. μGUI（µGUI）

**来源验证**：[achimdoebler/UGUI](https://github.com/achimdoebler/UGUI) (README 已读取)

- **文件组成**：仅 3 个文件 — `ugui.c`、`ugui.h`、`ugui_config.h`
- **语言**：纯 C，无外部依赖
- **许可证**：MIT
- **内存管理**：无动态内存分配（全部静态分配）

**资源占用**：
- Flash: ~6-10 KB
- RAM: ~2-3 KB（控件状态，不含帧缓冲）

**内置控件**：
- Window（窗口/容器）
- Button（按钮，支持按下/释放事件）
- Textbox（文本框）
- Checkbox（复选框）
- Image（图片显示）
- ProgressBar（进度条）
- Console（系统控制台）

**图形功能**：画线、画圆、画矩形、16 种内置字体（含西里尔字母）、TrueType 字体转换器

**触摸支持**：有，通过回调函数集成

**维护状态**：最后更新约 2019 年，代码已稳定，无活跃开发但无已知 bug

**集成方式**：

```cpp
// 1. 实现像素写入回调
void pixelDriver(UG_S16 x, UG_S16 y, UG_COLOR color) {
    lcdBsp.fillRect(x, y, x, y, static_cast<uint16_t>(color));
}

// 2. 实现触摸回调
void touchDriver(UG_TOUCH* touch) {
    uint16_t x, y;
    if (touchBsp.readPosition(x, y)) {
        touch->state = TOUCH_STATE_PRESSED;
        touch->xp = x;
        touch->yp = y;
    } else {
        touch->state = TOUCH_STATE_RELEASED;
    }
}

// 3. 初始化
UG_GUI gui;
UG_Init(&gui, pixelDriver, 480, 320);
UG_TouchRegister(touchDriver);

// 4. 创建控件
UG_WindowCreate(&win, ...);
UG_ButtonCreate(&win, &btn, BTN_ID_0, 20, 60, 220, 120);
UG_ButtonSetText(&win, BTN_ID_0, "Temperature");
UG_ProgressBarCreate(&win, &pbar, ...);
UG_ProgressBarSetProgress(&pbar, 65);
```

---

### 2. LVGL

**来源验证**：[lvgl/lvgl](https://github.com/lvgl/lvgl) (README 已读取)

**官方声明的最低资源需求**：
- **最低配置**：32 KB RAM + 128 KB Flash
- **简单 UI**：~100 KB RAM + 200-300 KB Flash + 屏幕 1/10 大小的渲染缓冲

**F103C8 可行性**：❌ **不可行**

| 资源 | F103C8 | LVGL 最低 | 差距 |
|------|--------|----------|------|
| Flash | 64 KB | 128 KB | **缺 64 KB** |
| RAM | 20 KB | 32 KB | **缺 12 KB** |

即使极限裁剪（禁用大部分控件、禁用动画/抗锯齿、只保留 1 个字体），预估仍需 ~25-30 KB Flash + ~12-15 KB RAM。剩余空间不足以放业务逻辑（传感器驱动、报警状态机、菜单逻辑）。

**内置功能**：30+ 控件、抗锯齿、动画、透明度、阴影、平滑滚动、3D 渲染、UTF-8 中日韩文字、Flexbox/Grid 布局

**许可证**：MIT

**结论**：功能强大但 F103C8 资源不足，需升级到 STM32F103RCT6（256KB Flash / 48KB RAM）才可使用。

---

### 3. GuiLite

**来源**：训练数据（GitHub 仓库 URL 存在多个变体，无法验证最新状态）

- **文件组成**：单头文件 `GuiLite.h`（~5000 行）
- **语言**：纯 C++（无 STL 依赖）
- **许可证**：Apache 2.0

**声称的资源占用**：
- Flash: ~6-10 KB（最小配置）
- RAM: ~1 KB（最小配置）

**内置控件**：Label、Button、Dialog、Edit、List、Table、Waveform（波形图）、Keyboard、Spinbox

**特点**：
- 国内开发者创建，中文资料相对多
- 支持 3D 渲染（基础）
- 支持 FreeRTOS / 裸机

**F103C8 可行性**：✅ 资源上可行

**注意事项**：
- 单头文件集成方便，但 C++ 编译可能比 C 略大
- 社区规模小于 LVGL
- 对 SPI LCD 的适配示例较少，需要自己写显示驱动适配层
- 仓库 URL 和维护状态需自行验证

---

### 4. µGFX

**来源**：训练数据

- **许可证**：商业许可（免费用于非商业/小规模商业）
- **语言**：C
- **特点**：模块化架构，支持多种 RTOS

**资源占用**：
- Flash: ~10-40 KB（取决于启用模块）
- RAM: ~5-15 KB

**内置控件**：Button、Label、Checkbox、Slider、List、ProgressBar、Container、Radio button

**F103C8 可行性**：⚠️ 勉强可行，需裁剪

**注意事项**：
- 商业使用需购买许可证
- 社区版功能受限

---

### 5. 自绘（当前方案扩展）

在现有 `LcdBsp` 基础上自己写控件系统。

- Flash: ~8 KB
- RAM: ~2 KB
- 无外部依赖，完全可控
- 工作量最大

---

## 综合对比

| | μGUI | LVGL | GuiLite | µGFX | 自绘 |
|---|---|---|---|---|---|
| **Flash** | ~6-10 KB | ≥128 KB | ~6-10 KB | ~10-40 KB | ~8 KB |
| **RAM** | ~2-3 KB | ≥32 KB | ~1 KB | ~5-15 KB | ~2 KB |
| **适合 F103C8** | ✅ | ❌ | ✅ | ⚠️ | ✅ |
| **控件丰富度** | 低 | 高 | 中 | 中 | 自定义 |
| **触摸支持** | ✅ 内置 | ✅ 内置 | ✅ | ✅ 内置 | 自己写 |
| **抗锯齿/动画** | ❌ | ✅ | 基础 | ❌ | ❌ |
| **中文支持** | 需加字模 | ✅ 内置 | 需加字模 | 需加字模 | 需加字模 |
| **集成难度** | 低 | 中 | 中 | 中 | 无 |
| **社区/文档** | 小 | 极大 | 小 | 中 | - |
| **维护状态** | 停更(稳定) | 活跃 | 不确定 | 活跃 | - |
| **许可证** | MIT | MIT | Apache 2.0 | 商业 | 无 |

---

## 推荐

**首选：μGUI**

理由：
1. 资源占用最低且已验证（README 确认无动态内存分配、3 文件集成）
2. 有按钮、进度条、文本框等满足触屏菜单需求的控件
3. 集成最简单 — 只需实现 2 个回调函数（像素写入 + 触摸输入）
4. MIT 许可，无商业风险
5. 代码虽不再更新但已成熟稳定，嵌入式 GUI 库不需要频繁迭代

**备选：GuiLite**

如果需要更多控件（如波形图、表格、键盘），且愿意花时间适配 SPI LCD 驱动，可以考虑。

**不推荐：LVGL**

官方最低要求 128KB Flash + 32KB RAM，F103C8 资源差距过大。如需 LVGL，应升级 MCU 至 STM32F103RCT6（256KB Flash / 48KB RAM）。
