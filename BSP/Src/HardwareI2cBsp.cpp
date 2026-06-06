#include "HardwareI2cBsp.hpp"

namespace Bsp {

HardwareI2cBsp::HardwareI2cBsp(I2C_HandleTypeDef* hi2c)
    : m_hi2c(hi2c)
{
}

void HardwareI2cBsp::init()
{
    // I2C clocks and pins are configured by CubeMX in MX_I2C2_Init/HAL_I2C_MspInit.
}

Sys::Status HardwareI2cBsp::writeRegs(uint8_t devAddr, uint8_t regAddr, const uint8_t* pData, uint16_t len)
{
    if (m_hi2c == nullptr || (pData == nullptr && len > 0U)) {
        return Sys::Status::ERROR_INIT;
    }

    return mapStatus(HAL_I2C_Mem_Write(m_hi2c,
                                       halAddress(devAddr),
                                       regAddr,
                                       I2C_MEMADD_SIZE_8BIT,
                                       const_cast<uint8_t*>(pData),
                                       len,
                                       kTimeoutMs));
}

Sys::Status HardwareI2cBsp::readRegs(uint8_t devAddr, uint8_t regAddr, uint8_t* pData, uint16_t len)
{
    if (m_hi2c == nullptr || (pData == nullptr && len > 0U)) {
        return Sys::Status::ERROR_INIT;
    }

    return mapStatus(HAL_I2C_Mem_Read(m_hi2c,
                                      halAddress(devAddr),
                                      regAddr,
                                      I2C_MEMADD_SIZE_8BIT,
                                      pData,
                                      len,
                                      kTimeoutMs));
}

Sys::Status HardwareI2cBsp::directRead(uint8_t devAddr, uint8_t* pData, uint16_t len)
{
    if (m_hi2c == nullptr || (pData == nullptr && len > 0U)) {
        return Sys::Status::ERROR_INIT;
    }

    return mapStatus(HAL_I2C_Master_Receive(m_hi2c, halAddress(devAddr), pData, len, kTimeoutMs));
}

Sys::Status HardwareI2cBsp::directWrite(uint8_t devAddr, const uint8_t* pData, uint16_t len)
{
    if (m_hi2c == nullptr || (pData == nullptr && len > 0U)) {
        return Sys::Status::ERROR_INIT;
    }

    return mapStatus(HAL_I2C_Master_Transmit(m_hi2c,
                                             halAddress(devAddr),
                                             const_cast<uint8_t*>(pData),
                                             len,
                                             kTimeoutMs));
}

Sys::Status HardwareI2cBsp::mapStatus(HAL_StatusTypeDef status)
{
    switch (status) {
        case HAL_OK:
            return Sys::Status::OK;
        case HAL_BUSY:
            return Sys::Status::ERROR_BUSY;
        case HAL_TIMEOUT:
            return Sys::Status::ERROR_TIMEOUT;
        default:
            return Sys::Status::ERROR_COM_FAIL;
    }
}

} // namespace Bsp