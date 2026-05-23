// App/Src/AppController.cpp
#include "AppController.hpp"

namespace App {

AppController::AppController(ITempHumSensor& th, IPressureSensor& press, IIndicator& led, IButton& keyPage, IButton& keyMute)
    : m_th(th)
    , m_press(press)
    , m_led(led)
    , m_keyPage(keyPage)
    , m_keyMute(keyMute)
{
}

void AppController::setup()
{
    SYS_LOG("Initializing AppController...");
    
    // 初始化指示灯
    m_led.turnOff();
    
    // 自检温湿传感器
    if (m_th.init() == Sys::Status::OK) {
        SYS_LOG("ITempHumSensor initialized successfully.");
    } else {
        SYS_LOG("ITempHumSensor initialization failed!");
    }
    
    // 自检气压传感器
    if (m_press.init() == Sys::Status::OK) {
        SYS_LOG("IPressureSensor initialized successfully.");
    } else {
        SYS_LOG("IPressureSensor initialization failed!");
    }
    
    // 设置初始指示灯为正常呼吸效果
    m_led.setNormalBreathing();
    m_data.alarmState = Sys::AlarmState::NORMAL;
    m_data.currentViewPage = 0;
    m_data.isMuted = false;
}

void AppController::run()
{
    // 1. 采集最新的传感器物理数据
    updateTelemetry();
    
    // 2. 检查按键输入执行切屏与消音翻转
    handleInteractions();
    
    // 3. 执行状态机并更新系统指标
    updateStateMachine();
}

void AppController::updateTelemetry()
{
    float temp{0.0f}, hum{0.0f};
    if (m_th.read(temp, hum) == Sys::Status::OK) {
        m_data.temperature = temp;
        m_data.humidity = hum;
    } else {
        SYS_LOG("Error: Failed to read temperature and humidity.");
    }

    float press{0.0f}, alt{0.0f};
    if (m_press.read(press, alt) == Sys::Status::OK) {
        m_data.pressure = press;
        m_data.altitude = alt;
    } else {
        SYS_LOG("Error: Failed to read atmospheric pressure.");
    }
}

void AppController::handleInteractions()
{
    // KEY1 用于切换 LCD 显示页 (0: 温湿屏, 1: 气压海拔屏)
    if (m_keyPage.isPressed()) {
        m_data.currentViewPage = (m_data.currentViewPage == 0) ? 1 : 0;
        SYS_LOG("KEY1 pressed. Switched LCD to page %d", m_data.currentViewPage);
    }
    
    // KEY2 用于报警状态下的静音消音操作
    if (m_keyMute.isPressed()) {
        if (m_data.alarmState == Sys::AlarmState::WARNING_TEMP || 
            m_data.alarmState == Sys::AlarmState::WARNING_PRES) {
            m_data.isMuted = true;
            m_data.alarmState = Sys::AlarmState::MUTED;
            SYS_LOG("KEY2 pressed. Alarm is muted.");
        } else if (m_data.alarmState == Sys::AlarmState::MUTED) {
            m_data.isMuted = false;
            // 重新进入异常状态以重新判定
            SYS_LOG("KEY2 pressed. Alarm unmuted.");
        }
    }
}

void AppController::updateStateMachine()
{
    // 报警上下限判定条件
    bool tempAbnormal = (m_data.temperature > m_data.tempHighLimit) || (m_data.temperature < m_data.tempLowLimit);
    bool pressAbnormal = (m_data.pressure > m_data.pressHighLimit) || (m_data.pressure < m_data.pressLowLimit);

    // 状态自愈：若当前没有任何异常，重置状态为正常，并解除静音
    if (!tempAbnormal && !pressAbnormal) {
        if (m_data.alarmState != Sys::AlarmState::NORMAL) {
            m_data.alarmState = Sys::AlarmState::NORMAL;
            m_data.isMuted = false;
            m_led.setNormalBreathing();
            SYS_LOG("Environment recovered. Back to STATE_NORMAL.");
        }
        return;
    }

    // 若已经处于静音状态，保持静音状态，除非环境恢复正常（上面已处理）
    if (m_data.isMuted) {
        m_data.alarmState = Sys::AlarmState::MUTED;
        // 在静音状态下，LED 仍然需要保持 5Hz 高频爆闪作为安全警示
        m_led.setWarningBlinking();
        return;
    }

    // 状态转移与报警判定
    if (tempAbnormal) {
        if (m_data.alarmState != Sys::AlarmState::WARNING_TEMP) {
            m_data.alarmState = Sys::AlarmState::WARNING_TEMP;
            m_led.setWarningBlinking();
            SYS_LOG("Alarm Triggered: Temperature abnormal! Temp: %.2f C", m_data.temperature);
        }
    } else if (pressAbnormal) {
        if (m_data.alarmState != Sys::AlarmState::WARNING_PRES) {
            m_data.alarmState = Sys::AlarmState::WARNING_PRES;
            m_led.setWarningBlinking();
            SYS_LOG("Alarm Triggered: Atmospheric pressure abnormal! Press: %.2f Pa", m_data.pressure);
        }
    }
}

void AppController::setTempLimits(float high, float low)
{
    m_data.tempHighLimit = high;
    m_data.tempLowLimit = low;
    SYS_LOG("Updated Temperature limits: High: %.1f C, Low: %.1f C", high, low);
}

void AppController::setPressureLimits(float high, float low)
{
    m_data.pressHighLimit = high;
    m_data.pressLowLimit = low;
    SYS_LOG("Updated Pressure limits: High: %.1f Pa, Low: %.1f Pa", high, low);
}

} // namespace App
