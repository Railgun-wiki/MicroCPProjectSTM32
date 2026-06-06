# CubeMX and BSP Boundary

This project keeps pin ownership in CubeMX generated code. BSP classes may drive pins and talk to devices, but they must not configure GPIO mode, pull, speed, alternate function, or peripheral clocks for pins already described in `MicroCPProjectSTM32.ioc`.

## CubeMX-owned configuration

- LCD SPI bus: `SPI1`, mode 0 (`CPOL_LOW`, `CPHA_1EDGE`), prescaler `/2`.
- LCD control pins: `PB6 LCD_LED`, `PB7 LCD_DC`, `PB8 LCD_RST`, `PB9 LCD_CS` as push-pull outputs, high speed.
- Touch bit-bang pins: `PA8 TOUCH_TCLK`, `PB3 TOUCH_TDIN`, `PB4 TOUCH_TCS` as push-pull outputs, high speed.
- Touch inputs: `PA0 TOUCH_PEN`, `PA1 TOUCH_DOUT` as pull-up inputs.
- `PA0/PA1` are touch-only in this project; do not bind `ButtonBsp` to them.
- Sensor bus: `PB10/PB11` are `I2C2_SCL/I2C2_SDA`; do not reuse them as software I2C GPIO once the hardware I2C adapter is enabled.
- Debug port: SWD-only. `PB3/PB4` are used by touch, so JTAG must be disabled while keeping `PA13/PA14` for SWD.

## BSP-owned behavior

- `LcdBsp` owns ST7796S reset timing, command/data writes, address windows, and rendering primitives.
- `TouchBsp` owns XPT2046/ADS7846 command timing, sampling, filtering, and calibration math.
- BSP `init()` methods may put devices into idle/reset states, but must not call `HAL_GPIO_Init()` for CubeMX-owned pins.

## Regeneration checklist

After regenerating code from CubeMX, verify:

- `MX_GPIO_Init()` sets `TOUCH_PEN_Pin|TOUCH_DOUT_Pin` to `GPIO_PULLUP`.
- `MX_GPIO_Init()` sets LCD/touch output pins to `GPIO_SPEED_FREQ_HIGH` with the expected initial levels.
- `MX_SPI1_Init()` keeps `SPI_POLARITY_LOW`, `SPI_PHASE_1EDGE`, and `SPI_BAUDRATEPRESCALER_2`.
- `MX_GPIO_Init()` or its user section keeps `__HAL_AFIO_REMAP_SWJ_NOJTAG()` so `PB3/PB4` are available to touch.
- BSP files do not reconfigure GPIO pins already represented in the `.ioc`.