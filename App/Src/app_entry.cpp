// App/Src/app_entry.cpp
#include "app_entry.h"
#include "main.h"
#include "sys.hpp"
#include "LcdBsp.hpp"
#include "GuiEngine.hpp"
#include "TouchBsp.hpp"
#include "Lab16LcdDemo.hpp"

// 外部硬件定时器与 SPI 句柄声明，由 CubeMX 在 main.c / tim.c / spi.c 中生成
extern SPI_HandleTypeDef hspi1;

// ---- 静态局部对象持久化实例化 (避免动态堆分配) ----

// 1. 挂载 LCD 调试屏幕 (SPI1 接口，参考工程接线：CS=PB9, DC=PB7, RST=PB8, LED=PB6)
static Bsp::LcdBsp     g_Lcd(&hspi1,
                             LCD_CS_GPIO_Port,  LCD_CS_Pin,
                             LCD_DC_GPIO_Port,  LCD_DC_Pin,
                             LCD_RST_GPIO_Port, LCD_RST_Pin,
                             LCD_LED_GPIO_Port, LCD_LED_Pin);

// 2. LAB16 触摸屏接线：TCLK=PA8, TCS=PB4, TDIN=PB3, DOUT=PA1, PEN=PA0
static Bsp::TouchBsp   g_Touch(TOUCH_TCLK_GPIO_Port, TOUCH_TCLK_Pin,
                               TOUCH_TCS_GPIO_Port,  TOUCH_TCS_Pin,
                               TOUCH_TDIN_GPIO_Port, TOUCH_TDIN_Pin,
                               TOUCH_DOUT_GPIO_Port, TOUCH_DOUT_Pin,
                               TOUCH_PEN_GPIO_Port,  TOUCH_PEN_Pin);

// 3. 复用现有几何绘图和 LCD/Touch BSP，实现 LAB16 画板例程
static Bsp::GuiEngine  g_Gui(g_Lcd);
static App::Lab16LcdDemo g_Demo(g_Lcd, g_Gui, g_Touch);


// ---- C 语言接口包装实现 ----

void App_Init(void)
{
    SYS_LOG("Booting LAB16 LCD touch demo...");

    g_Lcd.setGui(&g_Gui);
    g_Lcd.init();
    g_Touch.init();
    g_Touch.setCalibration(200, 3900, 200, 3900);
    g_Demo.init();

    SYS_LOG("LAB16 LCD touch demo initialized.");
}

void App_Loop(void)
{
    g_Demo.loop();
}

void App_Timer_10ms_ISR(void)
{
    // LAB16 demo polls touch state in the main loop.
}
