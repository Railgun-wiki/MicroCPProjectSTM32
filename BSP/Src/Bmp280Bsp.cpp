// BSP/Src/Bmp280Bsp.cpp
#include "Bmp280Bsp.hpp"
#include <math.h>

namespace Bsp {

Bmp280Bsp::Bmp280Bsp(SoftI2cBsp& i2cBus)
    : m_i2c(i2cBus)
{
}

Sys::Status Bmp280Bsp::init()
{
    if (m_initialized) return Sys::Status::OK;

    HAL_Delay(10); // 上电复位等待

    // 1. 读取 Chip ID 寄存器 0xD0，校验是否为 0x58
    uint8_t chipId = 0;
    if (m_i2c.readRegs(SYS_I2C_ADDR_BMP280, 0xD0, &chipId, 1) != Sys::Status::OK) {
        SYS_LOG("Error: Failed to read BMP280 chip ID.");
        return Sys::Status::ERROR_INIT;
    }
    
    if (chipId != 0x58) {
        SYS_LOG("Error: BMP280 chip ID mismatch! Expected 0x58, got 0x%02X", chipId);
        return Sys::Status::ERROR_INIT;
    }

    // 2. 一次性读取出厂标定参数 0x88 ~ 0xA1 (共 24 字节)
    if (loadCalibration() != Sys::Status::OK) {
        SYS_LOG("Error: Failed to load BMP280 calibration parameters.");
        return Sys::Status::ERROR_INIT;
    }

    // 3. 配置配置寄存器 0xF5 (config)
    // Standby = 500ms (100), IIR Filter = 16x (100), SPI 3-wire disabled (0)
    // 字节值: (4 << 5) | (4 << 2) = 0x80 | 0x10 = 0x90
    uint8_t configVal = 0x90;
    if (m_i2c.writeRegs(SYS_I2C_ADDR_BMP280, 0xF5, &configVal, 1) != Sys::Status::OK) {
        return Sys::Status::ERROR_INIT;
    }

    // 4. 配置控制寄存器 0xF4 (ctrl_meas)
    // Temperature oversampling = 2x (010), Pressure oversampling = 16x (101), Mode = Normal (11)
    // 字节值: (2 << 5) | (5 << 2) | 3 = 0x40 | 0x14 | 0x03 = 0x57
    uint8_t ctrlMeasVal = 0x57;
    if (m_i2c.writeRegs(SYS_I2C_ADDR_BMP280, 0xF4, &ctrlMeasVal, 1) != Sys::Status::OK) {
        return Sys::Status::ERROR_INIT;
    }

    m_initialized = true;
    SYS_LOG("BMP280 sensor initialized successfully.");
    return Sys::Status::OK;
}

Sys::Status Bmp280Bsp::loadCalibration()
{
    uint8_t buffer[24] = {0};
    // 0x88 起始连续读取 24 字节
    if (m_i2c.readRegs(SYS_I2C_ADDR_BMP280, 0x88, buffer, 24) != Sys::Status::OK) {
        return Sys::Status::ERROR_COM_FAIL;
    }

    // 解析出厂参数 (低字节在前，高字节在后)
    m_calib.dig_T1 = ((uint16_t)buffer[1] << 8) | buffer[0];
    m_calib.dig_T2 = (int16_t)(((uint16_t)buffer[3] << 8) | buffer[2]);
    m_calib.dig_T3 = (int16_t)(((uint16_t)buffer[5] << 8) | buffer[4]);
    
    m_calib.dig_P1 = ((uint16_t)buffer[7] << 8) | buffer[6];
    m_calib.dig_P2 = (int16_t)(((uint16_t)buffer[9] << 8) | buffer[8]);
    m_calib.dig_P3 = (int16_t)(((uint16_t)buffer[11] << 8) | buffer[10]);
    m_calib.dig_P4 = (int16_t)(((uint16_t)buffer[13] << 8) | buffer[12]);
    m_calib.dig_P5 = (int16_t)(((uint16_t)buffer[15] << 8) | buffer[14]);
    m_calib.dig_P6 = (int16_t)(((uint16_t)buffer[17] << 8) | buffer[16]);
    m_calib.dig_P7 = (int16_t)(((uint16_t)buffer[19] << 8) | buffer[18]);
    m_calib.dig_P8 = (int16_t)(((uint16_t)buffer[21] << 8) | buffer[20]);
    m_calib.dig_P9 = (int16_t)(((uint16_t)buffer[23] << 8) | buffer[22]);

    return Sys::Status::OK;
}

float Bmp280Bsp::compensateT(int32_t adcT)
{
    double var1, var2, T;
    var1 = (((double)adcT) / 16384.0 - ((double)m_calib.dig_T1) / 1024.0) * ((double)m_calib.dig_T2);
    var2 = ((((double)adcT) / 131072.0 - ((double)m_calib.dig_T1) / 8192.0) *
            (((double)adcT) / 131072.0 - ((double)m_calib.dig_T1) / 8192.0)) * ((double)m_calib.dig_T3);
    
    m_tFine = (int32_t)(var1 + var2);
    T = (var1 + var2) / 5120.0;
    return (float)T;
}

float Bmp280Bsp::compensateP(int32_t adcP)
{
    double var1, var2, p;
    var1 = ((double)m_tFine / 2.0) - 64000.0;
    var2 = var1 * var1 * ((double)m_calib.dig_P6) / 32768.0;
    var2 = var2 + var1 * ((double)m_calib.dig_P5) * 2.0;
    var2 = (var2 / 4.0) + (((double)m_calib.dig_P4) * 65536.0);
    var1 = (((double)m_calib.dig_P3) * var1 * var1 / 524288.0 + ((double)m_calib.dig_P2) * var1) / 524288.0;
    var1 = (1.0 + var1 / 32768.0) * ((double)m_calib.dig_P1);
    
    if (var1 == 0.0) {
        return 0.0f; // 避免除以 0
    }
    
    p = 1048576.0 - (double)adcP;
    p = (p - (var2 / 4096.0)) * 6250.0 / var1;
    var1 = ((double)m_calib.dig_P9) * p * p / 2147483648.0;
    var2 = p * ((double)m_calib.dig_P8) / 32768.0;
    p = p + (var1 + var2 + ((double)m_calib.dig_P7)) / 16.0;
    return (float)p;
}

Sys::Status Bmp280Bsp::read(float& pressure, float& altitude)
{
    if (!m_initialized) {
        if (init() != Sys::Status::OK) return Sys::Status::ERROR_INIT;
    }

    uint8_t buffer[6] = {0};
    // 从 0xF7 (press_msb) 起始连续读取 6 字节的数据帧
    if (m_i2c.readRegs(SYS_I2C_ADDR_BMP280, 0xF7, buffer, 6) != Sys::Status::OK) {
        return Sys::Status::ERROR_COM_FAIL;
    }

    // 解析原始温湿度与气压的 20-bit 值
    int32_t rawP = ((int32_t)buffer[0] << 12) | ((int32_t)buffer[1] << 4) | ((int32_t)buffer[2] >> 4);
    int32_t rawT = ((int32_t)buffer[3] << 12) | ((int32_t)buffer[4] << 4) | ((int32_t)buffer[5] >> 4);

    // 首先计算出温度，因为气压的补偿参数强依赖于中间因子 m_tFine
    compensateT(rawT);
    
    // 物理气压换算 (单位为 Pa)
    pressure = compensateP(rawP);

    // 海拔高度换算: H = 44330 * (1 - (P/101325)^0.190295)
    if (pressure > 0.0f) {
        altitude = 44330.0f * (1.0f - powf(pressure / 101325.0f, 0.190295f));
    } else {
        altitude = 0.0f;
    }

    return Sys::Status::OK;
}

} // namespace Bsp
