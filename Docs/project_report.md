# 环境温度与气压监测系统项目设计与工程报告

本报告系统性地整理了本项目（基于 STM32F103 MCU 与 Xilinx Zynq-7000 FPGA 联合设计的环境温度与气压监测系统）的硬件设计、资源分配、核心优势特色、开发历程及小组心得体会。

---

## 一、 硬件设计图 (系统模块连接图)

系统硬件由 **STM32F103 微控制器**、**Xilinx Zynq-7000 FPGA**、**底板外设**（LCD 屏、触摸屏、物理按键、状态 LED）以及**环境传感器**（AHT20、BMP280）组成。
FPGA 在本系统中主要作为**可编程数字交叉互连矩阵**（数字透传通道），实现了 MCU 引脚与底板外设物理引脚的软硬件解耦。

以下是系统的硬件模块连接与信号流向图（使用 Mermaid 绘制）：

```mermaid
graph LR
    %% Subgraphs for high-level division
    subgraph MCU_Block ["STM32F103 MCU 主控"]
        MCU["STM32F103"]
    end

    subgraph FPGA_Block ["Zynq-7000 FPGA 桥接器"]
        FPGA["XC7Z020 PL 逻辑透传"]
    end

    subgraph Peripherals_Block ["底板物理外设"]
        LCD["ST7796S TFT LCD 屏"]
        Touch["XPT2046 触摸板"]
        Keys["底板按键 (S0/S2/S3)"]
        LED["状态指示灯 (LED0)"]
    end

    subgraph Direct_Block ["直接总线外设"]
        Sensors["I2C 传感器 (AHT20 & BMP280)"]
        PC["PC 调试终端 (USART1)"]
    end

    %% Bus connections (clean, grouped arrows)
    
    %% MCU <-> FPGA
    MCU -- "LCD SPI & 控制总线<br/>(PA5/PA7/PB5-PB8)" --> FPGA
    MCU -- "触摸控制总线 (Bit-bang)<br/>(PA8/PB3/PB4/PA1/PA0)" <--> FPGA
    MCU <-- "独立按键回送<br/>(PA2/PA3/PA4)" -- FPGA
    MCU -- "PWM 状态灯信号<br/>(PB0/TIM3_CH3)" --> FPGA

    %% FPGA <-> Peripherals
    FPGA -- "TFT LCD 驱动总线<br/>(N19/M21/N20/P22/N22/M22)" --> LCD
    FPGA -- "触摸控制物理总线<br/>(M19/R21/P20/P21/P15)" <--> Touch
    Keys -- "按键物理输入<br/>(H15/J15/W18)" --> FPGA
    FPGA -- "PWM 驱动输出<br/>(U16)" --> LED

    %% MCU <-> Sensors / PC (Direct)
    MCU -- "I2C2 硬件总线<br/>(PB10/PB11)" --> Sensors
    MCU -- "USART1 调试日志<br/>(PA9/PA10)" --> PC

    %% Styles
    style MCU fill:#1a365d,stroke:#3182ce,stroke-width:2px,color:#fff
    style FPGA fill:#2d3748,stroke:#4a5568,stroke-width:2px,color:#fff
    style LCD fill:#2c5282,stroke:#4299e1,stroke-width:2px,color:#fff
    style Touch fill:#2b6cb0,stroke:#63b3ed,stroke-width:2px,color:#fff
    style Keys fill:#2c5282,stroke:#4299e1,stroke-width:2px,color:#fff
    style LED fill:#2c5282,stroke:#4299e1,stroke-width:2px,color:#fff
    style Sensors fill:#22543d,stroke:#38a169,stroke-width:2px,color:#fff
    style PC fill:#744210,stroke:#d69e2e,stroke-width:2px,color:#fff
```

---

## 二、 实验箱资源使用占比与选用思路

### 1. 实验箱平台芯片资源占比

*   **FPGA 芯片资源 (Xilinx Zynq-7000 XC7Z020CLG484-1)**
    *   **逻辑资源 (LUT/Register/BRAM/DSP) 占比：~0%**
        *   由于本项目中 FPGA 纯粹作为**引脚透传桥接器**（采用 Verilog 的 `assign` 组合逻辑互连），未占用时序逻辑与逻辑门，仅使用内部布线资源。
    *   **引脚资源 (User I/O) 占比：~15.0%**
        *   XC7Z020CLG484 拥有约 200 个用户 I/O，本项目共使用 **30 个引脚**（ARM 侧 15 个，底板侧 15 个），接口电平全部约束为 `LVCMOS33` 标准。

*   **MCU 芯片资源 (STM32F103XB - 72MHz)**
    *   **定时器资源**：使用 `TIM3`，启用其通道 3 (`PB0`) 用于输出状态 LED 的 PWM 信号。
    *   **SPI 通信资源**：使用 `SPI1` 主机模式（APB2 时钟二分频，高达 36MHz），用于高速驱动 LCD 屏。
    *   **I2C 通信资源**：使用硬件 `I2C2` 接口（`PB10/PB11`），工作在 Standard 模式下，挂载 AHT20 与 BMP280 传感器。
    *   **串口资源**：使用调试串口 `USART1`（波特率 115200），实时输出系统运行健康状态和调试边缘日志。
    *   **时基与中断资源**：使用系统 `SysTick` 定时器（1ms）作为任务调度时基；使用 `EXTI Line 0` 中断（映射至 `PA0`），用于触摸屏落笔/释放的快速检测。
    *   **Flash / RAM 占用**：在启用 `-fno-exceptions` 和 `-fno-rtti` 编译优化下，Flash 占用约 **45KB** 左右，RAM 占用不足 **10KB**（包含 300 点的双浮点环形数据缓冲区）。

---

### 2. 资源选用与分配思路

1.  **FPGA 软硬件解耦通道**：
    为了在不重新铺设物理电路的情况下，灵活地在主板 MCU 与底板 LCD 屏、触摸屏、按键和 LED 之间建立电气连接，系统将这些引脚通过 FPGA 进行了可编程映射。这既避免了大量凌乱的排线，又为后续在 FPGA 内部加入硬件级消抖逻辑、SPI 报文监听器或多路 PWM 控制器保留了可能。
2.  **高速硬件 SPI 驱动显示**：
    LCD（ST7796S，分辨率 320x480）刷新需要巨大的数据吞吐量。若采用 GPIO 模拟 SPI，页面刷新速度会有极高延迟与闪烁感。选用 STM32 的硬件 `SPI1`（72MHz 系统主频下进行 2 分频，实际运行在 36MHz 的极限速率），配合紧凑的字节级数据发送，能够实现页面的平滑加载，为实时温度/气压曲线绘制提供流畅的帧率保证。
3.  **软件模拟 SPI（GPIO Bit-bang）驱动触摸屏**：
    电阻式触摸屏芯片（XPT2046/ADS7846）对于时钟沿和转换建立时间有硬性要求，通常要求工作在 1-2MHz 的低速状态下。如果与 LCD 共用 SPI1，频繁在高速/低速、Mode0/Mode3 之间切换 SPI 配置，会产生严重的抗抖动和总线死锁问题。因此，触摸链路选用软件 GPIO Bit-bang 模拟 SPI，独占 `PA8`、`PB3`、`PB4`、`PA1`、`PA0` 资源，既彻底隔离了总线干扰，又方便了开发调试。
4.  **硬件 I2C2 传感器总线复用**：
    由于 AHT20 与 BMP280 本身支持不同的 I2C 从机地址（AHT20 为 `0x38`，BMP280 为 `0x76`），且同为 3.3V I2C 总线器件。选用 STM32 硬件 `I2C2` 挂载两路器件，既节省了引脚资源，又通过硬件外设简化了数据读写的状态管理。
5.  **硬件 PWM 控制状态指示灯**：
    为了显示“呼吸灯”效果（正常状态）或高频“双闪烁”效果（报警状态），传统做法需要依靠延时函数，这会白白消耗 CPU 的算力。我们选用定时器 `TIM3_CH3` 产生高精度的硬件 PWM，在后台通过调整占空比来产生呼吸动作，应用层只需以低频（10ms）更新占空比，极大降低了对系统实时性的影响。

---

### 3. 系统详细管脚约束与映射表

以下为 FPGA（Vivado 物理约束 `LCD.xdc`）中配置的引脚映射与电平约束。

| 信号名称 (FPGA 端口) | FPGA 管脚 | I/O 类型 | 电平标准 (IOSTANDARD) | STM32 侧物理引脚 | 底板外设侧物理引脚 | 信号描述与作用 |
| :--- | :--- | :--- | :--- | :--- | :--- | :--- |
| **CS_ARM** | `Y21` | Input | LVCMOS33 | `PB5` | - | STM32 驱动 LCD 片选信号输入 FPGA |
| **CS_BASE** | `N19` | Output | LVCMOS33 | - | LCD_CS (J5 引脚) | FPGA 透传输出至 LCD 片选引脚 |
| **DC_ARM** | `AB20` | Input | LVCMOS33 | `PB7` | - | STM32 驱动 LCD 数据/命令选择信号 |
| **DC_BASE** | `M21` | Output | LVCMOS33 | - | LCD_RS/DC (J5 引脚) | FPGA 透传输出至 LCD 数据/命令引脚 |
| **RST_ARM** | `Y18` | Input | LVCMOS33 | `PB8` | - | STM32 驱动 LCD 硬件复位信号 |
| **RST_BASE** | `N20` | Output | LVCMOS33 | - | LCD_RST (J5 引脚) | FPGA 透传输出至 LCD 复位引脚 |
| **LED_ARM** | `AB19` | Input | LVCMOS33 | `PB6` | - | STM32 驱动 LCD 背光控制信号 |
| **LED_BASE** | `P22` | Output | LVCMOS33 | - | LCD_LED (J5 引脚) | FPGA 透传输出至 LCD 背光引脚 |
| **SCK_ARM** | `W22` | Input | LVCMOS33 | `PA5` | - | STM32 硬件 SPI1_SCK 时钟信号 |
| **SCK_BASE** | `N22` | Output | LVCMOS33 | - | LCD_SCK (J5 引脚) | FPGA 透传输出至 LCD SPI 时钟引脚 |
| **SDI_ARM** | `V19` | Input | LVCMOS33 | `PA7` | - | STM32 硬件 SPI1_MOSI 数据信号 |
| **SDI_BASE** | `M22` | Output | LVCMOS33 | - | LCD_SDI (J5 引脚) | FPGA 透传输出至 LCD SPI 数据引脚 |
| **TCLK_ARM** | `AA22` | Input | LVCMOS33 | `PA8` | - | STM32 软件模拟触摸屏时钟信号 |
| **TCLK_BASE** | `M19` | Output | LVCMOS33 | - | Touch_TCLK (J5 引脚) | FPGA 透传输出至触摸 IC 时钟引脚 |
| **TCS_ARM** | `AA19` | Input | LVCMOS33 | `PB4` | - | STM32 软件模拟触摸屏片选信号 |
| **TCS_BASE** | `R21` | Output | LVCMOS33 | - | Touch_TCS (J5 引脚) | FPGA 透传输出至触摸 IC 片选引脚 |
| **TDIN_ARM** | `W17` | Input | LVCMOS33 | `PB3` | - | STM32 软件模拟触摸屏数据输入 |
| **TDIN_BASE** | `P20` | Output | LVCMOS33 | - | Touch_TDIN (J5 引脚) | FPGA 透传输出至触摸 IC 数据写入 |
| **TDO_BASE** | `P21` | Input | LVCMOS33 | - | Touch_TDO (J5 引脚) | 触摸 IC 数据输出 (DOUT) 进入 FPGA |
| **TDO_ARM** | `U21` | Output | LVCMOS33 | `PA1` | - | FPGA 回送数据至 STM32 接收引脚 |
| **TIRQ_BASE** | `P15` | Input | LVCMOS33 | - | Touch_TIRQ (J5 引脚) | 触摸按下中断信号 (PEN) 进入 FPGA |
| **TIRQ_ARM** | `T21` | Output | LVCMOS33 | `PA0` (EXTI0) | - | FPGA 回送中断信号至 STM32 中断线 |
| **LED_STATUS_ARM**| `Y20` | Input | LVCMOS33 | `PB0` (TIM3_CH3) | - | STM32 发出的状态灯 PWM 信号输入 |
| **LED0_BASE** | `U16` | Output | LVCMOS33 | - | LED0 (底板状态指示灯) | FPGA 透传驱动底板上的物理指示 LED |
| **KEY0_BASE** | `H15` | Input | LVCMOS33 | - | S0 按键 (底板物理) | 底板 S0 按键物理信号输入 FPGA |
| **KEY0_ARM** | `T22` | Output | LVCMOS33 | `PA2` | - | FPGA 回送 S0 状态至 STM32 切页按键 |
| **KEY2_BASE** | `J15` | Input | LVCMOS33 | - | S2 按键 (底板物理) | 底板 S2 按键物理信号输入 FPGA |
| **KEY2_ARM** | `U22` | Output | LVCMOS33 | `PA3` | - | FPGA 回送 S2 状态至 STM32 确认按键 |
| **KEY3_BASE** | `W18` | Input | LVCMOS33 | - | S3 按键 (底板物理) | 底板 S3 按键物理信号输入 FPGA |
| **KEY3_ARM** | `V22` | Output | LVCMOS33 | `PA4` | - | FPGA 回送 S3 状态至 STM32 返回按键 |

---

## 三、 系统特色综述 (小组差异化亮点与创新优势)

相比于其他常规的、采用裸机轮询或阻塞设计的温度监测系统，本设计在交互安全性、任务实时性、图形化显示以及面向对象架构上，进行了深度定制，具备以下五大差异化设计亮点与创新优势：

### 亮点 1：开机交互式四点电阻触屏校准与仿射矩阵拟合 (消除触屏漂移)
*   **痛点**：电阻屏因硬件批次、温度变化以及装配误差，其原始 ADC 采样值与物理坐标的对应关系存在漂移，使用硬编码的缩放比例极易产生触控不准的问题。
*   **设计与创新**：
    我们开发了**独立的交互式电阻屏校准引导程序**。系统开机时，若检测到物理按键 `S0` 处于按下状态，将引导进入全屏的校准模式。LCD 屏上四个角会依次闪烁红色十字靶心，要求用户用触控笔点按。在收集四个关键点坐标后，算法在内存中动态建立**仿射变换矩阵**：
    \[
    X_{pixel} = A \cdot X_{raw} + B \cdot Y_{raw} + C
    \]
    \[
    Y_{pixel} = D \cdot X_{raw} + E \cdot Y_{raw} + F
    \]
    实时求解出拉伸、旋转、平移和剪切修正参数，并通过串口输出详细的校准系数，然后将此矩阵应用于整个运行期，实现精准的像素级触控体验。

### 亮点 2：系统级协作式非阻塞调度与节拍任务表 (全流程零阻塞)
*   **痛点**：传统裸机单片机设计中，常在 `main()` 循环里使用 `HAL_Delay()` 强行等待，或使用 while 语句死等外设响应。例如 AHT20 的 80ms ADC 转换、按键的 20ms 延时消抖、以及 LCD 的高频刷新等，都会导致 CPU 长时间被死锁，造成 LED 呼吸卡顿、按键丢失或屏幕帧率断档。
*   **设计与创新**：
    本系统实现了一套**系统级协作式非阻塞调度框架**，彻底移除了任何 CPU 忙等与阻塞机制：
    1.  **SysTick 异步时间切片**：单片机 SysTick 产生 1ms 时基，并在 10ms 中断回调中向应用层分发周期任务标志表。主循环只消费标志，任务（LED呼吸、按键扫描、触摸轮询、UI 刷新、日志输出）分别按 10ms/20ms/50ms/100ms/1000ms 等周期片内并行，零忙等。
    2.  **传感器协作状态机**：`Aht20Bsp` 内部建立非阻塞采样状态机。只在触发时发送指令，随后释放 CPU；由 10ms 调度器静默倒计时，时间一到再触发异步读取，在 80ms 传感器转换期间 CPU 利用率接近 0%。
    3.  **中断与主循环异步对齐**：电阻屏触笔按下 (`TOUCH_PEN`) 触发外部中断 EXTI0 后，ISR 中绝不调用耗时的模拟 SPI 读坐标逻辑，仅置位标志即退出；具体的坐标读取与消抖工作在主循环的低频时间片内完成，确保了系统运行的绝对流畅。

### 亮点 3：300 点历史走势环形双缓冲区与实时曲线动态推移
*   **痛点**：大多数项目仅在 LCD 上以单调的数字来呈现当前数据，缺乏对环境整体温度或气压变化趋势的可视化管理。
*   **设计与创新**：
    我们在 `AppController` 中开辟了 300 点的数据环形双缓冲区。系统以 1Hz（1 秒 1 次）的频率，后台记录传感器温度与气压的历史波动。配合自主编写的 `GuiEngine` 几何绘图原语（支持高效折线连线算法），在 Page 0（温湿页）绘制红色的 5 分钟温度趋势曲线，在 Page 1（气压页）绘制蓝色的 5 分钟气压/海拔趋势曲线。图表每秒向左平滑推移一次，能够直观、无延时地呈现出当前室内温度波动的方向与速率。

### 亮点 4：工业级双重告警确认抑制与视觉安全指示冗余
*   **痛点**：常规报警系统在被操作员按下“静音”后，通常会关闭一切提示，操作员很容易因为界面没有警示，而遗忘环境风险依然存在这一事实。
*   **设计与创新**：
    我们借鉴了工业安全控制系统的“确认但非清除”的交互设计规范。当系统判定参数超出阈值时：
    1.  **首段告警**：LCD 显示醒目的红色闪烁 Banner（如 `Alarm: Temp Abnormal!`），后台状态 LED 转为高频双闪警示。
    2.  **告警确认 (S1 按键)**：按下 `S1` 键，LCD 页面的红色警示条消除，系统状态置为 `MUTED`（抑制干扰）。
    3.  **安全冗余**：此时底部的状态指示灯**依然保持高频闪烁**！作为无声的物理警示，直至环境温湿度自然降回安全线以下，状态灯才会恢复为呼吸状态。这种设计确保了既能“不干扰屏幕常态监控”，又能“通过硬件指示灯保持风险的持续警醒”。

### 亮点 5：纯 C++17 现代 OOP 架构与依赖倒置（DIP）的彻底贯彻
*   **痛点**：学生单片机项目通常将所有初始化与外设读写堆砌在 `main.c` 中，存在大量紧耦合，无法进行无硬件单元测试，且更换传感器或总线时需要重构绝大部分代码。
*   **设计与创新**：
    工程遵循现代软件设计原则，全面基于 C++17 编写，并在 `GNU` 交叉编译器下屏蔽 RTTI 与异常以控制固件体积。整个工程严格遵照**依赖倒置原则（Dependency Inversion Principle）**：应用层核心 `AppController` 仅面对 `ITempHumSensor`、`IPressureSensor`、`ILcdDisplay`、`ITouch`、`IIndicator` 等纯虚抽象接口编程。所有外设在 `app_entry.cpp` 中采用静态生命周期的类实例化，并在入口处显式通过构造函数**依赖注入**。
    这允许我们实现极为灵活的外设适配。例如，在需要由硬件 I2C 改为软件 GPIO 模拟 I2C 时，整个高层应用逻辑和传感器控制流无需更改任何一个字，只需修改 `app_entry.cpp` 中实例化的类并重定向至 `SoftI2cBsp` 即可。

---

## 四、 项目完整开发历程与所用全部开发工具

### 1. 基于 Git 提交记录梳理项目真实开发历程

根据项目代码仓库的 Git 提交记录，本项目的开发历程呈清晰的、以接口定义为先导、驱动渐进实现、最终向系统联调与鲁棒性优化迭代的演进轨迹。具体历程整理如下：

#### 【第一阶段：工程初始化与 C++17 现代架构设计（Git 提交点 14b1a29 ~ 4ce4ef7）】
*   **动作**：引入 STM32F103 基础工程与 CMake 构建系统，在 `CMakeLists.txt` 中开启 C++17 支持及嵌入式编译器优化参数 (`-fno-exceptions`, `-fno-rtti`)。
*   **架构确立**：在 `SYSTEM/sys.hpp` 中创建全局系统级定义与宏，并在 `App/Inc/` 中定义了传感器、显示屏、触摸屏、指示灯、按键等核心外设的**纯虚抽象类接口**（如 `ITempHumSensor` 等），确立了整个工程面向接口编程与依赖倒置（DIP）的底层契约。
*   **生命周期管理**：实现 C++ 静态运行期桥接与对象装配管理器 `app_entry`，作为系统对象的装配与生命周期入口。

#### 【第二阶段：板级驱动包 (BSP) 编写与定时器配置（Git 提交点 afd7d76 ~ 783d5c5）】
*   **基础驱动**：顺序开发了软件模拟 I2C 驱动 (`SoftI2cBsp`)，在此基础上实现了 AHT20 温湿度传感器与 BMP280 气压/海拔传感器的读写时序。
*   **按键与状态灯**：编写了按键消抖扫描驱动 (`ButtonBsp`)，以及利用 STM32 定时器控制占空比实现呼吸/闪烁效果的指示灯驱动 (`PwmLedBsp`)。
*   **硬件定时器注入**：配置并启用了由 CubeMX 生成的 `TIM3_CH3` PWM 输出，修正了初始化冲突，并在 BSP 层面将定时器输出通道与 `PwmLedBsp` 进行参数绑定。

#### 【第三阶段：显示与触摸总线整合及 FPGA 桥接（Git 提交点 d03368a ~ b9c5846）】
*   **显示链路开发**：定义 `ILcdDisplay` 接口，并基于底层硬件 SPI1 实现了 LCD（ST7796S）的初始化、像素绘制、矩形填充与英文字符绘制，并整合进 `AppController`。
*   **触摸总线与 FPGA 解耦**：通过 FPGA 编写 `LCD.v` 的透传引脚逻辑，并将 `LCD.xdc` 的物理引脚电平统一约束为 `LVCMOS33` 标准，打通 ARM 侧引脚与底板侧外设引脚的连接。
*   **I2C 硬件化**：为提升读取效率，传感器总线从原先的 `SoftI2cBsp` 迁移到硬件 `I2C2` 总线，在 `app_entry.cpp` 中完成硬件 I2C 对象的静态装配。

#### 【第四阶段：非阻塞调度重构与输入中断优化（Git 提交点 336cfc2 ~ 6be9ed3）】
*   **时间切片调度**：废除传统的阻塞式 `HAL_Delay` 延时，将整个系统重构为依赖单片机 SysTick 触发的**任务节拍标志表模型**，将按键扫描、LED 呼吸、UI 刷新、传感器采样分别以 10ms/20ms/100ms/500ms 等不同周期异步轮询。
*   **输入中断对齐**：对齐触摸屏落笔引脚与 CubeMX 的 EXTI0 外部中断线，利用中断标志位异步触发坐标采样，消除主循环中的等待延迟。
*   **图形化移植**：实现 `GuiEngine` 算法引擎，并完成折线绘制与多页面（温湿页、气压高度页、阈值设置页）的 GUI 渲染逻辑迁移。

#### 【第五阶段：系统综合优化与鲁棒性提升（Git 提交点 501a41c ~ master）】
*   **去浮点化体积剪裁**：为防止浮点数 `printf` 库极度膨胀单片机 Flash，移除高层浮点计算，编写了高效的手写定点数转 ASCII 字符格式化工具函数。
*   **触摸校准与消抖**：在触摸屏驱动中实现了 **trimmed-mean (修剪平均值) 滤波算法**，并引入了开机按键 S0 触发的四点仿射变换校准矩阵，克服漂移。
*   **交互逻辑闭环**：在按键驱动中加入了鲁棒的释放消抖与 1 秒触发锁，实现 `KEY_CONFIRM` 一键静音告警、`KEY_BACK` 切换设置面板的逻辑。
*   **5分钟实时曲线容量扩张**：将历史缓冲区容量扩大至 300 点，采样间隔缩短至 1s，并通过状态机微调，使系统上电时立即开启记录，消除曲线渲染初期的“等待”断档，最终在 master 分支实现系统的高稳定性交付。

---

### 2. 本次设计所用全部开发工具（PC 软件）汇总与分享

为了保证跨平台代码的规范编写、PL 逻辑的高效编译以及系统的高效维护，本项目采用了一套“现代、规范、软硬兼收”的 PC 端软件生态。各软件分类及具体分工分享如下：

#### ① IDE / 编辑器与代码生成 (IDE & Code Generators)
*   **VS Code (Visual Studio Code)**: 
    *   *用途*：核心代码编写工具。安装了 `C/C++`、`clangd` 静态代码诊断插件以及 `CMake Tools` 等。
    *   *分享*：用于编写高层业务逻辑（`AppController`）和板级驱动包（`BSP`）。VS Code 配套的 clangd 语法分析极其敏锐，在编写 C++17 模板与抽象接口类时能做到毫秒级补全与静态错误预警。
*   **STM32CubeMX**: 
    *   *用途*：STM32 底层配置与初始代码生成软件。
    *   *分享*：用来配置系统时钟树（72MHz）、SPI1（LCD 主机发送模式）、I2C2（传感器总线）以及 TIM3_CH3 的硬件 PWM 输出引脚，导出了规范的 HAL 初始化代码骨架。
*   **Xilinx Vivado (v2019.1)**: 
    *   *用途*：FPGA 硬件开发环境。
    *   *分享*：用来编写 `LCD.v` 的 Verilog 硬件逻辑，编辑 `LCD.xdc` 引脚物理约束文件，并完成硬件的逻辑综合、实现与 Bitstream 下载。

#### ② 构建、分析与编译/烧录工具 (Build, Analysis, Compile & Loader Tools)
*   **CMake & Ninja**: 
    *   *用途*：STM32 交叉编译构建生成器与构建工具。
    *   *分享*：用于替代传统的 Keil 较慢的项目构建。配合 VS Code 可以在极短时间内完成增量编译，提供极高的开发循环迭代效率。
*   **GNU Arm Embedded Toolchain (`arm-none-eabi-gcc`)**: 
    *   *用途*：Cortex-M3 单片机交叉编译器。
    *   *分享*：我们在 CMake 中开启了 `-fno-exceptions` 和 `-fno-rtti` 编译参数，不仅完美支持 C++17，还使固件体积大幅压缩。
*   **OpenOCD & Xilinx Hardware Manager**: 
    *   *用途*：STM32 固件烧录调试器与 FPGA 硬件管理器。
    *   *分享*：通过 SWD 协议利用 OpenOCD 将编译产物烧录至单片机 Flash；利用 Vivado Hardware Manager 将比特流写入并固化到 Zynq 7020 PL 侧。
*   **STM32 Build Analyzer (ST官方 STM32 VS Code Extension 或社区插件提供)**:
    *   *用途*：静态分析与可视化呈现编译后 Flash 和 RAM (如 `.text`、`.data`、`.bss` 段) 的空间占用。
    *   *分享*：通过解析构建生成的 `.map` 或 `.elf` 文件，以图表形式直观呈现内存水位。在优化浮点打印格式化函数（以节省 Flash 空间）和设计 300 点环形双缓冲区（以防 SRAM 堆栈溢出）时，该工具提供了直观的数据支持。
*   **picocom**:
    *   *用途*：轻量级终端串口调试助手。
    *   *分享*：用于捕获单片机 `sys.hpp` 中定义的 `SYS_LOG(...)` 输出的运行时日志，相比于图形化助手占用极低系统资源，且能平滑呈现低延时的多行数据输出。
*   **Git**:
    *   *用途*：分布式版本管理与协作工具。
    *   *分享*：管理 `master` 分支与各功能开发分支，详细追踪代码演进轨迹，便于在联调阶段进行冲突合并与版本快速回退。
*   **Python 辅助脚本与库 (pyserial, matplotlib, numpy)**:
    *   *用途*：用于上位机串口数据捕获与实时波形绘制（sniffing AHT20/BMP280 遥测流）；在 PC 端验证仿射校准系数算法，并批量生成 LCD 屏所需的英文字体与 ASCII 字库 C 语言点阵数组。

#### ③ AI 辅助开发工具 (AI Coding Assistants)
*   **Antigravity (基于 Gemini 的智能编程 Agent)**:
    *   *用途*：系统设计对齐、非阻塞传感器状态机编写以及设计文档与报告的自动化生成。
*   **Claude Code**:
    *   *用途*：终端命令行环境下的自动化测试流、跨文件重构脚本生成和 Git 冲突的分析解决。
*   **Codex (OpenAI 软件工程智能体)**:
    *   *用途*：OpenAI 于 2025 年推出的先进软件工程智能体（基于 GPT-5 模型家族）。
    *   *分享*：通过 VS Code 插件与 CLI 命令行，作为自主式结对编程队友。它可以理解整个代码库语义，在沙箱环境中自主运行测试、修复 Bug 并优化复杂的算法（如触摸消抖与环形缓冲逻辑）。

#### ④ Markdown 专业文档工具 (Markdown Editors & Viewers)
*   **Typora / VS Code Markdown Preview Enhanced**: 
    *   *用途*：项目文档阅读、规范编写与流程设计工具。
    *   *分享*：用于协作维护 master 分支的关键技术规范，如 `README.md`、`Status.md`（硬件接线事实基线），以及本设计报告。借助 Markdown 实时预览，各小组成员可以在同一套文档范式下工作，实现开发与文档的同步更新。

---

## 五、 小组设计心得体会 (PPT 汇报展示版)

本部分内容已按 **PPT 幻灯片单页结构** 进行精炼提炼，小组成员可直接拷贝文本用于答辩汇报 PPT 制作，每页幻灯片配有核心提炼与关键点说明：

### 🖥️ Slide 1: 软硬协同设计与系统级解耦
*   **FPGA 充当柔性电气连线桥**：利用 Zynq PL 侧的 Verilog 透传逻辑，实现单片机引脚与底板外设物理连接的可编程映射，简化硬件连线与 PCB 布局。
*   **现代 C++ 面向对象解耦**：彻底贯彻**依赖倒置原则 (DIP)**，高层应用仅面对虚接口编程。物理总线由模拟 I2C 切换为硬件 I2C2 时，业务层代码 0 修改，展现出极佳的架构扩展性。

### 🖥️ Slide 2: 协作式非阻塞状态机与确定性调度
*   **消除传统阻塞式等待**：针对 AHT20 传感器 80ms 的物理 ADC 延迟，设计 non-blocking 采样状态机，转换期间 CPU 释放率达 100%。
*   **时间切片标志表调度**：废弃 `HAL_Delay` 轮询模式，重构为 SysTick (1ms 时基) 驱动的任务调度表，保证 320x480 彩屏 UI 曲线流畅刷新与物理按键的即时响应。

### 🖥️ Slide 3: 工业级警报冗余与自适应触摸校准
*   **双重告警静音与安全冗余**：S1 键确认告警后 LCD 横幅消除（静音），但底板状态 LED 保持快速双闪，兼顾“降噪”与“视觉安全冗余”，防止隐患遗忘。
*   **开机四点标定消除机械漂移**：实现自适应 2D 空间仿射变换矩阵拟合，开机按住 S0 键即可动态校准电阻屏，实现精准的像素级触控。

### 🖥️ Slide 4: 现代开发流与多维工具协同
*   **脱离 Keil 的现代构建链**：使用 CMake/Ninja 增量构建，配合静态 RAM/Flash 内存占比分析插件，实时把控空间占用，规避堆栈溢出。
*   **多维 AI 智能 Agent 结对**：联合使用 OpenAI Codex、Claude Code 以及 Antigravity，涵盖算法编写、Git 冲突解决及工程报告生成的全流程。
*   **Python 上位机与点阵工具**：编写 Python 辅助脚本，捕获 picocom 串口数据流并在 PC 端绘图验证；编写字模转换脚本，批量生成 LCD 液晶屏英文字体与 ASCII 点阵。

---
*本报告由 Antigravity 整理输出，旨在为小组成员提供清晰的系统硬件与软件全貌参考。*
