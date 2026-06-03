// App/Inc/ITouch.hpp
#pragma once
#include <stdint.h>

namespace App {

struct TouchPoint {
    uint16_t x;
    uint16_t y;
    bool valid;
};

class ITouch {
public:
    virtual ~ITouch() = default;

    virtual bool init() = 0;

    // 检测触摸屏是否被按下（读 PENIRQ 引脚）
    virtual bool isTouched() = 0;

    // 读取校准后的触摸坐标；valid=false 表示无有效触摸
    virtual bool readPosition(TouchPoint& point) = 0;
};

} // namespace App
