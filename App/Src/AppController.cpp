// App/Src/AppController.cpp
#include "AppController.hpp"
#include "stm32f1xx_hal.h"
#include <stdlib.h>

namespace App {

AppController::AppController(ITempHumSensor& th, IPressureSensor& press, IIndicator& led, IButton& keyPage, IButton& keyConfirm, IButton& keyBack, AppGui& gui, ITouch& touch)
    : m_th(th)
    , m_press(press)
    , m_led(led)
    , m_keyPage(keyPage)
    , m_keyConfirm(keyConfirm)
    , m_keyBack(keyBack)
    , m_gui(gui)
    , m_touch(touch)
{
}

void AppController::setup()
{
    SYS_LOG("Initializing AppController...");
    
    // 初始化指示灯
    m_led.turnOff();

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
    m_tempHumSampleActive = false;
    m_previousPage = 0;
    m_pendingTempHighLimit = m_data.tempHighLimit;
    m_pendingTempLowLimit = m_data.tempLowLimit;
    m_pendingPressHighLimit = m_data.pressHighLimit;
    m_pendingPressLowLimit = m_data.pressLowLimit;
    m_selectedThresholdField = 0;
    m_historyCount = 0;
}

void AppController::run()
{
    const uint32_t nowMs = HAL_GetTick();
    updateLed(10U);
    scanKeys();
    pollTouch(nowMs);
    processInputs();
    startSensorSample(nowMs);
    stepSensors(nowMs);
    updateStateMachine();
    refreshDisplay();
    renderGuiTick();
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
    m_gui.setModel(buildGuiModel());
}

void AppController::renderGuiTick()
{
    m_gui.renderTick();
}

void AppController::startSensorSample(uint32_t nowMs)
{
    if (!m_tempHumSampleActive) {
        const bool wasConnected = m_tempHumConnected;
        bool shouldStartTempHum = m_tempHumConnected;
        static uint32_t lastTempHumRetryMs = 0;
        if (!m_tempHumConnected && (nowMs - lastTempHumRetryMs >= 5000U)) {
            shouldStartTempHum = true;
            lastTempHumRetryMs = nowMs;
        }

        if (shouldStartTempHum) {
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
    }

    const bool wasPressureConnected = m_pressureConnected;
    bool shouldReadPressure = m_pressureConnected;
    static uint32_t lastPressureRetryMs = 0;
    if (!m_pressureConnected && (nowMs - lastPressureRetryMs >= 5000U)) {
        shouldReadPressure = true;
        lastPressureRetryMs = nowMs;
    }

    if (shouldReadPressure) {
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

    if (m_pressureConnected && !m_tempHumSampleActive) {
        appendHistory(m_data.temperature, m_data.pressure);
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
        appendHistory(m_data.temperature, m_data.pressure);
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

void AppController::pollTouch(uint32_t nowMs)
{
    m_touch.scanTick(nowMs);

    TouchEvent event{};
    while (m_touch.popEvent(event)) {
        if (event.type == TouchEvent::Type::Tap && event.point.valid) {
            handleGuiCommand(m_gui.hitTest(event.point));
        }
    }
}

void AppController::processInputs()
{
    // KEY1 用于切换 LCD 显示页 (0: 温湿屏, 1: 气压海拔屏)。
    // 当前硬件映射中，该输入由底板 S0 经 FPGA 回送到 STM32 的 KEY_PAGE。
    if (m_keyPage.isPressed()) {
        if (m_data.currentViewPage != 2) {
            m_data.currentViewPage = (m_data.currentViewPage == 0) ? 1 : 0;
            SYS_LOG("Page button pressed. Switched LCD to page %d", m_data.currentViewPage);
        } else {
            SYS_LOG("Page button ignored on threshold settings page.");
        }
    }
    
    // KEY2 用于确认/抑制当前报警状态，不再代表声音相关的静音硬件。
    if (m_keyConfirm.isPressed()) {
        if (m_data.currentViewPage == 2) {
            applyPendingThresholds();
            SYS_LOG("Confirm button pressed. Threshold changes applied.");
        } else {
            m_data.isMuted = !m_data.isMuted;
            if (m_data.isMuted) {
                if (m_data.alarmState == Sys::AlarmState::WARNING_TEMP ||
                    m_data.alarmState == Sys::AlarmState::WARNING_PRES) {
                    m_data.alarmState = Sys::AlarmState::MUTED;
                }
                SYS_LOG("Confirm button pressed. MUTE is ON.");
            } else {
                if (m_data.alarmState == Sys::AlarmState::MUTED) {
                    m_data.alarmState = Sys::AlarmState::NORMAL;
                }
                SYS_LOG("Confirm button pressed. MUTE is OFF.");
            }
        }
    }

    // KEY3 (KEY_BACK) 用于进入/退出 Settings 页面。
    if (m_keyBack.isPressed()) {
        if (m_data.currentViewPage == 2) {
            cancelPendingThresholds();
            SYS_LOG("Back button pressed. Exited settings page (changes canceled).");
        } else {
            enterThresholdPage();
            SYS_LOG("Back button pressed. Entered settings page.");
        }
    }
}

AppGui::Model AppController::buildGuiModel() const
{
    AppGui::Model model{};
    model.temperature = m_data.temperature;
    model.humidity = m_data.humidity;
    model.pressure = m_data.pressure;
    model.altitude = m_data.altitude;
    model.tempHighLimit = m_data.tempHighLimit;
    model.tempLowLimit = m_data.tempLowLimit;
    model.pressHighLimit = m_data.pressHighLimit;
    model.pressLowLimit = m_data.pressLowLimit;
    model.alarmState = m_data.alarmState;
    model.currentViewPage = m_data.currentViewPage;
    model.isMuted = m_data.isMuted;
    model.tempHumConnected = m_tempHumConnected;
    model.pressureConnected = m_pressureConnected;
    model.historyCount = m_historyCount;
    model.selectedThresholdField = m_selectedThresholdField;
    model.pendingTempHighLimit = m_pendingTempHighLimit;
    model.pendingTempLowLimit = m_pendingTempLowLimit;
    model.pendingPressHighLimit = m_pendingPressHighLimit;
    model.pendingPressLowLimit = m_pendingPressLowLimit;

    for (uint8_t i = 0; i < m_historyCount; ++i) {
        model.tempHistory[i] = m_tempHistory[i];
        model.pressHistory[i] = m_pressHistory[i];
    }
    return model;
}

void AppController::handleGuiCommand(const AppGui::Command& command)
{
    switch (command.type) {
        case AppGui::Command::Type::TogglePage:
            if (m_data.currentViewPage != 2) {
                m_data.currentViewPage = (m_data.currentViewPage == 0) ? 1 : 0;
                SYS_LOG("Touch: switched to page %d", m_data.currentViewPage);
            }
            break;
        case AppGui::Command::Type::EnterSettings:
            enterThresholdPage();
            break;
        case AppGui::Command::Type::AdjustThreshold:
            if (m_data.currentViewPage == 2) {
                updatePendingThreshold(command.field, command.increase);
            }
            break;
        case AppGui::Command::Type::Confirm:
            if (m_data.currentViewPage == 2) {
                applyPendingThresholds();
            }
            break;
        case AppGui::Command::Type::Cancel:
            if (m_data.currentViewPage == 2) {
                cancelPendingThresholds();
            }
            break;
        default:
            break;
    }
}

void AppController::appendHistory(int32_t temperature, uint32_t pressure)
{
    if (m_historyCount < kHistorySize) {
        m_tempHistory[m_historyCount] = temperature;
        m_pressHistory[m_historyCount] = pressure;
        ++m_historyCount;
        return;
    }

    for (uint8_t i = 0; i < kHistorySize - 1; ++i) {
        m_tempHistory[i] = m_tempHistory[i + 1];
        m_pressHistory[i] = m_pressHistory[i + 1];
    }
    m_tempHistory[kHistorySize - 1] = temperature;
    m_pressHistory[kHistorySize - 1] = pressure;
}

void AppController::enterThresholdPage()
{
    m_previousPage = (m_data.currentViewPage <= 1) ? m_data.currentViewPage : 0;
    m_data.currentViewPage = 2;
    m_pendingTempHighLimit = m_data.tempHighLimit;
    m_pendingTempLowLimit = m_data.tempLowLimit;
    m_pendingPressHighLimit = m_data.pressHighLimit;
    m_pendingPressLowLimit = m_data.pressLowLimit;
    m_selectedThresholdField = 0;
    SYS_LOG("Entered threshold settings page.");
}

void AppController::applyPendingThresholds()
{
    setTempLimits(m_pendingTempHighLimit, m_pendingTempLowLimit);
    setPressureLimits(m_pendingPressHighLimit, m_pendingPressLowLimit);
    m_data.currentViewPage = m_previousPage;
    SYS_LOG("Threshold changes confirmed.");
}

void AppController::cancelPendingThresholds()
{
    m_pendingTempHighLimit = m_data.tempHighLimit;
    m_pendingTempLowLimit = m_data.tempLowLimit;
    m_pendingPressHighLimit = m_data.pressHighLimit;
    m_pendingPressLowLimit = m_data.pressLowLimit;
    m_data.currentViewPage = m_previousPage;
    SYS_LOG("Threshold changes canceled.");
}

void AppController::updatePendingThreshold(uint8_t field, bool increase)
{
    switch (field) {
        case 0:
            m_pendingTempHighLimit += increase ? kTempStep : -kTempStep;
            if (m_pendingTempHighLimit <= m_pendingTempLowLimit) {
                m_pendingTempHighLimit = m_pendingTempLowLimit + kTempStep;
            }
            break;
        case 1:
            m_pendingTempLowLimit += increase ? kTempStep : -kTempStep;
            if (m_pendingTempLowLimit < 0) {
                m_pendingTempLowLimit = 0;
            }
            if (m_pendingTempLowLimit >= m_pendingTempHighLimit) {
                m_pendingTempLowLimit = m_pendingTempHighLimit - kTempStep;
            }
            break;
        case 2:
            if (increase) {
                m_pendingPressHighLimit += kPressStep;
            } else if (m_pendingPressHighLimit > kPressStep) {
                m_pendingPressHighLimit -= kPressStep;
            }
            if (m_pendingPressHighLimit <= m_pendingPressLowLimit) {
                m_pendingPressHighLimit = m_pendingPressLowLimit + kPressStep;
            }
            break;
        case 3:
            if (increase) {
                m_pendingPressLowLimit += kPressStep;
            } else if (m_pendingPressLowLimit > kPressStep) {
                m_pendingPressLowLimit -= kPressStep;
            }
            if (m_pendingPressLowLimit + kPressStep >= m_pendingPressHighLimit) {
                m_pendingPressLowLimit = m_pendingPressHighLimit - kPressStep;
            }
            break;
        default:
            return;
    }

    m_selectedThresholdField = field;
    SYS_LOG("Threshold field %u adjusted %s.", field, increase ? "up" : "down");
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
