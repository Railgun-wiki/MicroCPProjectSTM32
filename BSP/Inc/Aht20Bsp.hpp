// BSP/Inc/Aht20Bsp.hpp
#pragma once
#include "ITempHumSensor.hpp"
#include "SoftI2cBsp.hpp"

namespace Bsp {

class Aht20Bsp : public App::ITempHumSensor {
public:
    // 构造函数传入 I2C 总线引用的实例，方便复用
    Aht20Bsp(SoftI2cBsp& i2cBus);
    
    Sys::Status init() override;
    Sys::Status read(float& temperature, float& humidity) override;

private:
    SoftI2cBsp& m_i2c;
    bool m_initialized{false};
};

} // namespace Bsp
