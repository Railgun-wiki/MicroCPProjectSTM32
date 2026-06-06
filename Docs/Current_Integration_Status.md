# Current Integration Status

This file is the maintenance-oriented snapshot of the project after the LCD/touch and I2C2 cleanup work.

## Active hardware mapping

- `SPI1` drives the LCD in mode 0: `CPOL_LOW`, `CPHA_1EDGE`, prescaler `/2`.
- LCD control GPIO:
  - `PB6` -> `LCD_LED`
  - `PB7` -> `LCD_DC`
  - `PB8` -> `LCD_RST`
  - `PB9` -> `LCD_CS`
- Touch GPIO:
  - `PA0` -> `TOUCH_PEN`
  - `PA1` -> `TOUCH_DOUT`
  - `PA8` -> `TOUCH_TCLK`
  - `PB3` -> `TOUCH_TDIN`
  - `PB4` -> `TOUCH_TCS`
- Sensor bus:
  - `PB10` -> `I2C2_SCL`
  - `PB11` -> `I2C2_SDA`

## Ownership boundary

- CubeMX owns pin mode, pull, speed, initial state, alternate function, DMA, and peripheral clock setup.
- `LcdBsp` owns LCD reset timing and LCD command/data transport.
- `TouchBsp` owns touch sampling, filtering, and calibration.
- `HardwareI2cBsp` is the default sensor bus wrapper around `HAL_I2C_*`.
- `SoftI2cBsp` remains in the tree only as an alternate implementation, not the default runtime path.

## Input model

- `PA0/PA1` are touch-only and must not be rebound to `ButtonBsp`.
- `AppController` still accepts abstract page/mute button dependencies, but the current app wiring uses null button instances.
- Page switching is currently available through touch input on the right half of the screen.

## Debug and remap requirements

- `PB3/PB4` are used by touch, so the debug port must stay in SWD-only mode.
- `MicroCPProjectSTM32.ioc` keeps `SYS.Debug=SYS_DEBUG_SERIAL_WIRE`.
- `MX_GPIO_Init()` user code keeps `__HAL_AFIO_REMAP_SWJ_NOJTAG()`.

## Generated code expectations

After CubeMX regeneration, the following should still be true:

- `HAL_DMA_MODULE_ENABLED` is on.
- `SPI1` RX/TX DMA is wired to `DMA1_Channel2` and `DMA1_Channel3`.
- `TOUCH_PEN` and `TOUCH_DOUT` remain pull-up inputs.
- LCD/touch output GPIO remain high-speed outputs with the expected idle levels.
