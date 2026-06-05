// App/Src/app_entry.cpp
#include "app_entry.h"
#include "main.h"
#include "sys.hpp"
#include "SoftI2cBsp.hpp"
#include "Aht20Bsp.hpp"
#include "Bmp280Bsp.hpp"
#include "PwmLedBsp.hpp"
#include "ButtonBsp.hpp"
#include "LcdBsp.hpp"
#include "GuiEngine.hpp"
#include "TouchBsp.hpp"
#include "AppController.hpp"

// 外部硬件定时器与 SPI 句柄声明，由 CubeMX 在 main.c / tim.c / spi.c 中生成
extern TIM_HandleTypeDef htim3;
extern SPI_HandleTypeDef hspi1;

// ---- 静态局部对象持久化实例化 (避免动态堆分配) ----

// 1. 软件 I2C 总线，绑定 PB10=SCL, PB11=SDA (CubeMX I2C2 引脚)
static Bsp::SoftI2cBsp g_I2cBus(GPIOB, GPIO_PIN_10, GPIOB, GPIO_PIN_11);

// 2. 挂载传感器，挂在同个 I2C 总线上
static Bsp::Aht20Bsp   g_Aht20(g_I2cBus);
static Bsp::Bmp280Bsp  g_Bmp280(g_I2cBus);

// 3. 挂载 PWM 呼吸指示灯，绑定 TIM3 通道 1 (CubeMX partial remap 后为 PB4)
static Bsp::PwmLedBsp  g_LedIndicator(&htim3, TIM_CHANNEL_1);

// 4. 挂载交互按键：KEY1 (PA0) 翻页，KEY2 (PA1) 静音
static Bsp::ButtonBsp  g_KeyPage(GPIOA, GPIO_PIN_0);
static Bsp::ButtonBsp  g_KeyMute(GPIOA, GPIO_PIN_1);

// 5. 挂载 LCD 调试屏幕 (SPI1 接口，参考工程接线：CS=PB9, DC=PB7, RST=PB8, LED=PB6)
static Bsp::LcdBsp     g_Lcd(&hspi1,
                             LCD_CS_GPIO_Port,  LCD_CS_Pin,
                             LCD_DC_GPIO_Port,  LCD_DC_Pin,
                             LCD_RST_GPIO_Port, LCD_RST_Pin,
                             LCD_LED_GPIO_Port, LCD_LED_Pin);

// 6. 实例化应用核心业务控制器，采用构造函数依赖注入
static Bsp::TouchBsp  g_Touch(GPIOA, GPIO_PIN_8,   // TCLK
	                              GPIOB, GPIO_PIN_1,   // TCS
	                              GPIOB, GPIO_PIN_0,   // TDIN
	                              GPIOB, GPIO_PIN_12,  // TDOUT
	                              GPIOA, GPIO_PIN_4);  // PENIRQ

	// 7. GuiEngine (pure geometry on ILcdDisplay)
	static Bsp::GuiEngine g_Gui(g_Lcd);

	// 8. AppController with touch injection
	static App::AppController g_App(g_Aht20, g_Bmp280, g_LedIndicator, g_KeyPage, g_KeyMute, g_Lcd, g_Touch);

// ---- C 语言接口包装实现 ----

void App_Init(void)
{
    SYS_LOG("Booting C++ application entry point...");
    
    // 初始化 I2C 物理引脚和总线模式
    g_I2cBus.init();

    // 注入 GuiEngine 到 LcdBsp
    g_Lcd.setGui(&g_Gui);

    // 初始化触摸屏
    g_Touch.init();

    // 初始化主状态机与各传感器驱动
    g_App.setup();
    
    SYS_LOG("C++ application initialized successfully.");
}

void App_Loop(void)
{
    // 主循环轮询状态机
    g_App.run();
}

void App_Timer_10ms_ISR(void)
{
    // 1. 驱动 LED 呼吸/闪烁指示灯的物理周期计算 (步长为 10ms)
    g_LedIndicator.updatePhysics(10);
    
    // 2. 执行按键物理信号扫描消抖推进
    g_KeyPage.scanTick();
    g_KeyMute.scanTick();
}
