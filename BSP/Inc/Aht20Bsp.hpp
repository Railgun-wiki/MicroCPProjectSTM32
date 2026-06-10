// BSP/Inc/Aht20Bsp.hpp
#pragma once
#include "ITempHumSensor.hpp"
#include "II2cBus.hpp"

namespace Bsp {

class Aht20Bsp : public App::ITempHumSensor {
public:
    // 构造函数传入 I2C 总线引用的实例，方便复用
    Aht20Bsp(II2cBus& i2cBus);
    
    Sys::Status init() override;
    Sys::Status read(int32_t& temperature, int32_t& humidity) override;

private:
    II2cBus& m_i2c;
    bool m_initialized{false};
};

} // namespace Bsp
