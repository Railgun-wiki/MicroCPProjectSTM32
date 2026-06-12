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

namespace {

struct AppTaskFlags {
    volatile uint8_t ledUpdate{0};
    volatile uint8_t keyScan{0};
    volatile uint8_t inputProcess{0};
    volatile uint8_t touchEvent{0};
    volatile uint8_t touchPoll{0};
    volatile uint8_t sensorStart{0};
    volatile uint8_t sensorStep{0};
    volatile uint8_t stateUpdate{0};
    volatile uint8_t uiRefresh{0};
    volatile uint8_t healthLog{0};
};

struct AppTaskSnapshot {
    uint8_t ledUpdate;
    uint8_t keyScan;
    uint8_t inputProcess;
    uint8_t touchEvent;
    uint8_t touchPoll;
    uint8_t sensorStart;
    uint8_t sensorStep;
    uint8_t stateUpdate;
    uint8_t uiRefresh;
    uint8_t healthLog;
};

AppTaskFlags g_taskFlags;
volatile uint8_t g_touchIrqPending = 0;

} // namespace

void App_Init(void)
{
    SYS_LOG("Booting C++ application entry point...");

    g_I2cBus.init();
    g_Lcd.setGui(&g_Gui);
    const bool touchReady = g_Touch.init();
    SYS_LOG("Touch BSP init: %s", touchReady ? "ready" : "failed");

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
    SYS_LOG("Boot scan summary: group=%lu aht20=%s bmp280=%s",
            (unsigned long)SYS_GROUP_NUMBER,
            aht20Connected ? "connected" : "missing",
            bmp280Connected ? "connected" : "missing");

    char lineBuf[48];
    sprintf(lineBuf, "GROUP: %lu", (unsigned long)SYS_GROUP_NUMBER);
    g_Lcd.drawString(20, 85, lineBuf, 0x07E0, 0x0000, 16);
    sprintf(lineBuf, "AHT20: %s", aht20Connected ? "Connected" : "Unconnected");
    g_Lcd.drawString(20, 120, lineBuf, aht20Connected ? 0x07E0 : 0xF800, 0x0000, 16);
    sprintf(lineBuf, "BMP280: %s", bmp280Connected ? "Connected" : "Unconnected");
    g_Lcd.drawString(20, 150, lineBuf, bmp280Connected ? 0x07E0 : 0xF800, 0x0000, 16);
    HAL_Delay(2000); // Show for 2 seconds

    g_App.setup();
    g_taskFlags.sensorStart = 1;
    g_taskFlags.sensorStep = 1;
    g_taskFlags.stateUpdate = 1;
    g_taskFlags.uiRefresh = 1;
    g_taskFlags.healthLog = 1;

    SYS_LOG("C++ application initialized successfully.");
}

void App_Loop(void)
{
    AppTaskSnapshot snapshot{0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
    Sys::EnterCritical();
    snapshot.ledUpdate = g_taskFlags.ledUpdate;
    snapshot.keyScan = g_taskFlags.keyScan;
    snapshot.inputProcess = g_taskFlags.inputProcess;
    snapshot.touchEvent = g_taskFlags.touchEvent;
    snapshot.touchPoll = g_taskFlags.touchPoll;
    snapshot.sensorStart = g_taskFlags.sensorStart;
    snapshot.sensorStep = g_taskFlags.sensorStep;
    snapshot.stateUpdate = g_taskFlags.stateUpdate;
    snapshot.uiRefresh = g_taskFlags.uiRefresh;
    snapshot.healthLog = g_taskFlags.healthLog;

    g_taskFlags.ledUpdate = 0;
    g_taskFlags.keyScan = 0;
    g_taskFlags.inputProcess = 0;
    g_taskFlags.touchEvent = 0;
    g_taskFlags.touchPoll = 0;
    g_taskFlags.sensorStart = 0;
    g_taskFlags.sensorStep = 0;
    g_taskFlags.stateUpdate = 0;
    g_taskFlags.uiRefresh = 0;
    g_taskFlags.healthLog = 0;
    Sys::ExitCritical();

    const uint32_t nowMs = HAL_GetTick();

    if (snapshot.ledUpdate) {
        g_App.updateLed(10U);
    }
    if (snapshot.keyScan) {
        g_App.scanKeys();
    }
    if (snapshot.touchEvent) {
        g_App.requestTouchToggle();
    }
    if (snapshot.touchPoll) {
        g_App.pollTouch();
    }
    if (snapshot.inputProcess) {
        g_App.processInputs();
    }
    if (snapshot.sensorStart) {
        g_App.startSensorSample(nowMs);
    }
    if (snapshot.sensorStep) {
        g_App.stepSensors(nowMs);
    }
    if (snapshot.stateUpdate) {
        g_App.updateStateMachine();
    }
    if (snapshot.uiRefresh) {
        g_App.refreshDisplay();
    }
    if (snapshot.healthLog) {
        g_App.logHealth();
    }
}

void App_Timer_10ms_ISR(void)
{
    static uint32_t inputDivider = 0;
    static uint32_t touchDivider = 0;
    static uint32_t sensorStartDivider = 0;
    static uint32_t stateDivider = 0;
    static uint32_t uiDivider = 0;
    static uint32_t healthDivider = 0;

    g_taskFlags.ledUpdate = 1;
    g_taskFlags.keyScan = 1;
    g_taskFlags.sensorStep = 1;

    if (++inputDivider >= 2U) {
        inputDivider = 0;
        g_taskFlags.inputProcess = 1;
    }
    if (g_touchIrqPending != 0U) {
        g_touchIrqPending = 0U;
        g_taskFlags.touchEvent = 1;
    }
    if (++touchDivider >= 5U) {
        touchDivider = 0;
        g_taskFlags.touchPoll = 1;
    }
    if (++sensorStartDivider >= 50U) {
        sensorStartDivider = 0;
        g_taskFlags.sensorStart = 1;
    }
    if (++stateDivider >= 10U) {
        stateDivider = 0;
        g_taskFlags.stateUpdate = 1;
    }
    if (++uiDivider >= 10U) {
        uiDivider = 0;
        g_taskFlags.uiRefresh = 1;
    }
    if (++healthDivider >= 100U) {
        healthDivider = 0;
        g_taskFlags.healthLog = 1;
    }
}

void App_TouchPen_ISR(void)
{
    g_touchIrqPending = 1U;
}
