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
    Sys::Status beginSample(uint32_t nowMs) override;
    Sys::Status pollSample(uint32_t nowMs, int32_t& temperature, int32_t& humidity) override;
    Sys::Status read(int32_t& temperature, int32_t& humidity) override;

private:
    enum class AsyncState : uint8_t {
        Idle = 0,
        PowerWait,
        CalibWait,
        Ready,
        WaitConversion
    };

    II2cBus& m_i2c;
    bool m_initialized{false};
    bool m_conversionPending{false};
    AsyncState m_state{AsyncState::Idle};
    uint32_t m_deadlineMs{0};

    Sys::Status ensureReady(uint32_t nowMs);
    static bool hasElapsed(uint32_t nowMs, uint32_t deadlineMs);
};

} // namespace Bsp
