#include "MathTables.hpp"

namespace Bsp {

const uint16_t PWM_SINE_LUT[64] = {
     15,  17,  24,  35,  50,  70,  93, 121,
    151, 185, 222, 261, 302, 345, 389, 434,
    480, 526, 571, 615, 658, 699, 738, 775,
    809, 839, 867, 890, 910, 925, 936, 943,
    945, 943, 936, 925, 910, 890, 867, 839,
    809, 775, 738, 699, 658, 615, 571, 526,
    480, 434, 389, 345, 302, 261, 222, 185,
    151, 121,  93,  70,  50,  35,  24,  17,
};

const uint32_t ALT_LUT_BASE_PRESSURE = 80000;
const uint32_t ALT_LUT_STEP = 1000;
const uint32_t ALT_LUT_SIZE = 31;

const int32_t ALTITUDE_LUT_X10[] = {
    19493, 18490, 17497, 16513, 15540, 14575, 13620, 12674,
    11736, 10807,  9886,  8974,  8070,  7174,  6285,  5404,
     4531,  3665,  2806,  1954,  1109,   271,  -560, -1385,
    -2204, -3016, -3821, -4621, -5415, -6202, -6984,
};

int32_t calculateAltitudeX10(uint32_t pressure)
{
    if (pressure <= ALT_LUT_BASE_PRESSURE) {
        return ALTITUDE_LUT_X10[0];
    }
    
    uint32_t maxPressure = ALT_LUT_BASE_PRESSURE + (ALT_LUT_SIZE - 1) * ALT_LUT_STEP;
    if (pressure >= maxPressure) {
        return ALTITUDE_LUT_X10[ALT_LUT_SIZE - 1];
    }
    
    uint32_t diff = pressure - ALT_LUT_BASE_PRESSURE;
    uint32_t index = diff / ALT_LUT_STEP;
    uint32_t remainder = diff % ALT_LUT_STEP;
    
    int32_t y0 = ALTITUDE_LUT_X10[index];
    int32_t y1 = ALTITUDE_LUT_X10[index + 1];
    
    // Linear interpolation
    int32_t dy = y1 - y0;
    int32_t interp = (dy * (int32_t)remainder) / (int32_t)ALT_LUT_STEP;
    
    return y0 + interp;
}

} // namespace Bsp
