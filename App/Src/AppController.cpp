// App/Src/AppController.cpp
#include "AppController.hpp"
#include "stm32f1xx_hal.h"
#include <stdlib.h>

namespace App {

AppController::AppController(ITempHumSensor& th, IPressureSensor& press, IIndicator& led, IButton& keyPage, IButton& keyConfirm, IButton& keyBack, ILcdDisplay& lcd, ITouch& touch)
    : m_th(th)
    , m_press(press)
    , m_led(led)
    , m_keyPage(keyPage)
    , m_keyConfirm(keyConfirm)
    , m_keyBack(keyBack)
    , m_lcd(lcd)
    , m_touch(touch)
{
}

void AppController::setup()
{
    SYS_LOG("Initializing AppController...");
    
    // 初始化指示灯
    m_led.turnOff();

    // 初始化 LCD
    m_lcd.init();
    
    // 自检温湿传感器
    if (m_th.init() == Sys::Status::OK) {
        m_tempHumConnected = true;
        SYS_LOG("ITempHumSensor initialized successfully.");
    } else {
        m_tempHumConnected = false;
        SYS_LOG("ITempHumSensor initialization failed!");
    }

    // 自检气压传感器
    if (m_press.init() == Sys::Status::OK) {
        m_pressureConnected = true;
        SYS_LOG("IPressureSensor initialized successfully.");
    } else {
        m_pressureConnected = false;
        SYS_LOG("IPressureSensor initialization failed!");
    }
    
    // 设置初始指示灯为正常呼吸效果
    m_led.setNormalBreathing();
    m_data.alarmState = Sys::AlarmState::NORMAL;
    m_data.currentViewPage = 0;
    m_data.isMuted = false;
    m_touchToggleRequested = false;
    m_touchObservedPressed = false;
    m_tempHumSampleActive = false;
}

void AppController::run()
{
    const uint32_t nowMs = HAL_GetTick();
    updateLed(10U);
    scanKeys();
    pollTouch();
    processInputs();
    startSensorSample(nowMs);
    stepSensors(nowMs);
    updateStateMachine();
    refreshDisplay();
}

void AppController::updateLed(uint32_t elapsedMs)
{
    m_led.updatePhysics(elapsedMs);
}

void AppController::scanKeys()
{
    m_keyPage.scanTick();
    m_keyConfirm.scanTick();
    m_keyBack.scanTick();
}

void AppController::refreshDisplay()
{
    ILcdDisplay::RenderData renderData {
        m_data.temperature,
        m_data.humidity,
        m_data.pressure,
        m_data.altitude,
        m_data.tempHighLimit,
        m_data.tempLowLimit,
        m_data.pressHighLimit,
        m_data.pressLowLimit,
        m_data.alarmState,
        m_data.currentViewPage,
        m_data.isMuted,
        m_tempHumConnected,
        m_pressureConnected
    };
    m_lcd.update(renderData);
}

void AppController::startSensorSample(uint32_t nowMs)
{
    if (!m_tempHumSampleActive) {
        const bool wasConnected = m_tempHumConnected;
        const Sys::Status tempStatus = m_th.beginSample(nowMs);
        if (tempStatus == Sys::Status::OK || tempStatus == Sys::Status::ERROR_BUSY) {
            m_tempHumSampleActive = true;
            if (!wasConnected) {
                SYS_LOG("AHT20 communication recovered. Sampling resumed.");
            }
        } else {
            m_tempHumConnected = false;
            if (wasConnected) {
                SYS_LOG("AHT20 communication lost. beginSample status=%d", static_cast<int>(tempStatus));
            }
        }
    }

    const bool wasPressureConnected = m_pressureConnected;
    uint32_t press{0};
    int32_t alt{0};
    const Sys::Status pressStatus = m_press.read(press, alt);
    if (pressStatus == Sys::Status::OK) {
        m_data.pressure = press;
        m_data.altitude = alt;
        m_pressureConnected = true;
        if (!wasPressureConnected) {
            SYS_LOG("BMP280 data recovered. Pressure=%lu Pa Altitude=%ld.%ld m",
                    (unsigned long)m_data.pressure,
                    (long)(m_data.altitude / 10),
                    (long)abs(m_data.altitude % 10));
        }
    } else {
        m_pressureConnected = false;
        if (wasPressureConnected) {
            SYS_LOG("BMP280 communication lost. read status=%d", static_cast<int>(pressStatus));
        }
    }
}

void AppController::stepSensors(uint32_t nowMs)
{
    if (!m_tempHumSampleActive) {
        return;
    }

    int32_t temp{0};
    int32_t hum{0};
    const bool wasConnected = m_tempHumConnected;
    const Sys::Status tempStatus = m_th.pollSample(nowMs, temp, hum);
    if (tempStatus == Sys::Status::OK) {
        m_data.temperature = temp;
        m_data.humidity = hum;
        m_tempHumConnected = true;
        m_tempHumSampleActive = false;
        if (!wasConnected) {
            SYS_LOG("AHT20 data recovered. Temperature=%ld.%ld C Humidity=%ld.%ld %%",
                    (long)(m_data.temperature / 10),
                    (long)abs(m_data.temperature % 10),
                    (long)(m_data.humidity / 10),
                    (long)abs(m_data.humidity % 10));
        }
    } else if (tempStatus != Sys::Status::ERROR_BUSY) {
        m_tempHumConnected = false;
        m_tempHumSampleActive = false;
        if (wasConnected) {
            SYS_LOG("AHT20 communication lost. pollSample status=%d", static_cast<int>(tempStatus));
        }
    }
}

void AppController::pollTouch()
{
    const bool touched = m_touch.isTouched();
    if (touched) {
        if (!m_touchPressLogged) {
            SYS_LOG("Touch pressed.");
            m_touchPressLogged = true;
        }
        m_touchObservedPressed = true;
        return;
    }

    if (m_touchObservedPressed && !m_touchToggleRequested) {
        m_touchObservedPressed = false;
        m_touchPressLogged = false;
        m_touchToggleRequested = true;
        SYS_LOG("Touch released. Source=polling fallback.");
    } else if (!m_touchObservedPressed) {
        m_touchPressLogged = false;
    }
}

void AppController::requestTouchToggle()
{
    m_touchObservedPressed = false;
    m_touchPressLogged = false;
    m_touchToggleRequested = true;
    SYS_LOG("Touch release event queued from IRQ.");
}

void AppController::processInputs()
{
    // KEY1 用于切换 LCD 显示页 (0: 温湿屏, 1: 气压海拔屏)。
    // 当前硬件映射中，该输入由底板 S0 经 FPGA 回送到 STM32 的 KEY_PAGE。
    if (m_keyPage.isPressed()) {
        m_data.currentViewPage = (m_data.currentViewPage == 0) ? 1 : 0;
        SYS_LOG("Page button pressed. Switched LCD to page %d", m_data.currentViewPage);
    }
    
    if (m_touchToggleRequested) {
        m_touchToggleRequested = false;
        m_data.currentViewPage = (m_data.currentViewPage == 0) ? 1 : 0;
        SYS_LOG("Touch release: switched to page %d", m_data.currentViewPage);
    }

    // KEY2 用于确认/抑制当前报警状态，不再代表声音相关的静音硬件。
    if (m_keyConfirm.isPressed()) {
        if (m_data.alarmState == Sys::AlarmState::WARNING_TEMP || 
            m_data.alarmState == Sys::AlarmState::WARNING_PRES) {
            m_data.isMuted = true;
            m_data.alarmState = Sys::AlarmState::MUTED;
            SYS_LOG("Confirm button pressed. Alarm indication suppressed.");
        } else if (m_data.alarmState == Sys::AlarmState::MUTED) {
            m_data.isMuted = false;
            SYS_LOG("Confirm button pressed. Alarm indication restored.");
        }
    }

    // KEY3 用于返回默认页面，并在已确认状态下允许恢复到未确认的告警展示。
    if (m_keyBack.isPressed()) {
        const bool pageChanged = (m_data.currentViewPage != 0);
        m_data.currentViewPage = 0;

        if (m_data.alarmState == Sys::AlarmState::MUTED) {
            m_data.isMuted = false;
            SYS_LOG("Back button pressed. Returned to page 0 and restored alarm indication.");
        } else if (pageChanged) {
            SYS_LOG("Back button pressed. Returned to page 0.");
        } else {
            SYS_LOG("Back button pressed.");
        }
    }
}

void AppController::logHealth()
{
    SYS_LOG("Health: page=%d tempConn=%d pressConn=%d alarm=%d muted=%d",
            m_data.currentViewPage,
            m_tempHumConnected ? 1 : 0,
            m_pressureConnected ? 1 : 0,
            static_cast<int>(m_data.alarmState),
            m_data.isMuted ? 1 : 0);
}

void AppController::updateStateMachine()
{
    // 报警上下限判定条件
    bool tempAbnormal = (m_data.temperature > m_data.tempHighLimit) || (m_data.temperature < m_data.tempLowLimit);
    bool pressAbnormal = (m_data.pressure > m_data.pressHighLimit) || (m_data.pressure < m_data.pressLowLimit);

    // 状态自愈：若当前没有任何异常，重置状态为正常，并解除已确认状态
    if (!tempAbnormal && !pressAbnormal) {
        if (m_data.alarmState != Sys::AlarmState::NORMAL) {
            m_data.alarmState = Sys::AlarmState::NORMAL;
            m_data.isMuted = false;
            m_led.setNormalBreathing();
            SYS_LOG("Environment recovered. Back to STATE_NORMAL.");
        }
        return;
    }

    // 若已经处于已确认状态，保持抑制展示状态，除非环境恢复正常（上面已处理）
    if (m_data.isMuted) {
        m_data.alarmState = Sys::AlarmState::MUTED;
        // 在已确认状态下，LED 仍然保持高频闪烁作为无声安全警示。
        m_led.setWarningBlinking();
        return;
    }

    // 状态转移与报警判定
    if (tempAbnormal) {
        if (m_data.alarmState != Sys::AlarmState::WARNING_TEMP) {
            m_data.alarmState = Sys::AlarmState::WARNING_TEMP;
            m_led.setWarningBlinking();
            SYS_LOG("Alarm Triggered: Temperature abnormal! Temp: %ld.%ld C", (long)(m_data.temperature / 10), (long)abs(m_data.temperature % 10));
        }
    } else if (pressAbnormal) {
        if (m_data.alarmState != Sys::AlarmState::WARNING_PRES) {
            m_data.alarmState = Sys::AlarmState::WARNING_PRES;
            m_led.setWarningBlinking();
            SYS_LOG("Alarm Triggered: Atmospheric pressure abnormal! Press: %lu Pa", (unsigned long)m_data.pressure);
        }
    }
}

void AppController::setTempLimits(int32_t high, int32_t low)
{
    m_data.tempHighLimit = high;
    m_data.tempLowLimit = low;
    SYS_LOG("Updated Temperature limits: High: %ld.%ld C, Low: %ld.%ld C", 
            (long)(high / 10), (long)abs(high % 10), 
            (long)(low / 10), (long)abs(low % 10));
}

void AppController::setPressureLimits(uint32_t high, uint32_t low)
{
    m_data.pressHighLimit = high;
    m_data.pressLowLimit = low;
    SYS_LOG("Updated Pressure limits: High: %lu Pa, Low: %lu Pa", (unsigned long)high, (unsigned long)low);
}

} // namespace App
