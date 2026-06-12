// BSP/Src/Aht20Bsp.cpp
#include "Aht20Bsp.hpp"
#include "stm32f1xx_hal.h"

namespace Bsp {

namespace {

constexpr uint32_t kPowerUpDelayMs = 40U;
constexpr uint32_t kCalibDelayMs = 10U;
constexpr uint32_t kConversionDelayMs = 80U;
constexpr uint32_t kInitTimeoutMs = 200U;

}

Aht20Bsp::Aht20Bsp(II2cBus& i2cBus)
    : m_i2c(i2cBus)
{
}

Sys::Status Aht20Bsp::init()
{
    if (m_initialized) {
        return Sys::Status::OK;
    }

    const uint32_t startMs = HAL_GetTick();
    while (!m_initialized) {
        const Sys::Status status = ensureReady(HAL_GetTick());
        if (status == Sys::Status::OK) {
            return Sys::Status::OK;
        }
        if (status != Sys::Status::ERROR_BUSY) {
            return status;
        }
        if (hasElapsed(HAL_GetTick(), startMs + kInitTimeoutMs)) {
            return Sys::Status::ERROR_TIMEOUT;
        }
        HAL_Delay(1);
    }

    return Sys::Status::OK;
}

Sys::Status Aht20Bsp::beginSample(uint32_t nowMs)
{
    const Sys::Status readyStatus = ensureReady(nowMs);
    if (readyStatus != Sys::Status::OK) {
        return readyStatus;
    }

    if (m_conversionPending) {
        return Sys::Status::ERROR_BUSY;
    }

    // 发送触发测量命令
    uint8_t trigCmd[] = {0x33, 0x00};
    if (m_i2c.writeRegs(SYS_I2C_ADDR_AHT20, 0xAC, trigCmd, 2) != Sys::Status::OK) {
        m_initialized = false;
        m_state = AsyncState::Idle;
        return Sys::Status::ERROR_COM_FAIL;
    }

    m_deadlineMs = nowMs + kConversionDelayMs;
    m_conversionPending = true;
    m_state = AsyncState::WaitConversion;
    return Sys::Status::ERROR_BUSY;
}

Sys::Status Aht20Bsp::pollSample(uint32_t nowMs, int32_t& temperature, int32_t& humidity)
{
    const Sys::Status readyStatus = ensureReady(nowMs);
    if (readyStatus != Sys::Status::OK && readyStatus != Sys::Status::ERROR_BUSY) {
        return readyStatus;
    }

    if (readyStatus == Sys::Status::OK && !m_conversionPending && m_state == AsyncState::Ready) {
        return beginSample(nowMs);
    }

    if (!m_conversionPending || m_state != AsyncState::WaitConversion) {
        return Sys::Status::ERROR_BUSY;
    }

    if (!hasElapsed(nowMs, m_deadlineMs)) {
        return Sys::Status::ERROR_BUSY;
    }

    uint8_t buffer[6] = {0};
    if (m_i2c.directRead(SYS_I2C_ADDR_AHT20, buffer, 6) != Sys::Status::OK) {
        m_initialized = false;
        m_conversionPending = false;
        m_state = AsyncState::Idle;
        return Sys::Status::ERROR_COM_FAIL;
    }

    if ((buffer[0] & 0x80U) != 0U) {
        m_deadlineMs = nowMs + 10U;
        return Sys::Status::ERROR_BUSY;
    }

    uint32_t rawHumidity = ((uint32_t)buffer[1] << 12) | ((uint32_t)buffer[2] << 4) | ((uint32_t)buffer[3] >> 4);
    uint32_t rawTemperature = (((uint32_t)buffer[3] & 0x0FU) << 16) | ((uint32_t)buffer[4] << 8) | (uint32_t)buffer[5];

    humidity = (int32_t)((rawHumidity * 1000U) >> 20);
    temperature = (int32_t)(((rawTemperature * 2000U) >> 20) - 500);

    m_conversionPending = false;
    m_state = AsyncState::Ready;
    m_initialized = true;
    return Sys::Status::OK;
}

Sys::Status Aht20Bsp::read(int32_t& temperature, int32_t& humidity)
{
    if (init() != Sys::Status::OK) {
        return Sys::Status::ERROR_INIT;
    }

    if (beginSample(HAL_GetTick()) != Sys::Status::ERROR_BUSY) {
        return Sys::Status::ERROR_COM_FAIL;
    }

    const uint32_t startMs = HAL_GetTick();
    while (true) {
        const Sys::Status status = pollSample(HAL_GetTick(), temperature, humidity);
        if (status == Sys::Status::OK) {
            return Sys::Status::OK;
        }
        if (status != Sys::Status::ERROR_BUSY) {
            return status;
        }
        if (hasElapsed(HAL_GetTick(), startMs + kInitTimeoutMs)) {
            m_conversionPending = false;
            m_state = AsyncState::Ready;
            return Sys::Status::ERROR_TIMEOUT;
        }
        HAL_Delay(1);
    }
}

Sys::Status Aht20Bsp::ensureReady(uint32_t nowMs)
{
    if (m_initialized && m_state == AsyncState::Ready) {
        return Sys::Status::OK;
    }

    if (m_state == AsyncState::Idle) {
        m_state = AsyncState::PowerWait;
        m_deadlineMs = nowMs + kPowerUpDelayMs;
        return Sys::Status::ERROR_BUSY;
    }

    if (m_state == AsyncState::PowerWait) {
        if (!hasElapsed(nowMs, m_deadlineMs)) {
            return Sys::Status::ERROR_BUSY;
        }

        uint8_t status = 0;
        if (m_i2c.directRead(SYS_I2C_ADDR_AHT20, &status, 1) != Sys::Status::OK) {
            m_state = AsyncState::Idle;
            m_initialized = false;
            return Sys::Status::ERROR_INIT;
        }

        if ((status & 0x08U) == 0U) {
            uint8_t initCmd[] = {0x08, 0x00};
            if (m_i2c.writeRegs(SYS_I2C_ADDR_AHT20, 0xBE, initCmd, 2) != Sys::Status::OK) {
                m_state = AsyncState::Idle;
                m_initialized = false;
                return Sys::Status::ERROR_INIT;
            }
            m_state = AsyncState::CalibWait;
            m_deadlineMs = nowMs + kCalibDelayMs;
            return Sys::Status::ERROR_BUSY;
        }

        m_state = AsyncState::Ready;
        m_initialized = true;
        return Sys::Status::OK;
    }

    if (m_state == AsyncState::CalibWait) {
        if (!hasElapsed(nowMs, m_deadlineMs)) {
            return Sys::Status::ERROR_BUSY;
        }
        m_state = AsyncState::Ready;
        m_initialized = true;
        return Sys::Status::OK;
    }

    return (m_state == AsyncState::Ready) ? Sys::Status::OK : Sys::Status::ERROR_BUSY;
}

bool Aht20Bsp::hasElapsed(uint32_t nowMs, uint32_t deadlineMs)
{
    return static_cast<int32_t>(nowMs - deadlineMs) >= 0;
}

} // namespace Bsp
