# Pinmap — MycoBrain V1

## SX1262 (Side-B + Gateway)

Authoritative schematic mapping:

- SX_Reset → **GPIO7**
- SX_Busy → **GPIO12**
- SX_CLK → **GPIO18**
- SX_CS → **GPIO17**
- SX_DI01 → **GPIO21**
- SX_DI02 → **GPIO22**
- SX_MISO → **GPIO19**
- SX_MOSI → **GPIO20**

## I2C

Board exposes **SCL** and **SDA** nets wired to ESP32 module pins.
Firmware defaults to SDA=GPIO4, SCL=GPIO5 but allows runtime override via `CMD_SET_I2C`.