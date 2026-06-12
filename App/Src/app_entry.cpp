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

#if APP_LCD_SCREEN_TEST

static Bsp::LcdBsp g_Lcd(&hspi1,
                         LCD_CS_GPIO_Port,  LCD_CS_Pin,
                         LCD_DC_GPIO_Port,  LCD_DC_Pin,
                         LCD_RST_GPIO_Port, LCD_RST_Pin,
                         LCD_LED_GPIO_Port, LCD_LED_Pin);

static Bsp::GuiEngine g_Gui(g_Lcd);

static void drawLcdTestPattern()
{
    const uint16_t w = g_Lcd.getWidth();
    const uint16_t h = g_Lcd.getHeight();
    const uint16_t midX = w / 2;
    const uint16_t midY = h / 2;

    g_Lcd.clear(0x0000);

    const uint16_t topH = h / 2;
    const uint16_t bandW = w / 4;
    g_Lcd.fillRect(0,           0, bandW - 1,     topH - 1, 0xF800);
    g_Lcd.fillRect(bandW,       0, bandW * 2 - 1, topH - 1, 0x07E0);
    g_Lcd.fillRect(bandW * 2,   0, bandW * 3 - 1, topH - 1, 0x001F);
    g_Lcd.fillRect(bandW * 3,   0, w - 1,         topH - 1, 0xFFE0);

    const uint16_t bandH = (h - topH) / 4;
    g_Lcd.fillRect(0, topH,             w - 1, topH + bandH - 1,     0xFFFF);
    g_Lcd.fillRect(0, topH + bandH,     w - 1, topH + bandH * 2 - 1, 0x07FF);
    g_Lcd.fillRect(0, topH + bandH * 2, w - 1, topH + bandH * 3 - 1, 0xF81F);
    g_Lcd.fillRect(0, topH + bandH * 3, w - 1, h - 1,               0x8410);

    g_Lcd.fillRect(0,     0,     w - 1, 2,     0xFFFF);
    g_Lcd.fillRect(0,     h - 3, w - 1, h - 1, 0xFFFF);
    g_Lcd.fillRect(0,     0,     2,     h - 1, 0xFFFF);
    g_Lcd.fillRect(w - 3, 0,     w - 1, h - 1, 0xFFFF);

    g_Lcd.fillRect(midX - 1, 0,        midX + 1, h - 1,    0x0000);
    g_Lcd.fillRect(0,        midY - 1, w - 1,    midY + 1, 0x0000);

    const uint16_t textX = (w > 88) ? static_cast<uint16_t>((w - 88) / 2) : 0;
    const uint16_t textY = (h > 16) ? static_cast<uint16_t>((h - 16) / 2) : 0;
    g_Lcd.drawString(textX, textY, "LCD TEST OK", 0x0000, 0xFFFF, 16);
}

static void flashStartupColors()
{
    constexpr uint16_t colors[] = {0xF800, 0x07E0, 0x001F, 0xFFFF};
    for (uint16_t color : colors) {
        g_Lcd.clear(color);
        HAL_Delay(350);
    }
}

#else

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

#endif

// ---- C 语言接口包装实现 ----

void App_Init(void)
{
#if APP_LCD_SCREEN_TEST
    SYS_LOG("Booting LCD-only screen test...");
    g_Lcd.setGui(&g_Gui);
    g_Lcd.init();
    flashStartupColors();
    drawLcdTestPattern();
    SYS_LOG("LCD-only screen test initialized.");
#else
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
#endif
}

void App_Loop(void)
{
#if APP_LCD_SCREEN_TEST
    static uint32_t lastTick = 0;
    static uint8_t page = 0;
    const uint32_t now = HAL_GetTick();

    if (now - lastTick < 1000) {
        return;
    }
    lastTick = now;

    switch (page) {
    case 0:
        g_Lcd.clear(0xF800);
        break;
    case 1:
        g_Lcd.clear(0x07E0);
        break;
    case 2:
        g_Lcd.clear(0x001F);
        break;
    case 3:
        g_Lcd.clear(0xFFFF);
        break;
    default:
        drawLcdTestPattern();
        break;
    }
    page = static_cast<uint8_t>((page + 1) % 5);
#else
    // 主循环轮询状态机
    g_App.run();
#endif
}

void App_Timer_10ms_ISR(void)
{
#if APP_LCD_SCREEN_TEST
    // LCD-only test mode intentionally leaves background peripherals idle.
#else
    // 1. 驱动 LED 呼吸/闪烁指示灯的物理周期计算 (步长为 10ms)
    g_LedIndicator.updatePhysics(10);
    
    // 2. 执行按键物理信号扫描消抖推进
    g_KeyPage.scanTick();
    g_KeyMute.scanTick();
#endif
}
