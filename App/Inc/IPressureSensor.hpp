// App/Inc/IPressureSensor.hpp
#pragma once
#include "sys.hpp"

namespace App {

class IPressureSensor {
public:
    virtual ~IPressureSensor() = default;
    
    // 初始化气压传感器，加载内部出厂校准寄存器参数，成功返回 Sys::Status::OK
    virtual Sys::Status init() = 0;
    
    // 获取最新大气压强原始补偿值(Pa) 与 估算海拔高度(m)
    virtual Sys::Status read(float& pressure, float& altitude) = 0;
};

} // namespace App
