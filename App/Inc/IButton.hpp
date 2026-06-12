// App/Inc/IButton.hpp
#pragma once

namespace App {

class IButton {
public:
    virtual ~IButton() = default;
    
    // 查询按键是否被有效按下（此操作为非阻塞、查询后内部标志自动复位，单次触发有效）
    virtual bool isPressed() = 0;
    
    // 按键物理电平扫描和消抖计数推进（由调度器按 10ms 周期调用）
    virtual void scanTick() = 0;
};

} // namespace App
