// App/Inc/ITempHumSensor.hpp
#pragma once
#include "sys.hpp"

namespace App {

class ITempHumSensor {
public:
    virtual ~ITempHumSensor() = default;
    
    // 初始化传感器引脚、总线或内部状态，成功返回 Sys::Status::OK
    virtual Sys::Status init() = 0;

    // 启动一轮非阻塞采样。若采样已在进行中或设备仍在准备阶段，返回 ERROR_BUSY。
    virtual Sys::Status beginSample(uint32_t nowMs) = 0;

    // 轮询非阻塞采样结果。采样未完成时返回 ERROR_BUSY，完成后写出温湿度。
    virtual Sys::Status pollSample(uint32_t nowMs, int32_t& temperature, int32_t& humidity) = 0;
    
    // 周期获取最新物理数值：温度(放大10倍) 与 相对湿度(放大10倍)
    virtual Sys::Status read(int32_t& temperature, int32_t& humidity) = 0;
};

} // namespace App
