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
#include "TouchBsp.hpp"
#include "GuiEngine.hpp"
#include "AppController.hpp"
#include "AppGui.hpp"
#include <stdlib.h>

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

static App::GuiEngine g_Gui(g_Lcd);

static App::AppGui g_AppGui(g_Lcd);
static App::AppController g_App(g_Aht20, g_Bmp280, g_LedIndicator,
                                g_KeyPage, g_KeyConfirm, g_KeyBack, g_AppGui, g_Touch);

namespace {

struct AppTaskFlags {
    volatile uint8_t ledUpdate{0};
    volatile uint8_t keyScan{0};
    volatile uint8_t inputProcess{0};
    volatile uint8_t touchPoll{0};
    volatile uint8_t sensorStart{0};
    volatile uint8_t sensorStep{0};
    volatile uint8_t stateUpdate{0};
    volatile uint8_t uiRefresh{0};
    volatile uint8_t guiRender{0};
    volatile uint8_t healthLog{0};
};

struct AppTaskSnapshot {
    uint8_t ledUpdate;
    uint8_t keyScan;
    uint8_t inputProcess;
    uint8_t touchPoll;
    uint8_t sensorStart;
    uint8_t sensorStep;
    uint8_t stateUpdate;
    uint8_t uiRefresh;
    uint8_t guiRender;
    uint8_t healthLog;
};

AppTaskFlags g_taskFlags;

} // namespace

static void runTouchCalibration()
{
    g_Lcd.clear(0xFFFF); // Clear to white
    
    // Draw instructions
    g_Lcd.drawString(20, 100, "Touch Screen Calibration", 0xF800, 0xFFFF, 16);
    g_Lcd.drawString(20, 130, "Please click the red targets in sequence.", 0x0000, 0xFFFF, 12);
    g_Lcd.drawString(20, 160, "Use a stylus for better accuracy.", 0x0000, 0xFFFF, 12);

    uint16_t targets[4][2] = {
        {20, 20},
        {460, 20},
        {20, 300},
        {460, 300}
    };
    uint16_t raw_coords[4][2] = {0};

    auto drawTarget = [](uint16_t tx, uint16_t ty, uint16_t color) {
        g_Gui.drawLine(tx - 12, ty, tx + 12, ty, color);
        g_Gui.drawLine(tx, ty - 12, tx, ty + 12, color);
        g_Gui.drawCircle(tx, ty, 6, color, false);
    };

    for (int i = 0; i < 4; ++i) {
        drawTarget(targets[i][0], targets[i][1], 0xF800); // Draw in red

        // Wait for touch press
        while (!g_Touch.isTouched()) {
            HAL_Delay(10);
        }

        // Read raw coordinates while touched
        uint32_t sumX = 0, sumY = 0;
        int samples = 0;
        while (g_Touch.isTouched()) {
            uint16_t rx = 0, ry = 0;
            if (g_Touch.readRaw(rx, ry)) {
                sumX += rx;
                sumY += ry;
                samples++;
            }
            HAL_Delay(20);
        }

        if (samples > 0) {
            raw_coords[i][0] = sumX / samples;
            raw_coords[i][1] = sumY / samples;
        }

        drawTarget(targets[i][0], targets[i][1], 0xFFFF); // Erase in white
        HAL_Delay(500); // Debounce delay
    }

    int32_t x_left = (raw_coords[0][0] + raw_coords[2][0]) / 2;
    int32_t x_right = (raw_coords[1][0] + raw_coords[3][0]) / 2;
    int32_t y_top = (raw_coords[0][1] + raw_coords[1][1]) / 2;
    int32_t y_bottom = (raw_coords[2][1] + raw_coords[3][1]) / 2;

    int16_t xMin = 0, xMax = 0, yMin = 0, yMax = 0;

    if (abs(x_right - x_left) < 100 || abs(y_bottom - y_top) < 100) {
        xMin = 300; xMax = 3900;
        yMin = 200; yMax = 3800;
        g_Touch.setCalibration(xMin, xMax, yMin, yMax);
        g_Lcd.clear(0xFFFF);
        g_Lcd.drawString(20, 120, "Calibration Failed!", 0xF800, 0xFFFF, 16);
        g_Lcd.drawString(20, 150, "Using default parameters.", 0x0000, 0xFFFF, 12);
        HAL_Delay(2000);
    } else {
        xMin = x_left - 20 * (x_right - x_left) / 440;
        xMax = x_right + 20 * (x_right - x_left) / 440;
        yMin = y_top - 20 * (y_bottom - y_top) / 280;
        yMax = y_bottom + 20 * (y_bottom - y_top) / 280;

        g_Touch.setCalibration(xMin, xMax, yMin, yMax);
        
        g_Lcd.clear(0xFFFF);
        g_Lcd.drawString(20, 120, "Calibration Successful!", 0x07E0, 0xFFFF, 16);
        char buf[64];
        sprintf(buf, "X Range: [%d, %d]", xMin, xMax);
        g_Lcd.drawString(20, 150, buf, 0x0000, 0xFFFF, 12);
        sprintf(buf, "Y Range: [%d, %d]", yMin, yMax);
        g_Lcd.drawString(20, 170, buf, 0x0000, 0xFFFF, 12);
        HAL_Delay(3000);
    }

    g_Lcd.clear(0xFFFF);
}

void App_Init(void)
{
    SYS_LOG("Booting C++ application entry point...");

    g_I2cBus.init();
    const bool touchReady = g_Touch.init();
    SYS_LOG("Touch BSP init: %s", touchReady ? "ready" : "failed");

    // Diagnostic I2C scanner on boot
    g_Lcd.init();

    if (touchReady) {
        if (HAL_GPIO_ReadPin(KEY_PAGE_GPIO_Port, KEY_PAGE_Pin) == GPIO_PIN_RESET) {
            runTouchCalibration();
        } else {
            g_Touch.setCalibration(314, 1976, 62, 1815);
        }
    }

    bool aht20Connected = false;
    bool bmp280Connected = false;
    SYS_LOG("Scanning I2C bus...");
    for (uint16_t i = 1; i < 128; i++) {
        if (HAL_I2C_IsDeviceReady(&hi2c2, i << 1, 1, 2) == HAL_OK) {
            SYS_LOG("  Found I2C device at address 0x%02X", i);
            if (i == SYS_I2C_ADDR_AHT20) {
                aht20Connected = true;
            }
            if (i == SYS_I2C_ADDR_BMP280 || i == 0x77) {
                bmp280Connected = true;
            }
        }
    }
    SYS_LOG("Boot scan summary: group=%lu aht20=%s bmp280=%s",
            (unsigned long)SYS_GROUP_NUMBER,
            aht20Connected ? "connected" : "missing",
            bmp280Connected ? "connected" : "missing");

    g_AppGui.renderBootScan(SYS_GROUP_NUMBER, aht20Connected, bmp280Connected);
    HAL_Delay(2000); // Show for 2 seconds

    g_App.setup();
    g_taskFlags.sensorStart = 1;
    g_taskFlags.sensorStep = 1;
    g_taskFlags.stateUpdate = 1;
    g_taskFlags.uiRefresh = 1;
    g_taskFlags.guiRender = 1;
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
    snapshot.touchPoll = g_taskFlags.touchPoll;
    snapshot.sensorStart = g_taskFlags.sensorStart;
    snapshot.sensorStep = g_taskFlags.sensorStep;
    snapshot.stateUpdate = g_taskFlags.stateUpdate;
    snapshot.uiRefresh = g_taskFlags.uiRefresh;
    snapshot.guiRender = g_taskFlags.guiRender;
    snapshot.healthLog = g_taskFlags.healthLog;

    g_taskFlags.ledUpdate = 0;
    g_taskFlags.keyScan = 0;
    g_taskFlags.inputProcess = 0;
    g_taskFlags.touchPoll = 0;
    g_taskFlags.sensorStart = 0;
    g_taskFlags.sensorStep = 0;
    g_taskFlags.stateUpdate = 0;
    g_taskFlags.uiRefresh = 0;
    g_taskFlags.guiRender = 0;
    g_taskFlags.healthLog = 0;
    Sys::ExitCritical();

    const uint32_t nowMs = HAL_GetTick();

    if (snapshot.ledUpdate) {
        g_App.updateLed(10U);
    }
    if (snapshot.keyScan) {
        g_App.scanKeys();
    }
    if (snapshot.touchPoll) {
        g_App.pollTouch(nowMs);
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
    if (snapshot.guiRender) {
        g_App.renderGuiTick();
    }
    if (snapshot.healthLog) {
        g_App.logHealth();
    }
}

void App_Timer_10ms_ISR(void)
{
    static uint32_t inputDivider = 0;
    static uint32_t sensorStartDivider = 0;
    static uint32_t stateDivider = 0;
    static uint32_t uiDivider = 0;
    static uint32_t healthDivider = 0;

    g_taskFlags.ledUpdate = 1;
    g_taskFlags.keyScan = 1;
    g_taskFlags.sensorStep = 1;
    g_taskFlags.guiRender = 1;
    g_taskFlags.touchPoll = 1;

    if (++inputDivider >= 2U) {
        inputDivider = 0;
        g_taskFlags.inputProcess = 1;
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
    // Touch is sampled by the 10ms polling task; no IRQ-side work is required.
}
