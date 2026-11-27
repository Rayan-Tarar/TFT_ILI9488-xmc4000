# ILI9488 driver for Infineon XMC4700

A small, dependency-free C driver for 320 × 480 ILI9488 TFT displays on the Infineon **XMC4700**, built on **XMCLib** and **ModusToolbox**. It talks to the panel over 4-wire SPI using any USIC channel, and you pass in your pins at init, so nothing in the driver is tied to a particular board.

- Hardware-agnostic: SPI channel and CS / DC / RESET pins are passed in at runtime
- Rectangles, lines, pixels, RGB565 images and a built-in 5×7 scalable font
- Four rotations with correct clipping in every orientation
- Hardware vertical scrolling, display inversion, display on/off
- Fast paths: fills, lines and opaque text stream through a single address window
- Two files (`ili9488.c` / `ili9488.h`), no heap use

<!-- Add a photo of your display here, e.g. ![demo](docs/demo.jpg) -->

## Repository layout

```
ili9488.c            driver
ili9488.h            public API + configuration
examples/basic/      minimal main.c
.cyignore            stops ModusToolbox from building the example into your app
```

## Hardware

| Display pin | Connect to | Notes |
|---|---|---|
| VCC | 3.3 V | |
| GND | GND | |
| LED | 3.3 V (or a GPIO / PWM pin) | Backlight |
| SCK | USIC `SCLKOUT` pin | Configured in the Device Configurator |
| SDI (MOSI) | USIC `DOUT0` pin | Configured in the Device Configurator |
| CS | Any GPIO | Driven by the driver |
| DC / RS | Any GPIO | Driven by the driver |
| RESET | Any GPIO, or tie to 3.3 V | Optional; software reset is used if not wired |
| SDO (MISO) | Not connected | The driver never reads from the panel |

Example pinout (tested on the XMC4700 Relax Kit, USIC1 channel 1):

| Signal | Pin |
|---|---|
| SCLK | P0.10 |
| MOSI | P0.1 |
| CS | P0.2 |
| RESET | P0.3 |
| DC | P0.4 |

## Setup in ModusToolbox

### 1. Add the driver

Copy (or add as a git submodule) this repository into your application folder. ModusToolbox picks up the `.c` files automatically. The included `.cyignore` keeps the example's `main()` out of your build.

### 2. Configure the SPI channel in the Device Configurator

Open `design.modus` and enable a USIC channel with the **SPI** personality:

| Setting | Value |
|---|---|
| Mode | Master |
| Clock polarity / phase | SPI mode 0 (CPOL = 0, CPHA = 0), i.e. `XMC_SPI_CH_BRG_SHIFT_CLOCK_PASSIVE_LEVEL_0_DELAY_ENABLED` |
| Bit order | MSB first |
| Word / frame length | 8 (the driver also sets this) |
| Bus speed | Start at **1,000 kHz**; see [Bus speed](#bus-speed) |

Then route the channel's **SCLKOUT** and **DOUT0** signals to your pins. After saving, check the generated `cycfg_routing.h` for a non-zero `..._ALT` define on both pins. If the define is missing, the pin is a plain GPIO and no SPI signal will reach it.

You don't need a hardware slave-select. If the configurator assigned one (`SELOx`) to your CS pin, that's fine: `ILI9488_Init()` switches the CS pin back to a software-controlled GPIO.

### 3. Initialise and draw

```c
#include "cybsp.h"
#include "ili9488.h"

static const ILI9488_Config_t lcd_cfg =
{
    .spi      = XMC_SPI1_CH1,
    .cs_port  = XMC_GPIO_PORT0, .cs_pin  = 2,
    .dc_port  = XMC_GPIO_PORT0, .dc_pin  = 4,
    .rst_port = XMC_GPIO_PORT0, .rst_pin = 3,   /* or .rst_port = NULL */
};

int main(void)
{
    cybsp_init();                    /* must run first: clocks, pins, SPI */

    ILI9488_Init(&lcd_cfg);
    ILI9488_SetRotation(1);          /* landscape, 480 x 320 */

    ILI9488_FillScreen(ILI9488_WHITE);
    ILI9488_FillRect(40, 30, 400, 80, ILI9488_BLACK);
    ILI9488_PrintText("Hello XMC4700", 87, 54, ILI9488_WHITE, ILI9488_BLACK, 4);

    for (;;) { }
}
```

If you named your pins in the Device Configurator, you can use the generated macros instead of hard-coding them, e.g. `.cs_port = LCD_CS_PORT, .cs_pin = LCD_CS_PIN`.

> `ILI9488_Init()` must be called **after** `cybsp_init()`. Otherwise the USIC channel isn't configured yet.

## API

All coordinates are in pixels for the current rotation, with (0, 0) at the top left. Anything drawn partly off-screen is clipped.

### Setup

| Function | Description |
|---|---|
| `ILI9488_Init(const ILI9488_Config_t *cfg)` | Reset and initialise the panel. Starts in rotation 0. |
| `ILI9488_SetRotation(uint8_t r)` | 0 / 2 = portrait (320 × 480), 1 / 3 = landscape (480 × 320) |
| `ILI9488_GetWidth()` / `ILI9488_GetHeight()` | Current screen size, after rotation |
| `ILI9488_InvertDisplay(bool)` | Invert colours |
| `ILI9488_DisplayOn(bool)` | Turn the panel output on or off (GRAM is kept) |

### Drawing

| Function | Description |
|---|---|
| `ILI9488_FillScreen(color)` | Fill the whole screen |
| `ILI9488_FillRect(x, y, w, h, color)` | Filled rectangle |
| `ILI9488_DrawRect(x, y, w, h, color)` | Rectangle outline |
| `ILI9488_DrawPixel(x, y, color)` | Single pixel |
| `ILI9488_DrawHLine(x, y, w, color)` / `ILI9488_DrawVLine(x, y, h, color)` | Fast horizontal / vertical lines |
| `ILI9488_DrawLine(x0, y0, x1, y1, color)` | Any line (Bresenham) |
| `ILI9488_DrawImage(img, x, y, w, h)` | RGB565 bitmap: `w*h` pixels, 2 bytes each, high byte first, row-major |

### Text

| Function | Description |
|---|---|
| `ILI9488_PrintText(text, x, y, color, bg, size)` | Draw a string. `\n` moves to the next line at the original `x`. |
| `ILI9488_DrawChar(x, y, c, color, bg, size)` | Draw one character |
| `ILI9488_SetCP437(bool)` | Use the true code page 437 layout for characters ≥ 176 |

Each character occupies a **6 × 8** cell multiplied by `size`. At size 4 that's 24 × 32 px, so a string is `6 * size * strlen(text)` pixels wide.

- **Opaque text** (`bg != color`) paints the whole cell, which is fast and overwrites what's underneath.
- **Transparent text** (`bg == color`) writes only the glyph pixels. It's slower, but keeps the background.

Centring a string in a box:

```c
int16_t tx = box_x + (box_w - 6 * size * (int16_t)strlen(s)) / 2;
int16_t ty = box_y + (box_h - 8 * size) / 2;
```

### Colours

Colours are 16-bit **RGB565**. Predefined colours include `ILI9488_BLACK`, `WHITE`, `RED`, `GREEN`, `BLUE`, `CYAN`, `MAGENTA`, `YELLOW`, `ORANGE`, `NAVY`, `DARKGREEN` and others (see `ili9488.h`). Build your own with:

```c
uint16_t teal = ILI9488_Color565(0, 128, 128);
```

Over SPI the ILI9488 only accepts 18-bit colour (3 bytes per pixel), so the driver converts RGB565 on the fly.

### Scrolling

```c
ILI9488_SetScrollArea(top_fixed, bottom_fixed);   /* lines along the panel's native 480-line axis */
ILI9488_Scroll(line);
```

Hardware scrolling always runs along the panel's native long axis, whatever the rotation.

### Low-level access

`ILI9488_SendCommand()`, `ILI9488_SendData()` and `ILI9488_SetAddrWindow()` let you send raw commands. Each call asserts and releases CS on its own.

## Compile-time options

Define these before including `ili9488.h`, or pass them as compiler flags (e.g. `DEFINES+=...` in the ModusToolbox Makefile):

| Macro | Default | Purpose |
|---|---|---|
| `ILI9488_DELAY_MS(ms)` | `XMC_Delay(ms)` | Millisecond delay used during reset. Point it at your RTOS delay if you use one. |
| `ILI9488_MADCTL_COLOR_ORDER` | `ILI9488_MADCTL_BGR` | Set to `ILI9488_MADCTL_RGB` if red and blue are swapped on your panel |
| `ILI9488_CHUNK_PIXELS` | `128` | Pixels buffered per SPI burst (3 bytes each, on the stack) |
| `ILI9488_TEXT_FAST_MAX_SIZE` | `8` | Largest text size that uses the fast single-window path |

## Bus speed

The ILI9488 datasheet specifies a maximum serial **write** clock of about **15 MHz**. Many modules run faster with short wires, but that's out of spec, so test it rather than assuming it works.

| SCLK | Full-screen fill (theoretical) |
|---|---|
| 1 MHz | ~3.7 s |
| 10 MHz | ~0.37 s |
| 15 MHz | ~0.25 s |

Recommended approach:

1. Bring the display up at 1 MHz.
2. Raise the clock toward 10–15 MHz.
3. Above about 10 MHz, set the SCLK and MOSI pins to a strong driver in the pin personality, and keep the wires short.

Real throughput is lower than these numbers because transfers are blocking and byte-by-byte (see [Limitations](#limitations)).

## Troubleshooting

| Symptom | Likely cause |
|---|---|
| Screen stays white (backlight only) | No SPI traffic reaches the panel. Check that SCLK/MOSI are routed as `ALT` functions (`cycfg_routing.h`), that `cybsp_init()` runs before `ILI9488_Init()`, and the wiring. Put a scope on SCLK. |
| Random noise, then nothing | Init works, but drawing doesn't reach the panel, or the bus is too fast. Lower the bus speed. |
| Red and blue swapped | Define `ILI9488_MADCTL_COLOR_ORDER=ILI9488_MADCTL_RGB` |
| Image mirrored or upside down | Try another rotation (e.g. 3 instead of 1) |
| Corrupted or shifted pixels at high speed | Signal integrity. Lower the clock, use a strong pad driver, and shorten the wires. |
| Drawing is extremely slow | Check the bus speed units: 19.2 means **kHz** in the configurator, not MHz. |

## Limitations

- Write-only: MISO isn't used, and there are no read-back functions.
- Transfers are blocking and polled byte-by-byte. A TX FIFO or DMA backend would raise throughput at high clock speeds.
- Only one display instance per build.

Contributions are welcome, especially a FIFO/DMA transfer backend.

## Credits

- Original STM32 HAL ILI9488 driver by **timagr615** (2021)
- XMC4700 port, fixes and refactor by **Rayan Tarar** (2026)
- 5×7 font from the classic Adafruit GFX glyph table

## License

MIT. See [LICENSE](LICENSE).
