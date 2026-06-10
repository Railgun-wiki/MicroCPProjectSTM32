#ifndef MATH_TABLES_HPP
#define MATH_TABLES_HPP

#include <stdint.h>

namespace Bsp {

// 64-element sine lookup table for PWM breathing, mapped to 15 ~ 945
extern const uint16_t PWM_SINE_LUT[64];

// Base pressure in Pa for altitude LUT
extern const uint32_t ALT_LUT_BASE_PRESSURE;
// Step size in Pa between LUT entries
extern const uint32_t ALT_LUT_STEP;
// Number of entries in altitude LUT
extern const uint32_t ALT_LUT_SIZE;

// Altitude values * 10 (e.g. 19493 means 1949.3 m)
extern const int32_t ALTITUDE_LUT_X10[];

/**
 * @brief Calculate altitude x10 from pressure in Pa using piecewise linear interpolation
 * @param pressure Pressure in Pa
 * @return Estimated altitude * 10 in meters
 */
int32_t calculateAltitudeX10(uint32_t pressure);

} // namespace Bsp

#endif // MATH_TABLES_HPP
