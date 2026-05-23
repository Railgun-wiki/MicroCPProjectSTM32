// BSP/Src/Aht20Bsp.cpp
#include "Aht20Bsp.hpp"

namespace Bsp {

Aht20Bsp::Aht20Bsp(SoftI2cBsp& i2cBus)
    : m_i2c(i2cBus)
{
}

Sys::Status Aht20Bsp::init()
{
    if (m_initialized) return Sys::Status::OK;

    HAL_Delay(40); // 上电延时 40ms

    uint8_t status = 0;
    // 读取 AHT20 的状态字
    if (m_i2c.directRead(SYS_I2C_ADDR_AHT20, &status, 1) != Sys::Status::OK) {
        return Sys::Status::ERROR_INIT;
    }

    // 检查校准使能位 Bit[3] 是否为 1
    if ((status & 0x08) == 0) {
        // 未校准，需要发送初始化命令 0xBE
        uint8_t initCmd[] = {0x08, 0x00};
        if (m_i2c.writeRegs(SYS_I2C_ADDR_AHT20, 0xBE, initCmd, 2) != Sys::Status::OK) {
            return Sys::Status::ERROR_INIT;
        }
        HAL_Delay(10);
    }

    m_initialized = true;
    return Sys::Status::OK;
}

Sys::Status Aht20Bsp::read(float& temperature, float& humidity)
{
    if (!m_initialized) {
        if (init() != Sys::Status::OK) {
            return Sys::Status::ERROR_INIT;
        }
    }

    // 1. 发送触发测量命令
    uint8_t trigCmd[] = {0x33, 0x00};
    if (m_i2c.writeRegs(SYS_I2C_ADDR_AHT20, 0xAC, trigCmd, 2) != Sys::Status::OK) {
        return Sys::Status::ERROR_COM_FAIL;
    }

    // 2. 等待 80ms 测量完成 (AHT20 转换需要 75-80ms)
    HAL_Delay(80);

    // 3. 读取 6 字节的数据帧
    uint8_t buffer[6] = {0};
    if (m_i2c.directRead(SYS_I2C_ADDR_AHT20, buffer, 6) != Sys::Status::OK) {
        return Sys::Status::ERROR_COM_FAIL;
    }

    // 4. 判定是否处于忙状态 (Bit[7] == 1 表示忙，0 表示测量结束)
    if ((buffer[0] & 0x80) != 0) {
        return Sys::Status::ERROR_BUSY;
    }

    // 5. 解析并换算温湿度物理值 (20-bit 原始数据)
    uint32_t rawHumidity = ((uint32_t)buffer[1] << 12) | ((uint32_t)buffer[2] << 4) | ((uint32_t)buffer[3] >> 4);
    uint32_t rawTemperature = (((uint32_t)buffer[3] & 0x0F) << 16) | ((uint32_t)buffer[4] << 8) | (uint32_t)buffer[5];

    // 相对湿度换算: RH% = S_RH / 2^20 * 100
    humidity = ((float)rawHumidity / 1048576.0f) * 100.0f;

    // 温度换算: T = S_T / 2^20 * 200 - 50
    temperature = ((float)rawTemperature / 1048576.0f) * 200.0f - 50.0f;

    return Sys::Status::OK;
}

} // namespace Bsp
