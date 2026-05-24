// App/Inc/IIndicator.hpp
#pragma once
#include <stdint.h>

namespace App {

class IIndicator {
public:
    virtual ~IIndicator() = default;
    
    // 设置指示灯工作状态为“正常呼吸灯”(频率约 0.5Hz，2秒呼吸一次)
    virtual void setNormalBreathing() = 0;
    
    // 设置指示灯工作状态为“异常剧烈闪烁”(频率 5Hz)
    virtual void setWarningBlinking() = 0;
    
    // 强制关闭指示灯
    virtual void turnOff() = 0;
    
    // 渐变时序更新驱动接口，用于在 10ms 的 Timer 中断内周期执行动画物理模拟
    virtual void updatePhysics(uint32_t elapsedMs) = 0;
};

} // namespace App
