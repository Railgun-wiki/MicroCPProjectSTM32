// BSP/Src/SoftI2cBsp.cpp
#include "SoftI2cBsp.hpp"

namespace Bsp {

SoftI2cBsp::SoftI2cBsp(GPIO_TypeDef* sclPort, uint16_t sclPin, GPIO_TypeDef* sdaPort, uint16_t sdaPin)
    : m_sclPort(sclPort)
    , m_sclPin(sclPin)
    , m_sdaPort(sdaPort)
    , m_sdaPin(sdaPin)
{
}

void SoftI2cBsp::init()
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};

    // 启用 GPIO 时钟 (根据传入的 Port 动态判断并启用时钟)
    if (m_sclPort == GPIOA || m_sdaPort == GPIOA) __HAL_RCC_GPIOA_CLK_ENABLE();
    if (m_sclPort == GPIOB || m_sdaPort == GPIOB) __HAL_RCC_GPIOB_CLK_ENABLE();
    if (m_sclPort == GPIOC || m_sdaPort == GPIOC) __HAL_RCC_GPIOC_CLK_ENABLE();

    // 配置 SCL 为开漏输出 (带上拉)
    GPIO_InitStruct.Pin = m_sclPin;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_OD;
    GPIO_InitStruct.Pull = GPIO_PULLUP;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
    HAL_GPIO_Init(m_sclPort, &GPIO_InitStruct);

    // 配置 SDA 为开漏输出 (带上拉)
    GPIO_InitStruct.Pin = m_sdaPin;
    HAL_GPIO_Init(m_sdaPort, &GPIO_InitStruct);

    // 总线空闲状态：SCL 和 SDA 均为高电平
    HAL_GPIO_WritePin(m_sclPort, m_sclPin, GPIO_PIN_SET);
    HAL_GPIO_WritePin(m_sdaPort, m_sdaPin, GPIO_PIN_SET);
}

void SoftI2cBsp::delayUs()
{
    // 在 72MHz 下，volatile 循环 30 次大约产生 4~5 微秒延时，符合 100kHz I2C 速率要求
    volatile uint32_t i = 35;
    while (i--);
}

void SoftI2cBsp::sdaOut()
{
    // 由于采用开漏 (Open-Drain) 模式配置，SDA 可以直接输出，也可以直接读取输入
    // 无需在运行时动态频繁重构 GPIO 寄存器，极大提升通信稳定度和速度
}

void SoftI2cBsp::sdaIn()
{
}

void SoftI2cBsp::start()
{
    HAL_GPIO_WritePin(m_sdaPort, m_sdaPin, GPIO_PIN_SET);
    HAL_GPIO_WritePin(m_sclPort, m_sclPin, GPIO_PIN_SET);
    delayUs();
    HAL_GPIO_WritePin(m_sdaPort, m_sdaPin, GPIO_PIN_RESET);
    delayUs();
    HAL_GPIO_WritePin(m_sclPort, m_sclPin, GPIO_PIN_RESET);
    delayUs();
}

void SoftI2cBsp::stop()
{
    HAL_GPIO_WritePin(m_sdaPort, m_sdaPin, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(m_sclPort, m_sclPin, GPIO_PIN_SET);
    delayUs();
    HAL_GPIO_WritePin(m_sdaPort, m_sdaPin, GPIO_PIN_SET);
    delayUs();
}

void SoftI2cBsp::sendAck()
{
    HAL_GPIO_WritePin(m_sdaPort, m_sdaPin, GPIO_PIN_RESET);
    delayUs();
    HAL_GPIO_WritePin(m_sclPort, m_sclPin, GPIO_PIN_SET);
    delayUs();
    HAL_GPIO_WritePin(m_sclPort, m_sclPin, GPIO_PIN_RESET);
    delayUs();
    HAL_GPIO_WritePin(m_sdaPort, m_sdaPin, GPIO_PIN_SET);
}

void SoftI2cBsp::sendNack()
{
    HAL_GPIO_WritePin(m_sdaPort, m_sdaPin, GPIO_PIN_SET);
    delayUs();
    HAL_GPIO_WritePin(m_sclPort, m_sclPin, GPIO_PIN_SET);
    delayUs();
    HAL_GPIO_WritePin(m_sclPort, m_sclPin, GPIO_PIN_RESET);
    delayUs();
}

bool SoftI2cBsp::waitAck()
{
    HAL_GPIO_WritePin(m_sdaPort, m_sdaPin, GPIO_PIN_SET); // 释放 SDA 总线
    delayUs();
    HAL_GPIO_WritePin(m_sclPort, m_sclPin, GPIO_PIN_SET);
    delayUs();
    
    uint16_t timeout = 0;
    while (HAL_GPIO_ReadPin(m_sdaPort, m_sdaPin) == GPIO_PIN_SET) {
        timeout++;
        if (timeout > 500) {
            stop();
            return false;
        }
        delayUs();
    }
    
    HAL_GPIO_WritePin(m_sclPort, m_sclPin, GPIO_PIN_RESET);
    delayUs();
    return true;
}

void SoftI2cBsp::writeByte(uint8_t byte)
{
    for (uint8_t i = 0; i < 8; i++) {
        if (byte & 0x80) {
            HAL_GPIO_WritePin(m_sdaPort, m_sdaPin, GPIO_PIN_SET);
        } else {
            HAL_GPIO_WritePin(m_sdaPort, m_sdaPin, GPIO_PIN_RESET);
        }
        byte <<= 1;
        delayUs();
        HAL_GPIO_WritePin(m_sclPort, m_sclPin, GPIO_PIN_SET);
        delayUs();
        HAL_GPIO_WritePin(m_sclPort, m_sclPin, GPIO_PIN_RESET);
        delayUs();
    }
}

uint8_t SoftI2cBsp::readByte(bool ack)
{
    uint8_t byte = 0;
    HAL_GPIO_WritePin(m_sdaPort, m_sdaPin, GPIO_PIN_SET); // 释放 SDA，准备读取
    delayUs();
    
    for (uint8_t i = 0; i < 8; i++) {
        HAL_GPIO_WritePin(m_sclPort, m_sclPin, GPIO_PIN_SET);
        delayUs();
        byte <<= 1;
        if (HAL_GPIO_ReadPin(m_sdaPort, m_sdaPin) == GPIO_PIN_SET) {
            byte |= 0x01;
        }
        HAL_GPIO_WritePin(m_sclPort, m_sclPin, GPIO_PIN_RESET);
        delayUs();
    }
    
    if (ack) {
        sendAck();
    } else {
        sendNack();
    }
    return byte;
}

Sys::Status SoftI2cBsp::writeRegs(uint8_t devAddr, uint8_t regAddr, const uint8_t* pData, uint16_t len)
{
    start();
    writeByte((devAddr << 1) | 0);
    if (!waitAck()) return Sys::Status::ERROR_COM_FAIL;

    writeByte(regAddr);
    if (!waitAck()) return Sys::Status::ERROR_COM_FAIL;

    for (uint16_t i = 0; i < len; i++) {
        writeByte(pData[i]);
        if (!waitAck()) return Sys::Status::ERROR_COM_FAIL;
    }

    stop();
    return Sys::Status::OK;
}

Sys::Status SoftI2cBsp::readRegs(uint8_t devAddr, uint8_t regAddr, uint8_t* pData, uint16_t len)
{
    // 1. 写寄存器地址
    start();
    writeByte((devAddr << 1) | 0);
    if (!waitAck()) return Sys::Status::ERROR_COM_FAIL;

    writeByte(regAddr);
    if (!waitAck()) return Sys::Status::ERROR_COM_FAIL;

    // 2. 重新启动，进入读模式
    start();
    writeByte((devAddr << 1) | 1);
    if (!waitAck()) return Sys::Status::ERROR_COM_FAIL;

    for (uint16_t i = 0; i < len; i++) {
        pData[i] = readByte(i < (len - 1)); // 最后一个字节发送 NACK
    }

    stop();
    return Sys::Status::OK;
}

Sys::Status SoftI2cBsp::directRead(uint8_t devAddr, uint8_t* pData, uint16_t len)
{
    start();
    writeByte((devAddr << 1) | 1);
    if (!waitAck()) return Sys::Status::ERROR_COM_FAIL;

    for (uint16_t i = 0; i < len; i++) {
        pData[i] = readByte(i < (len - 1));
    }

    stop();
    return Sys::Status::OK;
}

Sys::Status SoftI2cBsp::directWrite(uint8_t devAddr, const uint8_t* pData, uint16_t len)
{
    start();
    writeByte((devAddr << 1) | 0);
    if (!waitAck()) return Sys::Status::ERROR_COM_FAIL;

    for (uint16_t i = 0; i < len; i++) {
        writeByte(pData[i]);
        if (!waitAck()) return Sys::Status::ERROR_COM_FAIL;
    }

    stop();
    return Sys::Status::OK;
}

} // namespace Bsp
