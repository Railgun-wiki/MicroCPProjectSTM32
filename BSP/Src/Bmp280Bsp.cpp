// BSP/Src/Bmp280Bsp.cpp
#include "Bmp280Bsp.hpp"
#include "stm32f1xx_hal.h"
#include "MathTables.hpp"

namespace Bsp {

Bmp280Bsp::Bmp280Bsp(II2cBus& i2cBus)
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
        SYS_LOG("Error: Failed to write BMP280 config register.");
        return Sys::Status::ERROR_INIT;
    }

    // 4. 配置控制寄存器 0xF4 (ctrl_meas)
    // Temperature oversampling = 2x (010), Pressure oversampling = 16x (101), Mode = Normal (11)
    // 字节值: (2 << 5) | (5 << 2) | 3 = 0x40 | 0x14 | 0x03 = 0x57
    uint8_t ctrlMeasVal = 0x57;
    if (m_i2c.writeRegs(SYS_I2C_ADDR_BMP280, 0xF4, &ctrlMeasVal, 1) != Sys::Status::OK) {
        SYS_LOG("Error: Failed to write BMP280 ctrl_meas register.");
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

int32_t Bmp280Bsp::compensateT(int32_t adcT)
{
    int32_t var1, var2, T;
    var1 = ((((adcT >> 3) - ((int32_t)m_calib.dig_T1 << 1))) * ((int32_t)m_calib.dig_T2)) >> 11;
    var2 = (((((adcT >> 4) - ((int32_t)m_calib.dig_T1)) * ((adcT >> 4) - ((int32_t)m_calib.dig_T1))) >> 12) * ((int32_t)m_calib.dig_T3)) >> 14;
    m_tFine = var1 + var2;
    T = (m_tFine * 5 + 128) >> 8;
    return T; // resolution 0.01 DegC
}

uint32_t Bmp280Bsp::compensateP(int32_t adcP)
{
    int64_t var1, var2, p;
    var1 = ((int64_t)m_tFine) - 128000;
    var2 = var1 * var1 * (int64_t)m_calib.dig_P6;
    var2 = var2 + ((var1 * (int64_t)m_calib.dig_P5) << 17);
    var2 = var2 + (((int64_t)m_calib.dig_P4) << 35);
    var1 = ((var1 * var1 * (int64_t)m_calib.dig_P3) >> 8) + ((var1 * (int64_t)m_calib.dig_P2) << 12);
    var1 = (((((int64_t)1) << 47) + var1)) * ((int64_t)m_calib.dig_P1) >> 33;
    
    if (var1 == 0) {
        return 0; // avoid exception caused by division by zero
    }
    
    p = 1048576 - adcP;
    p = (((p << 31) - var2) * 3125) / var1;
    var1 = (((int64_t)m_calib.dig_P9) * (p >> 13) * (p >> 13)) >> 25;
    var2 = (((int64_t)m_calib.dig_P8) * p) >> 19;
    p = ((p + var1 + var2) >> 8) + (((int64_t)m_calib.dig_P7) << 4);
    
    return (uint32_t)(p / 256); // Output in Pa
}

Sys::Status Bmp280Bsp::read(uint32_t& pressure, int32_t& altitude)
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
    // 注意：内部温度更新，但传感器并不向外暴露该温度（使用AHT20的温度）
    compensateT(rawT);
    
    // 物理气压换算 (单位为 Pa)
    pressure = compensateP(rawP);

    // 海拔高度换算: 查表法避免 powf
    if (pressure > 0) {
        altitude = calculateAltitudeX10(pressure);
    } else {
        altitude = 0;
    }

    return Sys::Status::OK;
}

} // namespace Bsp
