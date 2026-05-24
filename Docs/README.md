# 📚 STM32 C++ 固件库设计文档中心 (MicroCPProjectSTM32 Docs)

欢迎来到 **MicroCPProjectSTM32** 项目的设计与开发文档中心！本中心致力于为团队协作提供全面、专业且易于上手的工程参考资料。

本项目在 **STM32F103** 平台上构建了一套高度复用、松耦合、面向对象的嵌入式系统。通过本中心提供的文档，团队成员（成员 A 与 成员 B）可以零耦合地并行推进底层驱动与上层图形界面的开发。

---

## 🗺️ 文档导航

我们为您准备了以下两份核心设计与开发文档，请点击下方链接直接跳转阅读：

| 文档名称 | 核心内容介绍 | 适用阶段 / 读者 |
| :--- | :--- | :--- |
| [📐 设计与开发规范](file:///home/yuki/Documents/Coding/Project-Micro/MicroCPProjectSTM32/Docs/Specification.md) | C++17 编译器优化配置、免 exceptions & RTTI 规范、目录职责、基于纯虚函数的控制反转（IoC）解耦机制。 | 基础规范 / 全体开发人员 |
| [🔌 API 接口与类参考手册](file:///home/yuki/Documents/Coding/Project-Micro/MicroCPProjectSTM32/Docs/API.md) | 系统全局状态、抽象传感器/外设接口定义、BSP 驱动类（模拟 I2C、AHT20、BMP280、PWM 呼吸灯、按键消抖）详细函数原型、遥测结构体以及 C 语言桥接包装指南。 | 编码实践 / 底层与 UI 开发者 |

---

## 🛠️ 系统软硬件配置概览

为了方便快速布线与验证，以下整理了系统的核心硬件连接与软件构建参数：

### 1. 物理引脚连接表 (Pinout)
| 外设模块 | 外设型号 | STM32 物理引脚 | 配置模式 | 物理作用与连接说明 |
| :--- | :--- | :--- | :--- | :--- |
| **复用 I2C 总线** | **AHT20 + BMP280** | **PB6 (SCL)**<br>**PB7 (SDA)** | 开漏输出 / 浮空输入 | **软件模拟 I2C 总线**。挂载 AHT20 (地址 `0x38`) 与 BMP280 (地址 `0x76`)，有效避免 STM32 硬件 I2C 的死锁缺陷。 |
| **指示指示灯** | **高亮 LED** | **PA6 (TIM3_CH1)** | 复用推挽输出 | 输出高频 PWM（频率约 1kHz）驱动 LED。支持正常平滑呼吸与异常 5Hz 爆闪。 |
| **翻页按键 (KEY1)** | **轻触按键** | **PA0** | 上拉输入 | 检测按键按下，供主循环轮询触发 LCD 的数据显示切换。 |
| **静音按键 (KEY2)** | **轻触按键** | **PA1** | 上拉输入 | 检测按键按下，在系统报警时用于静音消音；非报警时可做恢复。 |

### 2. 软件运行参数 (sys.hpp)
*   **CPU 主频**：`72 MHz` (STM32F103 极限标称主频)
*   **主循环轮询频率**：`10 Hz`（主业务状态机轮询周期 `100 ms`）
*   **LED 动画采样步长**：`10 ms`（在中断服务函数 `App_Timer_10ms_ISR` 中以 `100Hz` 推进物理模拟）
*   **按键消抖时间**：`20 ms`（连续 2 个定时器中断扫描周期检测为低电平即判定有效触发）

---

## ⚙️ 核心架构思想：依赖注入 (DIP)

传统的嵌入式代码直接将硬件底层代码（如 I2C 读写）耦合在业务逻辑（如报警判断）中。本项目彻底杜绝了这种低劣的设计。

我们通过在应用层定义**纯虚类接口**，把控制权反转过来。

```mermaid
graph TD
    subgraph Application Layer (纯 C++, 平台无关)
        AppController[AppController 核心业务控制器]
        ITempHumSensor[ITempHumSensor 抽象温湿接口]
        IPressureSensor[IPressureSensor 抽象气压接口]
        IIndicator[IIndicator 抽象指示灯接口]
        IButton[IButton 抽象按键接口]
    end

    subgraph BSP Layer (硬件适配层, C++)
        Aht20Bsp[Aht20Bsp 驱动实现]
        Bmp280Bsp[Bmp280Bsp 驱动实现]
        PwmLedBsp[PwmLedBsp 动画驱动]
        ButtonBsp[ButtonBsp 消抖驱动]
        SoftI2cBsp[SoftI2cBsp 模拟 I2C]
    end

    %% 应用层依赖抽象
    AppController --> ITempHumSensor
    AppController --> IPressureSensor
    AppController --> IIndicator
    AppController --> IButton

    %% 驱动层实现抽象
    ITempHumSensor -. 实现 .-> Aht20Bsp
    IPressureSensor -. 实现 .-> Bmp280Bsp
    IIndicator -. 实现 .-> PwmLedBsp
    IButton -. 实现 .-> ButtonBsp

    %% 总线复用
    Aht20Bsp --> SoftI2cBsp
    Bmp280Bsp --> SoftI2cBsp
```

*   **业务逻辑（App）是“甲方”**：它只声明我需要什么数据，只依赖这 4 个抽象接口类。
*   **驱动适配（BSP）是“乙方”**：它继承并实现这些纯虚接口，负责底层物理操作。
*   **系统桥接（Entry）是“红娘”**：在 `app_entry.cpp` 中以静态对象实例化所有具体实现，并通过构造函数注入给 `AppController`。

---

## 🏗️ 快速开始：构建与编译说明

本项目采用现代化的 **CMake + Arm GCC** 构建链。您可以通过以下简单的步骤在本地进行编译构建：

### 编译先决条件
1. 安装 **GCC ARM Embedded Toolchain** (确保 `arm-none-eabi-gcc` 已经加入环境变量)。
2. 安装 **CMake** (v3.15 或以上) 和 **Make** 或 **Ninja** 构建工具。

### 构建步骤
打开终端并运行以下命令：

```bash
# 1. 创建并进入 build 构建目录
mkdir build && cd build

# 2. 生成 Makefile
cmake -DCMAKE_TOOLCHAIN_FILE=cmake/stm32_gcc.cmake ..

# 3. 编译生成固件 (支持 ELF, HEX, BIN 格式)
make -j4
```

For Cmake:
```bash
cube-cmake --build PATH_TO_PROJECT/Debug --
```


> [!NOTE]
> 编译完成后，生成的固件位于 `build` 文件夹下，包含 `MicroCPProjectSTM32.elf`、`MicroCPProjectSTM32.hex` 以及 `MicroCPProjectSTM32.bin`，可以直接使用 ST-Link Utility、OpenOCD 或 J-Link 工具烧录至 STM32F103 硬件开发板上。

---

> 💡 **小贴士**：如果您在开发中需要修改某些引脚或添加新的传感器，请牢记我们的设计原则：**修改驱动代码时决不应影响应用层的核心逻辑；反之，修改业务状态机也决不应触及任何具体的寄存器或 HAL 库代码。** 让我们一起维护这套干净、优雅的嵌入式 C++ 架构！
