// App/Inc/ITouch.hpp
#pragma once
#include <stdint.h>

namespace App {

struct TouchPoint {
    uint16_t x;
    uint16_t y;
    bool valid;
};

struct TouchEvent {
    enum class Type : uint8_t {
        Pressed,
        Released,
        Tap
    };

    Type type{Type::Released};
    TouchPoint point{0, 0, false};
};

class ITouch {
public:
    virtual ~ITouch() = default;

    virtual bool init() = 0;

    // 检测触摸屏是否被按下（读 PENIRQ 引脚）
    virtual bool isTouched() = 0;

    // 推进触摸采样状态机。调用者按调度节拍周期执行。
    virtual void scanTick(uint32_t nowMs) = 0;

    // 弹出最新触摸事件；无事件时返回 false。
    virtual bool popEvent(TouchEvent& event) = 0;
};

} // namespace App
