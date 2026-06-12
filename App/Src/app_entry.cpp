// App/Src/app_entry.cpp
#include "app_entry.h"
#include "main.h"
#include "sys.hpp"
#include "HardwareI2cBsp.hpp"
#include "Aht20Bsp.hpp"
#include "Bmp280Bsp.hpp"
#include "PwmLedBsp.hpp"
#include "ButtonBsp.hpp"
#include "LcdBsp.hpp"
#include "GuiEngine.hpp"
#include "TouchBsp.hpp"
#include "AppController.hpp"

#include <stdio.h>

extern TIM_HandleTypeDef htim3;
extern SPI_HandleTypeDef hspi1;
extern I2C_HandleTypeDef hi2c2;

static Bsp::HardwareI2cBsp g_I2cBus(&hi2c2);

static Bsp::Aht20Bsp  g_Aht20(g_I2cBus);
static Bsp::Bmp280Bsp g_Bmp280(g_I2cBus);

static Bsp::PwmLedBsp g_LedIndicator(&htim3, TIM_CHANNEL_3);

static Bsp::ButtonBsp g_KeyPage(KEY_PAGE_GPIO_Port, KEY_PAGE_Pin);
static Bsp::ButtonBsp g_KeyConfirm(KEY_CONFIRM_GPIO_Port, KEY_CONFIRM_Pin);
static Bsp::ButtonBsp g_KeyBack(KEY_BACK_GPIO_Port, KEY_BACK_Pin);

static Bsp::LcdBsp g_Lcd(&hspi1,
                         LCD_CS_GPIO_Port,  LCD_CS_Pin,
                         LCD_DC_GPIO_Port,  LCD_DC_Pin,
                         LCD_RST_GPIO_Port, LCD_RST_Pin,
                         LCD_LED_GPIO_Port, LCD_LED_Pin);

static Bsp::TouchBsp g_Touch(TOUCH_TCLK_GPIO_Port, TOUCH_TCLK_Pin,
                             TOUCH_TCS_GPIO_Port,  TOUCH_TCS_Pin,
                             TOUCH_TDIN_GPIO_Port, TOUCH_TDIN_Pin,
                             TOUCH_DOUT_GPIO_Port, TOUCH_DOUT_Pin,
                             TOUCH_PEN_GPIO_Port,  TOUCH_PEN_Pin);

static Bsp::GuiEngine g_Gui(g_Lcd);
static App::AppController g_App(g_Aht20, g_Bmp280, g_LedIndicator,
                                g_KeyPage, g_KeyConfirm, g_KeyBack, g_Lcd, g_Touch);

void App_Init(void)
{
    SYS_LOG("Booting C++ application entry point...");

    g_I2cBus.init();
    g_Lcd.setGui(&g_Gui);
    g_Touch.init();

    // Diagnostic I2C scanner on boot
    g_Lcd.init();
    g_Lcd.clear(0x0000); // Clear to Black
    g_Lcd.drawString(20, 30, "MicroCP Sensor Monitor", 0xFFE0, 0x0000, 16);
    g_Lcd.drawString(20, 55, "Scanning I2C2 Bus...", 0xFFFF, 0x0000, 16);

    bool aht20Connected = false;
    bool bmp280Connected = false;
    for (uint16_t i = 1; i < 128; i++) {
        if (HAL_I2C_IsDeviceReady(&hi2c2, i << 1, 1, 10) == HAL_OK) {
            if (i == SYS_I2C_ADDR_AHT20) {
                aht20Connected = true;
            }
            if (i == SYS_I2C_ADDR_BMP280) {
                bmp280Connected = true;
            }
        }
    }

    char lineBuf[48];
    sprintf(lineBuf, "GROUP: %lu", (unsigned long)SYS_GROUP_NUMBER);
    g_Lcd.drawString(20, 85, lineBuf, 0x07E0, 0x0000, 16);
    sprintf(lineBuf, "AHT20: %s", aht20Connected ? "Connected" : "Unconnected");
    g_Lcd.drawString(20, 120, lineBuf, aht20Connected ? 0x07E0 : 0xF800, 0x0000, 16);
    sprintf(lineBuf, "BMP280: %s", bmp280Connected ? "Connected" : "Unconnected");
    g_Lcd.drawString(20, 150, lineBuf, bmp280Connected ? 0x07E0 : 0xF800, 0x0000, 16);
    HAL_Delay(2000); // Show for 2 seconds

    g_App.setup();

    SYS_LOG("C++ application initialized successfully.");
}

void App_Loop(void)
{
    g_App.run();
}

void App_Timer_10ms_ISR(void)
{
    static uint32_t debugTicks = 0;
    static GPIO_PinState lastPageState = GPIO_PIN_SET;
    static GPIO_PinState lastConfirmState = GPIO_PIN_SET;
    static GPIO_PinState lastBackState = GPIO_PIN_SET;

    g_LedIndicator.updatePhysics(10);
    g_KeyPage.scanTick();
    g_KeyConfirm.scanTick();
    g_KeyBack.scanTick();

    const GPIO_PinState pageState = HAL_GPIO_ReadPin(KEY_PAGE_GPIO_Port, KEY_PAGE_Pin);
    const GPIO_PinState confirmState = HAL_GPIO_ReadPin(KEY_CONFIRM_GPIO_Port, KEY_CONFIRM_Pin);
    const GPIO_PinState backState = HAL_GPIO_ReadPin(KEY_BACK_GPIO_Port, KEY_BACK_Pin);

    if (pageState != lastPageState || confirmState != lastConfirmState || backState != lastBackState) {
        SYS_LOG("Key raw state changed: PAGE=%d CONFIRM=%d BACK=%d",
                pageState == GPIO_PIN_SET ? 1 : 0,
                confirmState == GPIO_PIN_SET ? 1 : 0,
                backState == GPIO_PIN_SET ? 1 : 0);
        lastPageState = pageState;
        lastConfirmState = confirmState;
        lastBackState = backState;
    }

    debugTicks++;
    if (debugTicks >= 100) {
        debugTicks = 0;
        SYS_LOG("Heartbeat: PAGE=%d CONFIRM=%d BACK=%d",
                pageState == GPIO_PIN_SET ? 1 : 0,
                confirmState == GPIO_PIN_SET ? 1 : 0,
                backState == GPIO_PIN_SET ? 1 : 0);
    }
}
