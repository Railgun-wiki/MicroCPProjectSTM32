// App/Inc/AppController.hpp
#pragma once
#include "ITempHumSensor.hpp"
#include "IPressureSensor.hpp"
#include "IIndicator.hpp"
#include "IButton.hpp"
#include "ILcdDisplay.hpp"
#include "ITouch.hpp"
#include "sys.hpp"

namespace App {

class AppController {
public:
    // 依赖注入：在构造时引入所有解耦后的虚接口引用，彻底隔绝底层硬件实现
    AppController(ITempHumSensor& th, IPressureSensor& press, IIndicator& led, IButton& keyPage, IButton& keyConfirm, IButton& keyBack, ILcdDisplay& lcd, ITouch& touch);
    
    // 初始化应用控制器，复位状态机与传感器参数
    void setup();
    
    // 主循环状态机轮询方法 (以 10Hz 的心跳周期运行)
    void run();

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
        uint8_t currentViewPage{0}; // 0: 温湿大屏, 1: 气压海拔
        bool isMuted{false}; // 当前用于表示“告警展示已被确认/抑制”
    };

    // 获取全局遥测结构体实例 (由外部线程安全地只读)
    const TelemetryData& getTelemetry() const { return m_data; }
    
    // 允许修改报警限值（成员 B 通过屏幕交互操作）
    void setTempLimits(int32_t high, int32_t low);
    void setPressureLimits(uint32_t high, uint32_t low);

private:
    ITempHumSensor& m_th;
    IPressureSensor& m_press;
    IIndicator& m_led;
    IButton& m_keyPage;
    IButton& m_keyConfirm;
    IButton& m_keyBack;
    ILcdDisplay& m_lcd;
    ITouch& m_touch;

    TelemetryData m_data;

    // 传感器连接状态追踪
    bool m_tempHumConnected{false};
    bool m_pressureConnected{false};

    void updateTelemetry();
    void updateStateMachine();
    void handleInteractions();
};

} // namespace App
