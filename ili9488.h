/*
 * ili9488.h - ILI9488 TFT LCD driver for Infineon XMC4700 (XMCLib / ModusToolbox)
 *
 * 4-wire SPI (SCLK, MOSI, CS, D/C) + optional RESET line.
 * The display is driven in 18-bit colour mode (3 bytes per pixel); the API
 * takes RGB565 colours and converts them on the fly.
 *
 * Ported from the STM32 HAL ILI9488 driver by timagr615 (2021).
 * XMC4700 port and refactor: Rayan Tarar (2026).
 */

#ifndef ILI9488_H
#define ILI9488_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#include "xmc_spi.h"
#include "xmc_gpio.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ------------------------------------------------------------------------- */
/* User-overridable settings (define before including, or via compiler flags) */
/* ------------------------------------------------------------------------- */

/* Millisecond delay used during reset/power-up. */
#ifndef ILI9488_DELAY_MS
#define ILI9488_DELAY_MS(ms)        XMC_Delay(ms)
#endif

/* Colour order bit written to MADCTL. If red and blue appear swapped on your
 * panel, build with -DILI9488_MADCTL_COLOR_ORDER=ILI9488_MADCTL_RGB */
#ifndef ILI9488_MADCTL_COLOR_ORDER
#define ILI9488_MADCTL_COLOR_ORDER  ILI9488_MADCTL_BGR
#endif

/* ------------------------------------------------------------------------- */
/* Panel geometry (native portrait orientation)                              */
/* ------------------------------------------------------------------------- */
#define ILI9488_NATIVE_WIDTH    320
#define ILI9488_NATIVE_HEIGHT   480

/* ------------------------------------------------------------------------- */
/* Command set                                                               */
/* ------------------------------------------------------------------------- */
#define ILI9488_NOP         0x00
#define ILI9488_SWRESET     0x01
#define ILI9488_RDDID       0x04
#define ILI9488_RDDST       0x09
#define ILI9488_SLPIN       0x10
#define ILI9488_SLPOUT      0x11
#define ILI9488_PTLON       0x12
#define ILI9488_NORON       0x13
#define ILI9488_INVOFF      0x20
#define ILI9488_INVON       0x21
#define ILI9488_DISPOFF     0x28
#define ILI9488_DISPON      0x29
#define ILI9488_CASET       0x2A
#define ILI9488_PASET       0x2B
#define ILI9488_RAMWR       0x2C
#define ILI9488_RAMRD       0x2E
#define ILI9488_VSCRDEF     0x33
#define ILI9488_MADCTL      0x36
#define ILI9488_VSCRSADD    0x37
#define ILI9488_PIXFMT      0x3A
#define ILI9488_IFMODE      0xB0
#define ILI9488_FRMCTR1     0xB1
#define ILI9488_INVCTR      0xB4
#define ILI9488_DFUNCTR     0xB6
#define ILI9488_PWCTR1      0xC0
#define ILI9488_PWCTR2      0xC1
#define ILI9488_VMCTR1      0xC5
#define ILI9488_GMCTRP1     0xE0
#define ILI9488_GMCTRN1     0xE1
#define ILI9488_SETIMAGE    0xE9
#define ILI9488_ADJCTL3     0xF7

/* MADCTL bits */
#define ILI9488_MADCTL_MY   0x80
#define ILI9488_MADCTL_MX   0x40
#define ILI9488_MADCTL_MV   0x20
#define ILI9488_MADCTL_ML   0x10
#define ILI9488_MADCTL_RGB  0x00
#define ILI9488_MADCTL_BGR  0x08
#define ILI9488_MADCTL_MH   0x04

/* ------------------------------------------------------------------------- */
/* RGB565 colours                                                            */
/* ------------------------------------------------------------------------- */
#define ILI9488_BLACK       0x0000
#define ILI9488_NAVY        0x000F
#define ILI9488_DARKGREEN   0x03E0
#define ILI9488_DARKCYAN    0x03EF
#define ILI9488_MAROON      0x7800
#define ILI9488_PURPLE      0x780F
#define ILI9488_OLIVE       0x7BE0
#define ILI9488_LIGHTGREY   0xC618
#define ILI9488_DARKGREY    0x7BEF
#define ILI9488_BLUE        0x001F
#define ILI9488_GREEN       0x07E0
#define ILI9488_CYAN        0x07FF
#define ILI9488_RED         0xF800
#define ILI9488_MAGENTA     0xF81F
#define ILI9488_YELLOW      0xFFE0
#define ILI9488_WHITE       0xFFFF
#define ILI9488_ORANGE      0xFD20
#define ILI9488_GREENYELLOW 0xAFE5
#define ILI9488_PINK        0xF81F

/* ------------------------------------------------------------------------- */
/* Hardware configuration                                                    */
/* ------------------------------------------------------------------------- */
typedef struct
{
    XMC_USIC_CH_t   *spi;       /* USIC channel already initialised as SPI master, e.g. XMC_SPI1_CH1 */

    XMC_GPIO_PORT_t *cs_port;   /* Chip select (driven as GPIO by this driver) */
    uint8_t          cs_pin;

    XMC_GPIO_PORT_t *dc_port;   /* Data/Command (a.k.a. RS) */
    uint8_t          dc_pin;

    XMC_GPIO_PORT_t *rst_port;  /* Reset; set to NULL if RESET is tied high -> software reset is used */
    uint8_t          rst_pin;
} ILI9488_Config_t;

/* ------------------------------------------------------------------------- */
/* Setup                                                                     */
/* ------------------------------------------------------------------------- */
void     ILI9488_Init(const ILI9488_Config_t *cfg);
void     ILI9488_SetRotation(uint8_t rotation);      /* 0,2 = portrait; 1,3 = landscape */
uint16_t ILI9488_GetWidth(void);
uint16_t ILI9488_GetHeight(void);
void     ILI9488_InvertDisplay(bool invert);
void     ILI9488_DisplayOn(bool on);

/* ------------------------------------------------------------------------- */
/* Drawing                                                                   */
/* ------------------------------------------------------------------------- */
void ILI9488_FillScreen(uint16_t color);
void ILI9488_FillRect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color);
void ILI9488_DrawRect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color);
void ILI9488_DrawPixel(int16_t x, int16_t y, uint16_t color);
void ILI9488_DrawHLine(int16_t x, int16_t y, int16_t w, uint16_t color);
void ILI9488_DrawVLine(int16_t x, int16_t y, int16_t h, uint16_t color);
void ILI9488_DrawLine(int16_t x0, int16_t y0, int16_t x1, int16_t y1, uint16_t color);

/* img: RGB565 pixels, 2 bytes each, big-endian (high byte first), row-major, w*h pixels */
void ILI9488_DrawImage(const uint8_t *img, int16_t x, int16_t y, int16_t w, int16_t h);

/* ------------------------------------------------------------------------- */
/* Text (built-in 5x7 font in a 6x8 cell, scaled by `size`)                  */
/* bg == color draws transparent text (only the glyph pixels are written).   */
/* ------------------------------------------------------------------------- */
void ILI9488_DrawChar(int16_t x, int16_t y, unsigned char c, uint16_t color, uint16_t bg, uint8_t size);
void ILI9488_PrintText(const char *text, int16_t x, int16_t y, uint16_t color, uint16_t bg, uint8_t size);
void ILI9488_SetCP437(bool enable);

/* ------------------------------------------------------------------------- */
/* Utilities                                                                 */
/* ------------------------------------------------------------------------- */
uint16_t ILI9488_Color565(uint8_t r, uint8_t g, uint8_t b);

/* Hardware vertical scrolling (along the panel's native 480-line axis) */
void ILI9488_SetScrollArea(uint16_t top_fixed, uint16_t bottom_fixed);
void ILI9488_Scroll(uint16_t line);

/* Low-level access: each call asserts and releases CS on its own */
void ILI9488_SendCommand(uint8_t cmd);
void ILI9488_SendData(const uint8_t *data, size_t len);
void ILI9488_SetAddrWindow(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1); /* ends with RAMWR */

#ifdef __cplusplus
}
#endif

#endif /* ILI9488_H */
