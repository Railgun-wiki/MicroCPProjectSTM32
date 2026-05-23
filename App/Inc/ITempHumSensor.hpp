// App/Inc/ITempHumSensor.hpp
#pragma once
#include "sys.hpp"

namespace App {

class ITempHumSensor {
public:
    virtual ~ITempHumSensor() = default;
    
    // 初始化传感器引脚、总线或内部状态，成功返回 Sys::Status::OK
    virtual Sys::Status init() = 0;
    
    // 周期获取最新物理数值：温度(℃) 与 相对湿度(%)
    virtual Sys::Status read(float& temperature, float& humidity) = 0;
};

} // namespace App
