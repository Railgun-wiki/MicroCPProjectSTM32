// BSP/Inc/SoftI2cBsp.hpp
#pragma once
#include "stm32f1xx_hal.h"
#include "II2cBus.hpp"

namespace Bsp {

class SoftI2cBsp : public II2cBus {
public:
    SoftI2cBsp(GPIO_TypeDef* sclPort, uint16_t sclPin, GPIO_TypeDef* sdaPort, uint16_t sdaPin);
    
    void init();
    
    Sys::Status writeRegs(uint8_t devAddr, uint8_t regAddr, const uint8_t* pData, uint16_t len) override;
    Sys::Status readRegs(uint8_t devAddr, uint8_t regAddr, uint8_t* pData, uint16_t len) override;
    Sys::Status directRead(uint8_t devAddr, uint8_t* pData, uint16_t len) override;
    Sys::Status directWrite(uint8_t devAddr, const uint8_t* pData, uint16_t len) override;

private:
    GPIO_TypeDef* m_sclPort;
    uint16_t m_sclPin;
    GPIO_TypeDef* m_sdaPort;
    uint16_t m_sdaPin;

    void start();
    void stop();
    void sendAck();
    void sendNack();
    bool waitAck();
    void writeByte(uint8_t byte);
    uint8_t readByte(bool ack);
    
    void delayUs();
    void sdaOut();
    void sdaIn();
};

} // namespace Bsp
