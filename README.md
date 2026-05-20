# POCKET_SPRITE

A custom ESP32-S3 handheld game console with a joystick, action button, TFT display, buzzer, battery power, custom PCB, and built-in mini games.

POCKET_SPRITE is a compact original hardware project built around the Seeed Studio XIAO ESP32-S3 and a 1.8" ST7735 SPI TFT display. The board combines custom electronics, embedded firmware, pixel graphics, and playful silkscreen art into a small battery-powered handheld.

## Features

- Seeed Studio XIAO ESP32-S3
- 1.8" ST7735 SPI TFT display
- Joystick input
- One action button
- Buzzer output
- Battery input through the XIAO battery pads
- Switched 3.3V rail for game hardware
- Custom PCB outline
- Custom silkscreen artwork
- LovyanGFX display driver
- Multiple built-in games and modes

## Games / Modes

Current firmware includes:

- **Adventure**  
  A small character-based adventure game.

- **Pocket Pet**  
  A virtual pet room with movement, jumping, ball pushing, and ball kicking.

- **Dress Up**  
  A character creator with selectable gender, hairstyles, colors, outfits, shoes, and a small playable character.

- **BM Tron**  
  A light-cycle style trail game.

- **Snake**  
  A classic snake mini game.

## Hardware

| Function | Part / Notes |
|---|---|
| MCU | Seeed Studio XIAO ESP32-S3 |
| Display | 1.8" ST7735 SPI TFT |
| Input | Joystick + one A button |
| Audio | Buzzer driven from GPIO |
| Power | Battery connected to XIAO BAT pads |
| Switched rail | XIAO_3V3 → switch → +3V3_SW |

## Pin Map

| XIAO Pin | Net | Function |
|---|---|---|
| D0 | JOY_SW | Joystick press |
| D1 | JOY_X | Joystick X analog |
| D2 | JOY_Y | Joystick Y analog |
| D3 | A_JUMP | A / jump button |
| D4 | CS | TFT chip select |
| D5 | DC | TFT data/command |
| D6 | RST | TFT reset |
| D7 | B-LITE | TFT backlight |
| D8 | SCK | TFT SPI clock |
| D9 | CHIRP | Buzzer drive |
| D10 | MOSI | TFT SPI MOSI |
| 3V3 | XIAO_3V3 | Regulated 3.3V from XIAO |
| GND | GND | Ground |
| BAT+ | XIAO_BAT+ | Battery positive |
| BAT- | XIAO_BAT- | Battery negative |

## Power Design

The battery connects directly to the XIAO battery pads. The XIAO provides the regulated 3.3V rail used by the game hardware.

```text
Battery → XIAO BAT pads
XIAO_3V3 → switch → +3V3_SW