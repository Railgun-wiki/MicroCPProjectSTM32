// App/Inc/AppController.hpp
#pragma once
#include "ITempHumSensor.hpp"
#include "IPressureSensor.hpp"
#include "IIndicator.hpp"
#include "IButton.hpp"
#include "ITouch.hpp"
#include "AppGui.hpp"
#include "sys.hpp"

namespace App {

class AppController {
public:
    // 依赖注入：在构造时引入所有解耦后的虚接口引用，彻底隔绝底层硬件实现
    AppController(ITempHumSensor& th, IPressureSensor& press, IIndicator& led, IButton& keyPage, IButton& keyConfirm, IButton& keyBack, AppGui& gui, ITouch& touch);
    
    // 初始化应用控制器，复位状态机与传感器参数
    void setup();
    
    // 主循环状态机轮询方法 (以 10Hz 的心跳周期运行)
    void run();
    void updateLed(uint32_t elapsedMs);
    void scanKeys();
    void processInputs();
    void pollTouch(uint32_t nowMs);
    void renderGuiTick();
    void startSensorSample(uint32_t nowMs);
    void stepSensors(uint32_t nowMs);
    void updateStateMachine();
    void refreshDisplay();
    void logHealth();

    // 传感器遥测数据采集结构体 (用于由成员 B 读取直接更新 UI)
    struct TelemetryData {
        int32_t temperature{0};
        int32_t humidity{0};
        uint32_t pressure{0};
        int32_t altitude{0};
        
        int32_t tempHighLimit{350};
        int32_t tempLowLimit{100};
        uint32_t pressHighLimit{103000}; // 1030 hPa
        uint32_t pressLowLimit{98000};   // 980 hPa
        
        Sys::AlarmState alarmState{Sys::AlarmState::NORMAL};
        uint8_t currentViewPage{0}; // 0: 温湿大屏, 1: 气压海拔, 2: 阈值设置
        bool isMuted{false}; // 当前用于表示“告警展示已被确认/抑制”
    };

    // 获取全局遥测结构体实例 (由外部线程安全地只读)
    const TelemetryData& getTelemetry() const { return m_data; }
    
    // 允许修改报警限值（成员 B 通过屏幕交互操作）
    void setTempLimits(int32_t high, int32_t low);
    void setPressureLimits(uint32_t high, uint32_t low);

private:
    static constexpr uint8_t kHistorySize = 30;
    static constexpr int32_t kTempStep = 1;       // 0.1 C
    static constexpr uint32_t kPressStep = 100;   // 0.1 kPa

    ITempHumSensor& m_th;
    IPressureSensor& m_press;
    IIndicator& m_led;
    IButton& m_keyPage;
    IButton& m_keyConfirm;
    IButton& m_keyBack;
    AppGui& m_gui;
    ITouch& m_touch;

    TelemetryData m_data;

    // 传感器连接状态追踪
    bool m_tempHumConnected{false};
    bool m_pressureConnected{false};
    bool m_tempHumSampleActive{false};

    uint8_t m_previousPage{0};
    int32_t m_pendingTempHighLimit{350};
    int32_t m_pendingTempLowLimit{100};
    uint32_t m_pendingPressHighLimit{103000};
    uint32_t m_pendingPressLowLimit{98000};
    uint8_t m_selectedThresholdField{0};

    int32_t m_tempHistory[kHistorySize]{};
    uint32_t m_pressHistory[kHistorySize]{};
    uint8_t m_historyCount{0};

    AppGui::Model buildGuiModel() const;
    void handleGuiCommand(const AppGui::Command& command);
    void appendHistory(int32_t temperature, uint32_t pressure);
    void enterThresholdPage();
    void applyPendingThresholds();
    void cancelPendingThresholds();
    void updatePendingThreshold(uint8_t field, bool increase);
};

} // namespace App
