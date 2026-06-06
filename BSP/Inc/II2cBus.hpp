#pragma once

#include <stdint.h>
#include "sys.hpp"

namespace Bsp {

class II2cBus {
public:
    virtual ~II2cBus() = default;

    virtual Sys::Status writeRegs(uint8_t devAddr, uint8_t regAddr, const uint8_t* pData, uint16_t len) = 0;
    virtual Sys::Status readRegs(uint8_t devAddr, uint8_t regAddr, uint8_t* pData, uint16_t len) = 0;
    virtual Sys::Status directRead(uint8_t devAddr, uint8_t* pData, uint16_t len) = 0;
    virtual Sys::Status directWrite(uint8_t devAddr, const uint8_t* pData, uint16_t len) = 0;
};

} // namespace Bsp