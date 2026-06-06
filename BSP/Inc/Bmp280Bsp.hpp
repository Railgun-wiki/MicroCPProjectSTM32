// BSP/Inc/Bmp280Bsp.hpp
#pragma once
#include "IPressureSensor.hpp"
#include "II2cBus.hpp"

namespace Bsp {

class Bmp280Bsp : public App::IPressureSensor {
public:
    Bmp280Bsp(II2cBus& i2cBus);
    
    Sys::Status init() override;
    Sys::Status read(float& pressure, float& altitude) override;

private:
    // 出厂标定参数结构体 (dig_T1 ~ dig_T3, dig_P1 ~ dig_P9)
    struct CalibData {
        uint16_t dig_T1;
        int16_t  dig_T2;
        int16_t  dig_T3;
        uint16_t dig_P1;
        int16_t  dig_P2;
        int16_t  dig_P3;
        int16_t  dig_P4;
        int16_t  dig_P5;
        int16_t  dig_P6;
        int16_t  dig_P7;
        int16_t  dig_P8;
        int16_t  dig_P9;
    } m_calib;

    II2cBus& m_i2c;
    int32_t m_tFine{0}; // 气压计算必须依赖温度微调因子
    bool m_initialized{false};

    Sys::Status loadCalibration();
    float compensateT(int32_t adcT);
    float compensateP(int32_t adcP);
};

} // namespace Bsp
