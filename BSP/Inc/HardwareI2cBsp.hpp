#pragma once

#include "II2cBus.hpp"
#include "stm32f1xx_hal.h"

namespace Bsp {

class HardwareI2cBsp : public II2cBus {
public:
    explicit HardwareI2cBsp(I2C_HandleTypeDef* hi2c);

    void init();

    Sys::Status writeRegs(uint8_t devAddr, uint8_t regAddr, const uint8_t* pData, uint16_t len) override;
    Sys::Status readRegs(uint8_t devAddr, uint8_t regAddr, uint8_t* pData, uint16_t len) override;
    Sys::Status directRead(uint8_t devAddr, uint8_t* pData, uint16_t len) override;
    Sys::Status directWrite(uint8_t devAddr, const uint8_t* pData, uint16_t len) override;

private:
    static constexpr uint32_t kTimeoutMs = 100U;

    I2C_HandleTypeDef* m_hi2c;

    static uint16_t halAddress(uint8_t devAddr) { return static_cast<uint16_t>(devAddr << 1); }
    static Sys::Status mapStatus(HAL_StatusTypeDef status);
};

} // namespace Bsp