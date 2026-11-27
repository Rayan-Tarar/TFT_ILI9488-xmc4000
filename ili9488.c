/*
 * ili9488.c - ILI9488 TFT LCD driver for Infineon XMC4700 (XMCLib / ModusToolbox)
 *
 * See ili9488.h and README.md for usage.
 *
 * Ported from the STM32 HAL ILI9488 driver by timagr615 (2021).
 * XMC4700 port and refactor: Rayan Tarar (2026).
 */

#include "ili9488.h"

/* Pixels buffered per SPI burst for fills and images (3 bytes each, on the stack). */
#ifndef ILI9488_CHUNK_PIXELS
#define ILI9488_CHUNK_PIXELS        128
#endif

/* Largest text size that uses the fast (single window) glyph path. */
#ifndef ILI9488_TEXT_FAST_MAX_SIZE
#define ILI9488_TEXT_FAST_MAX_SIZE  8
#endif

/* ------------------------------------------------------------------------- */
/* Driver state                                                              */
/* ------------------------------------------------------------------------- */
static ILI9488_Config_t s_cfg;
static bool     s_ready  = false;
static uint16_t s_width  = ILI9488_NATIVE_WIDTH;
static uint16_t s_height = ILI9488_NATIVE_HEIGHT;
static bool     s_cp437  = false;

/* Classic 5x7 font, 256 glyphs x 5 column bytes (LSB = top row). */
static const uint8_t s_font[256 * 5] = {
    0x00, 0x00, 0x00, 0x00, 0x00,
    0x3E, 0x5B, 0x4F, 0x5B, 0x3E,
    0x3E, 0x6B, 0x4F, 0x6B, 0x3E,
    0x1C, 0x3E, 0x7C, 0x3E, 0x1C,
    0x18, 0x3C, 0x7E, 0x3C, 0x18,
    0x1C, 0x57, 0x7D, 0x57, 0x1C,
    0x1C, 0x5E, 0x7F, 0x5E, 0x1C,
    0x00, 0x18, 0x3C, 0x18, 0x00,
    0xFF, 0xE7, 0xC3, 0xE7, 0xFF,
    0x00, 0x18, 0x24, 0x18, 0x00,
    0xFF, 0xE7, 0xDB, 0xE7, 0xFF,
    0x30, 0x48, 0x3A, 0x06, 0x0E,
    0x26, 0x29, 0x79, 0x29, 0x26,
    0x40, 0x7F, 0x05, 0x05, 0x07,
    0x40, 0x7F, 0x05, 0x25, 0x3F,
    0x5A, 0x3C, 0xE7, 0x3C, 0x5A,
    0x7F, 0x3E, 0x1C, 0x1C, 0x08,
    0x08, 0x1C, 0x1C, 0x3E, 0x7F,
    0x14, 0x22, 0x7F, 0x22, 0x14,
    0x5F, 0x5F, 0x00, 0x5F, 0x5F,
    0x06, 0x09, 0x7F, 0x01, 0x7F,
    0x00, 0x66, 0x89, 0x95, 0x6A,
    0x60, 0x60, 0x60, 0x60, 0x60,
    0x94, 0xA2, 0xFF, 0xA2, 0x94,
    0x08, 0x04, 0x7E, 0x04, 0x08,
    0x10, 0x20, 0x7E, 0x20, 0x10,
    0x08, 0x08, 0x2A, 0x1C, 0x08,
    0x08, 0x1C, 0x2A, 0x08, 0x08,
    0x1E, 0x10, 0x10, 0x10, 0x10,
    0x0C, 0x1E, 0x0C, 0x1E, 0x0C,
    0x30, 0x38, 0x3E, 0x38, 0x30,
    0x06, 0x0E, 0x3E, 0x0E, 0x06,
    0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x5F, 0x00, 0x00,
    0x00, 0x07, 0x00, 0x07, 0x00,
    0x14, 0x7F, 0x14, 0x7F, 0x14,
    0x24, 0x2A, 0x7F, 0x2A, 0x12,
    0x23, 0x13, 0x08, 0x64, 0x62,
    0x36, 0x49, 0x56, 0x20, 0x50,
    0x00, 0x08, 0x07, 0x03, 0x00,
    0x00, 0x1C, 0x22, 0x41, 0x00,
    0x00, 0x41, 0x22, 0x1C, 0x00,
    0x2A, 0x1C, 0x7F, 0x1C, 0x2A,
    0x08, 0x08, 0x3E, 0x08, 0x08,
    0x00, 0x80, 0x70, 0x30, 0x00,
    0x08, 0x08, 0x08, 0x08, 0x08,
    0x00, 0x00, 0x60, 0x60, 0x00,
    0x20, 0x10, 0x08, 0x04, 0x02,
    0x3E, 0x51, 0x49, 0x45, 0x3E,
    0x00, 0x42, 0x7F, 0x40, 0x00,
    0x72, 0x49, 0x49, 0x49, 0x46,
    0x21, 0x41, 0x49, 0x4D, 0x33,
    0x18, 0x14, 0x12, 0x7F, 0x10,
    0x27, 0x45, 0x45, 0x45, 0x39,
    0x3C, 0x4A, 0x49, 0x49, 0x31,
    0x41, 0x21, 0x11, 0x09, 0x07,
    0x36, 0x49, 0x49, 0x49, 0x36,
    0x46, 0x49, 0x49, 0x29, 0x1E,
    0x00, 0x00, 0x14, 0x00, 0x00,
    0x00, 0x40, 0x34, 0x00, 0x00,
    0x00, 0x08, 0x14, 0x22, 0x41,
    0x14, 0x14, 0x14, 0x14, 0x14,
    0x00, 0x41, 0x22, 0x14, 0x08,
    0x02, 0x01, 0x59, 0x09, 0x06,
    0x3E, 0x41, 0x5D, 0x59, 0x4E,
    0x7C, 0x12, 0x11, 0x12, 0x7C,
    0x7F, 0x49, 0x49, 0x49, 0x36,
    0x3E, 0x41, 0x41, 0x41, 0x22,
    0x7F, 0x41, 0x41, 0x41, 0x3E,
    0x7F, 0x49, 0x49, 0x49, 0x41,
    0x7F, 0x09, 0x09, 0x09, 0x01,
    0x3E, 0x41, 0x41, 0x51, 0x73,
    0x7F, 0x08, 0x08, 0x08, 0x7F,
    0x00, 0x41, 0x7F, 0x41, 0x00,
    0x20, 0x40, 0x41, 0x3F, 0x01,
    0x7F, 0x08, 0x14, 0x22, 0x41,
    0x7F, 0x40, 0x40, 0x40, 0x40,
    0x7F, 0x02, 0x1C, 0x02, 0x7F,
    0x7F, 0x04, 0x08, 0x10, 0x7F,
    0x3E, 0x41, 0x41, 0x41, 0x3E,
    0x7F, 0x09, 0x09, 0x09, 0x06,
    0x3E, 0x41, 0x51, 0x21, 0x5E,
    0x7F, 0x09, 0x19, 0x29, 0x46,
    0x26, 0x49, 0x49, 0x49, 0x32,
    0x03, 0x01, 0x7F, 0x01, 0x03,
    0x3F, 0x40, 0x40, 0x40, 0x3F,
    0x1F, 0x20, 0x40, 0x20, 0x1F,
    0x3F, 0x40, 0x38, 0x40, 0x3F,
    0x63, 0x14, 0x08, 0x14, 0x63,
    0x03, 0x04, 0x78, 0x04, 0x03,
    0x61, 0x59, 0x49, 0x4D, 0x43,
    0x00, 0x7F, 0x41, 0x41, 0x41,
    0x02, 0x04, 0x08, 0x10, 0x20,
    0x00, 0x41, 0x41, 0x41, 0x7F,
    0x04, 0x02, 0x01, 0x02, 0x04,
    0x40, 0x40, 0x40, 0x40, 0x40,
    0x00, 0x03, 0x07, 0x08, 0x00,
    0x20, 0x54, 0x54, 0x78, 0x40,
    0x7F, 0x28, 0x44, 0x44, 0x38,
    0x38, 0x44, 0x44, 0x44, 0x28,
    0x38, 0x44, 0x44, 0x28, 0x7F,
    0x38, 0x54, 0x54, 0x54, 0x18,
    0x00, 0x08, 0x7E, 0x09, 0x02,
    0x18, 0xA4, 0xA4, 0x9C, 0x78,
    0x7F, 0x08, 0x04, 0x04, 0x78,
    0x00, 0x44, 0x7D, 0x40, 0x00,
    0x20, 0x40, 0x40, 0x3D, 0x00,
    0x7F, 0x10, 0x28, 0x44, 0x00,
    0x00, 0x41, 0x7F, 0x40, 0x00,
    0x7C, 0x04, 0x78, 0x04, 0x78,
    0x7C, 0x08, 0x04, 0x04, 0x78,
    0x38, 0x44, 0x44, 0x44, 0x38,
    0xFC, 0x18, 0x24, 0x24, 0x18,
    0x18, 0x24, 0x24, 0x18, 0xFC,
    0x7C, 0x08, 0x04, 0x04, 0x08,
    0x48, 0x54, 0x54, 0x54, 0x24,
    0x04, 0x04, 0x3F, 0x44, 0x24,
    0x3C, 0x40, 0x40, 0x20, 0x7C,
    0x1C, 0x20, 0x40, 0x20, 0x1C,
    0x3C, 0x40, 0x30, 0x40, 0x3C,
    0x44, 0x28, 0x10, 0x28, 0x44,
    0x4C, 0x90, 0x90, 0x90, 0x7C,
    0x44, 0x64, 0x54, 0x4C, 0x44,
    0x00, 0x08, 0x36, 0x41, 0x00,
    0x00, 0x00, 0x77, 0x00, 0x00,
    0x00, 0x41, 0x36, 0x08, 0x00,
    0x02, 0x01, 0x02, 0x04, 0x02,
    0x3C, 0x26, 0x23, 0x26, 0x3C,
    0x1E, 0xA1, 0xA1, 0x61, 0x12,
    0x3A, 0x40, 0x40, 0x20, 0x7A,
    0x38, 0x54, 0x54, 0x55, 0x59,
    0x21, 0x55, 0x55, 0x79, 0x41,
    0x22, 0x54, 0x54, 0x78, 0x42,
    0x21, 0x55, 0x54, 0x78, 0x40,
    0x20, 0x54, 0x55, 0x79, 0x40,
    0x0C, 0x1E, 0x52, 0x72, 0x12,
    0x39, 0x55, 0x55, 0x55, 0x59,
    0x39, 0x54, 0x54, 0x54, 0x59,
    0x39, 0x55, 0x54, 0x54, 0x58,
    0x00, 0x00, 0x45, 0x7C, 0x41,
    0x00, 0x02, 0x45, 0x7D, 0x42,
    0x00, 0x01, 0x45, 0x7C, 0x40,
    0x7D, 0x12, 0x11, 0x12, 0x7D,
    0xF0, 0x28, 0x25, 0x28, 0xF0,
    0x7C, 0x54, 0x55, 0x45, 0x00,
    0x20, 0x54, 0x54, 0x7C, 0x54,
    0x7C, 0x0A, 0x09, 0x7F, 0x49,
    0x32, 0x49, 0x49, 0x49, 0x32,
    0x3A, 0x44, 0x44, 0x44, 0x3A,
    0x32, 0x4A, 0x48, 0x48, 0x30,
    0x3A, 0x41, 0x41, 0x21, 0x7A,
    0x3A, 0x42, 0x40, 0x20, 0x78,
    0x00, 0x9D, 0xA0, 0xA0, 0x7D,
    0x3D, 0x42, 0x42, 0x42, 0x3D,
    0x3D, 0x40, 0x40, 0x40, 0x3D,
    0x3C, 0x24, 0xFF, 0x24, 0x24,
    0x48, 0x7E, 0x49, 0x43, 0x66,
    0x2B, 0x2F, 0xFC, 0x2F, 0x2B,
    0xFF, 0x09, 0x29, 0xF6, 0x20,
    0xC0, 0x88, 0x7E, 0x09, 0x03,
    0x20, 0x54, 0x54, 0x79, 0x41,
    0x00, 0x00, 0x44, 0x7D, 0x41,
    0x30, 0x48, 0x48, 0x4A, 0x32,
    0x38, 0x40, 0x40, 0x22, 0x7A,
    0x00, 0x7A, 0x0A, 0x0A, 0x72,
    0x7D, 0x0D, 0x19, 0x31, 0x7D,
    0x26, 0x29, 0x29, 0x2F, 0x28,
    0x26, 0x29, 0x29, 0x29, 0x26,
    0x30, 0x48, 0x4D, 0x40, 0x20,
    0x38, 0x08, 0x08, 0x08, 0x08,
    0x08, 0x08, 0x08, 0x08, 0x38,
    0x2F, 0x10, 0xC8, 0xAC, 0xBA,
    0x2F, 0x10, 0x28, 0x34, 0xFA,
    0x00, 0x00, 0x7B, 0x00, 0x00,
    0x08, 0x14, 0x2A, 0x14, 0x22,
    0x22, 0x14, 0x2A, 0x14, 0x08,
    0x55, 0x00, 0x55, 0x00, 0x55,
    0xAA, 0x55, 0xAA, 0x55, 0xAA,
    0xFF, 0x55, 0xFF, 0x55, 0xFF,
    0x00, 0x00, 0x00, 0xFF, 0x00,
    0x10, 0x10, 0x10, 0xFF, 0x00,
    0x14, 0x14, 0x14, 0xFF, 0x00,
    0x10, 0x10, 0xFF, 0x00, 0xFF,
    0x10, 0x10, 0xF0, 0x10, 0xF0,
    0x14, 0x14, 0x14, 0xFC, 0x00,
    0x14, 0x14, 0xF7, 0x00, 0xFF,
    0x00, 0x00, 0xFF, 0x00, 0xFF,
    0x14, 0x14, 0xF4, 0x04, 0xFC,
    0x14, 0x14, 0x17, 0x10, 0x1F,
    0x10, 0x10, 0x1F, 0x10, 0x1F,
    0x14, 0x14, 0x14, 0x1F, 0x00,
    0x10, 0x10, 0x10, 0xF0, 0x00,
    0x00, 0x00, 0x00, 0x1F, 0x10,
    0x10, 0x10, 0x10, 0x1F, 0x10,
    0x10, 0x10, 0x10, 0xF0, 0x10,
    0x00, 0x00, 0x00, 0xFF, 0x10,
    0x10, 0x10, 0x10, 0x10, 0x10,
    0x10, 0x10, 0x10, 0xFF, 0x10,
    0x00, 0x00, 0x00, 0xFF, 0x14,
    0x00, 0x00, 0xFF, 0x00, 0xFF,
    0x00, 0x00, 0x1F, 0x10, 0x17,
    0x00, 0x00, 0xFC, 0x04, 0xF4,
    0x14, 0x14, 0x17, 0x10, 0x17,
    0x14, 0x14, 0xF4, 0x04, 0xF4,
    0x00, 0x00, 0xFF, 0x00, 0xF7,
    0x14, 0x14, 0x14, 0x14, 0x14,
    0x14, 0x14, 0xF7, 0x00, 0xF7,
    0x14, 0x14, 0x14, 0x17, 0x14,
    0x10, 0x10, 0x1F, 0x10, 0x1F,
    0x14, 0x14, 0x14, 0xF4, 0x14,
    0x10, 0x10, 0xF0, 0x10, 0xF0,
    0x00, 0x00, 0x1F, 0x10, 0x1F,
    0x00, 0x00, 0x00, 0x1F, 0x14,
    0x00, 0x00, 0x00, 0xFC, 0x14,
    0x00, 0x00, 0xF0, 0x10, 0xF0,
    0x10, 0x10, 0xFF, 0x10, 0xFF,
    0x14, 0x14, 0x14, 0xFF, 0x14,
    0x10, 0x10, 0x10, 0x1F, 0x00,
    0x00, 0x00, 0x00, 0xF0, 0x10,
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
    0xF0, 0xF0, 0xF0, 0xF0, 0xF0,
    0xFF, 0xFF, 0xFF, 0x00, 0x00,
    0x00, 0x00, 0x00, 0xFF, 0xFF,
    0x0F, 0x0F, 0x0F, 0x0F, 0x0F,
    0x38, 0x44, 0x44, 0x38, 0x44,
    0xFC, 0x4A, 0x4A, 0x4A, 0x34,
    0x7E, 0x02, 0x02, 0x06, 0x06,
    0x02, 0x7E, 0x02, 0x7E, 0x02,
    0x63, 0x55, 0x49, 0x41, 0x63,
    0x38, 0x44, 0x44, 0x3C, 0x04,
    0x40, 0x7E, 0x20, 0x1E, 0x20,
    0x06, 0x02, 0x7E, 0x02, 0x02,
    0x99, 0xA5, 0xE7, 0xA5, 0x99,
    0x1C, 0x2A, 0x49, 0x2A, 0x1C,
    0x4C, 0x72, 0x01, 0x72, 0x4C,
    0x30, 0x4A, 0x4D, 0x4D, 0x30,
    0x30, 0x48, 0x78, 0x48, 0x30,
    0xBC, 0x62, 0x5A, 0x46, 0x3D,
    0x3E, 0x49, 0x49, 0x49, 0x00,
    0x7E, 0x01, 0x01, 0x01, 0x7E,
    0x2A, 0x2A, 0x2A, 0x2A, 0x2A,
    0x44, 0x44, 0x5F, 0x44, 0x44,
    0x40, 0x51, 0x4A, 0x44, 0x40,
    0x40, 0x44, 0x4A, 0x51, 0x40,
    0x00, 0x00, 0xFF, 0x01, 0x03,
    0xE0, 0x80, 0xFF, 0x00, 0x00,
    0x08, 0x08, 0x6B, 0x6B, 0x08,
    0x36, 0x12, 0x36, 0x24, 0x36,
    0x06, 0x0F, 0x09, 0x0F, 0x06,
    0x00, 0x00, 0x18, 0x18, 0x00,
    0x00, 0x00, 0x10, 0x10, 0x00,
    0x30, 0x40, 0xFF, 0x01, 0x01,
    0x00, 0x1F, 0x01, 0x01, 0x1E,
    0x00, 0x19, 0x1D, 0x17, 0x12,
    0x00, 0x3C, 0x3C, 0x3C, 0x3C,
    0x00, 0x00, 0x00, 0x00, 0x00
};

/* ------------------------------------------------------------------------- */
/* Pin helpers                                                               */
/* ------------------------------------------------------------------------- */
#define CS_LOW()    XMC_GPIO_SetOutputLow (s_cfg.cs_port, s_cfg.cs_pin)
#define CS_HIGH()   XMC_GPIO_SetOutputHigh(s_cfg.cs_port, s_cfg.cs_pin)
#define DC_CMD()    XMC_GPIO_SetOutputLow (s_cfg.dc_port, s_cfg.dc_pin)
#define DC_DATA()   XMC_GPIO_SetOutputHigh(s_cfg.dc_port, s_cfg.dc_pin)

/* ------------------------------------------------------------------------- */
/* SPI                                                                       */
/* ------------------------------------------------------------------------- */

/*
 * Blocking transmit. Each byte is complete only when its receive flag is set:
 * RX finishes on the last clock edge, whereas the transmit-shift flag fires as
 * soon as shifting *starts*. Waiting on RX makes it safe to change CS or D/C
 * immediately after this returns. MISO does not need to be connected.
 */
static void spi_write(const uint8_t *data, size_t len)
{
    const uint32_t rx_flags = (uint32_t)XMC_SPI_CH_STATUS_FLAG_RECEIVE_INDICATION |
                              (uint32_t)XMC_SPI_CH_STATUS_FLAG_ALTERNATIVE_RECEIVE_INDICATION;

    for (size_t i = 0; i < len; i++)
    {
        XMC_SPI_CH_ClearStatusFlag(s_cfg.spi, rx_flags);
        XMC_SPI_CH_Transmit(s_cfg.spi, data[i], XMC_SPI_CH_MODE_STANDARD);
        while ((XMC_SPI_CH_GetStatusFlag(s_cfg.spi) & rx_flags) == 0U) { }
        (void)XMC_SPI_CH_GetReceivedData(s_cfg.spi);
    }
}

/* The following helpers expect CS to already be low. */
static void write_cmd(uint8_t cmd)
{
    DC_CMD();
    spi_write(&cmd, 1);
}

static void write_data(const uint8_t *data, size_t len)
{
    DC_DATA();
    spi_write(data, len);
}

/* Sets the drawing window and issues RAMWR; leaves D/C in data mode. */
static void set_window(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1)
{
    uint8_t buf[4];

    write_cmd(ILI9488_CASET);
    buf[0] = (uint8_t)(x0 >> 8); buf[1] = (uint8_t)x0;
    buf[2] = (uint8_t)(x1 >> 8); buf[3] = (uint8_t)x1;
    write_data(buf, 4);

    write_cmd(ILI9488_PASET);
    buf[0] = (uint8_t)(y0 >> 8); buf[1] = (uint8_t)y0;
    buf[2] = (uint8_t)(y1 >> 8); buf[3] = (uint8_t)y1;
    write_data(buf, 4);

    write_cmd(ILI9488_RAMWR);
    DC_DATA();
}

/* One complete command transaction (asserts and releases CS). */
static void cmd_with_data(uint8_t cmd, const uint8_t *data, size_t len)
{
    CS_LOW();
    write_cmd(cmd);
    if (len > 0U)
    {
        write_data(data, len);
    }
    CS_HIGH();
}

/* RGB565 -> the 3 bytes the panel expects in 18-bit mode (upper 6 bits used). */
static void to_rgb666(uint16_t c, uint8_t out[3])
{
    uint8_t r = (uint8_t)((c >> 11) & 0x1FU);
    uint8_t g = (uint8_t)((c >> 5)  & 0x3FU);
    uint8_t b = (uint8_t)( c        & 0x1FU);
    out[0] = (uint8_t)((r << 3) | (r >> 2));
    out[1] = (uint8_t)((g << 2) | (g >> 4));
    out[2] = (uint8_t)((b << 3) | (b >> 2));
}

/* ------------------------------------------------------------------------- */
/* Initialisation                                                            */
/* ------------------------------------------------------------------------- */

/* Format: command, number of data bytes, data... */
static const uint8_t s_init_seq[] =
{
    ILI9488_GMCTRP1, 15, 0x00, 0x03, 0x09, 0x08, 0x16, 0x0A, 0x3F, 0x78,
                         0x4C, 0x09, 0x0A, 0x08, 0x16, 0x1A, 0x0F,
    ILI9488_GMCTRN1, 15, 0x00, 0x16, 0x19, 0x03, 0x0F, 0x05, 0x32, 0x45,
                         0x46, 0x04, 0x0E, 0x0D, 0x35, 0x37, 0x0F,
    ILI9488_PWCTR1,   2, 0x17, 0x15,          /* VREG1OUT, VREG2OUT          */
    ILI9488_PWCTR2,   1, 0x41,                /* VGH, VGL                    */
    ILI9488_VMCTR1,   3, 0x00, 0x12, 0x80,    /* VCOM                        */
    ILI9488_PIXFMT,   1, 0x66,                /* 18 bits/pixel (SPI)         */
    ILI9488_IFMODE,   1, 0x80,                /* SDO not used                */
    ILI9488_FRMCTR1,  1, 0xA0,                /* 60 Hz                       */
    ILI9488_INVCTR,   1, 0x02,                /* 2-dot inversion             */
    ILI9488_DFUNCTR,  2, 0x02, 0x02,          /* MCU interface, scan dir     */
    ILI9488_SETIMAGE, 1, 0x00,                /* 24-bit data bus off         */
    ILI9488_ADJCTL3,  4, 0xA9, 0x51, 0x2C, 0x82,
};

void ILI9488_Init(const ILI9488_Config_t *cfg)
{
    if ((cfg == NULL) || (cfg->spi == NULL) || (cfg->cs_port == NULL) || (cfg->dc_port == NULL))
    {
        return;
    }
    s_cfg = *cfg;

    /* Drive CS high before switching the pin to output so the panel never sees a glitch.
     * This also overrides any hardware SELOx function the configurator assigned. */
    XMC_GPIO_SetOutputHigh(s_cfg.cs_port, s_cfg.cs_pin);
    XMC_GPIO_SetMode(s_cfg.cs_port, s_cfg.cs_pin, XMC_GPIO_MODE_OUTPUT_PUSH_PULL);
    XMC_GPIO_SetMode(s_cfg.dc_port, s_cfg.dc_pin, XMC_GPIO_MODE_OUTPUT_PUSH_PULL);
    if (s_cfg.rst_port != NULL)
    {
        XMC_GPIO_SetOutputHigh(s_cfg.rst_port, s_cfg.rst_pin);
        XMC_GPIO_SetMode(s_cfg.rst_port, s_cfg.rst_pin, XMC_GPIO_MODE_OUTPUT_PUSH_PULL);
    }

    /* One byte per frame: every byte is a complete SPI word. */
    XMC_SPI_CH_SetWordLength(s_cfg.spi, 8U);
    XMC_SPI_CH_SetFrameLength(s_cfg.spi, 8U);

    s_ready = true;

    if (s_cfg.rst_port != NULL)
    {
        XMC_GPIO_SetOutputLow(s_cfg.rst_port, s_cfg.rst_pin);
        ILI9488_DELAY_MS(10);
        XMC_GPIO_SetOutputHigh(s_cfg.rst_port, s_cfg.rst_pin);
    }
    else
    {
        cmd_with_data(ILI9488_SWRESET, NULL, 0);
    }
    ILI9488_DELAY_MS(120);

    for (size_t i = 0; i < sizeof(s_init_seq); )
    {
        uint8_t cmd = s_init_seq[i++];
        uint8_t n   = s_init_seq[i++];
        cmd_with_data(cmd, &s_init_seq[i], n);
        i += n;
    }

    ILI9488_SetRotation(0);

    cmd_with_data(ILI9488_SLPOUT, NULL, 0);
    ILI9488_DELAY_MS(120);
    cmd_with_data(ILI9488_DISPON, NULL, 0);
}

/* ------------------------------------------------------------------------- */
/* Configuration                                                             */
/* ------------------------------------------------------------------------- */
void ILI9488_SetRotation(uint8_t rotation)
{
    uint8_t madctl;

    switch (rotation & 3U)
    {
        case 0:
            madctl   = ILI9488_MADCTL_MX;
            s_width  = ILI9488_NATIVE_WIDTH;
            s_height = ILI9488_NATIVE_HEIGHT;
            break;
        case 1:
            madctl   = ILI9488_MADCTL_MV;
            s_width  = ILI9488_NATIVE_HEIGHT;
            s_height = ILI9488_NATIVE_WIDTH;
            break;
        case 2:
            madctl   = ILI9488_MADCTL_MY;
            s_width  = ILI9488_NATIVE_WIDTH;
            s_height = ILI9488_NATIVE_HEIGHT;
            break;
        default:
            madctl   = ILI9488_MADCTL_MX | ILI9488_MADCTL_MY | ILI9488_MADCTL_MV;
            s_width  = ILI9488_NATIVE_HEIGHT;
            s_height = ILI9488_NATIVE_WIDTH;
            break;
    }
    madctl |= (uint8_t)ILI9488_MADCTL_COLOR_ORDER;

    if (s_ready)
    {
        cmd_with_data(ILI9488_MADCTL, &madctl, 1);
    }
}

uint16_t ILI9488_GetWidth(void)  { return s_width;  }
uint16_t ILI9488_GetHeight(void) { return s_height; }

void ILI9488_InvertDisplay(bool invert)
{
    cmd_with_data(invert ? ILI9488_INVON : ILI9488_INVOFF, NULL, 0);
}

void ILI9488_DisplayOn(bool on)
{
    cmd_with_data(on ? ILI9488_DISPON : ILI9488_DISPOFF, NULL, 0);
}

/* ------------------------------------------------------------------------- */
/* Drawing                                                                   */
/* ------------------------------------------------------------------------- */
void ILI9488_FillRect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color)
{
    if ((w <= 0) || (h <= 0))
    {
        return;
    }

    int32_t x0 = x, y0 = y;
    int32_t x1 = (int32_t)x + w - 1;
    int32_t y1 = (int32_t)y + h - 1;

    if ((x1 < 0) || (y1 < 0) || (x0 >= s_width) || (y0 >= s_height))
    {
        return;
    }
    if (x0 < 0)         { x0 = 0; }
    if (y0 < 0)         { y0 = 0; }
    if (x1 >= s_width)  { x1 = s_width  - 1; }
    if (y1 >= s_height) { y1 = s_height - 1; }

    uint32_t remaining = (uint32_t)(x1 - x0 + 1) * (uint32_t)(y1 - y0 + 1);
    uint32_t chunk     = (remaining < ILI9488_CHUNK_PIXELS) ? remaining : ILI9488_CHUNK_PIXELS;
    uint8_t  buf[ILI9488_CHUNK_PIXELS * 3];
    uint8_t  rgb[3];

    to_rgb666(color, rgb);
    for (uint32_t i = 0; i < chunk; i++)
    {
        buf[i * 3]     = rgb[0];
        buf[i * 3 + 1] = rgb[1];
        buf[i * 3 + 2] = rgb[2];
    }

    CS_LOW();
    set_window((uint16_t)x0, (uint16_t)y0, (uint16_t)x1, (uint16_t)y1);
    while (remaining > 0U)
    {
        uint32_t n = (remaining < chunk) ? remaining : chunk;
        spi_write(buf, n * 3U);
        remaining -= n;
    }
    CS_HIGH();
}

void ILI9488_FillScreen(uint16_t color)
{
    ILI9488_FillRect(0, 0, (int16_t)s_width, (int16_t)s_height, color);
}

void ILI9488_DrawPixel(int16_t x, int16_t y, uint16_t color)
{
    if ((x < 0) || (y < 0) || (x >= s_width) || (y >= s_height))
    {
        return;
    }

    uint8_t rgb[3];
    to_rgb666(color, rgb);

    CS_LOW();
    set_window((uint16_t)x, (uint16_t)y, (uint16_t)x, (uint16_t)y);
    spi_write(rgb, 3);
    CS_HIGH();
}

void ILI9488_DrawHLine(int16_t x, int16_t y, int16_t w, uint16_t color)
{
    ILI9488_FillRect(x, y, w, 1, color);
}

void ILI9488_DrawVLine(int16_t x, int16_t y, int16_t h, uint16_t color)
{
    ILI9488_FillRect(x, y, 1, h, color);
}

void ILI9488_DrawRect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color)
{
    if ((w <= 0) || (h <= 0))
    {
        return;
    }
    ILI9488_DrawHLine(x, y, w, color);
    ILI9488_DrawHLine(x, (int16_t)(y + h - 1), w, color);
    ILI9488_DrawVLine(x, y, h, color);
    ILI9488_DrawVLine((int16_t)(x + w - 1), y, h, color);
}

void ILI9488_DrawLine(int16_t x0, int16_t y0, int16_t x1, int16_t y1, uint16_t color)
{
    if (x0 == x1)
    {
        int16_t top = (y0 < y1) ? y0 : y1;
        ILI9488_DrawVLine(x0, top, (int16_t)((y0 < y1 ? y1 - y0 : y0 - y1) + 1), color);
        return;
    }
    if (y0 == y1)
    {
        int16_t left = (x0 < x1) ? x0 : x1;
        ILI9488_DrawHLine(left, y0, (int16_t)((x0 < x1 ? x1 - x0 : x0 - x1) + 1), color);
        return;
    }

    /* Bresenham */
    int32_t dx  = (x1 > x0) ? (x1 - x0) : (x0 - x1);
    int32_t dy  = (y1 > y0) ? (y0 - y1) : (y1 - y0);   /* negative */
    int32_t sx  = (x0 < x1) ? 1 : -1;
    int32_t sy  = (y0 < y1) ? 1 : -1;
    int32_t err = dx + dy;
    int32_t cx  = x0, cy = y0;

    for (;;)
    {
        ILI9488_DrawPixel((int16_t)cx, (int16_t)cy, color);
        if ((cx == x1) && (cy == y1))
        {
            break;
        }
        int32_t e2 = 2 * err;
        if (e2 >= dy) { err += dy; cx += sx; }
        if (e2 <= dx) { err += dx; cy += sy; }
    }
}

void ILI9488_DrawImage(const uint8_t *img, int16_t x, int16_t y, int16_t w, int16_t h)
{
    if ((img == NULL) || (w <= 0) || (h <= 0))
    {
        return;
    }

    int32_t x0 = x, y0 = y;
    int32_t x1 = (int32_t)x + w - 1;
    int32_t y1 = (int32_t)y + h - 1;
    int32_t src_x = 0, src_y = 0;

    if ((x1 < 0) || (y1 < 0) || (x0 >= s_width) || (y0 >= s_height))
    {
        return;
    }
    if (x0 < 0)         { src_x = -x0; x0 = 0; }
    if (y0 < 0)         { src_y = -y0; y0 = 0; }
    if (x1 >= s_width)  { x1 = s_width  - 1; }
    if (y1 >= s_height) { y1 = s_height - 1; }

    int32_t cols = x1 - x0 + 1;
    int32_t rows = y1 - y0 + 1;
    uint8_t buf[ILI9488_CHUNK_PIXELS * 3];

    CS_LOW();
    set_window((uint16_t)x0, (uint16_t)y0, (uint16_t)x1, (uint16_t)y1);
    for (int32_t r = 0; r < rows; r++)
    {
        const uint8_t *src = img + ((((uint32_t)(src_y + r) * (uint32_t)w) + (uint32_t)src_x) * 2U);
        int32_t left = cols;
        while (left > 0)
        {
            int32_t n = (left < ILI9488_CHUNK_PIXELS) ? left : ILI9488_CHUNK_PIXELS;
            for (int32_t i = 0; i < n; i++)
            {
                uint16_t c = (uint16_t)(((uint16_t)src[0] << 8) | src[1]);
                to_rgb666(c, &buf[i * 3]);
                src += 2;
            }
            spi_write(buf, (size_t)n * 3U);
            left -= n;
        }
    }
    CS_HIGH();
}

/* ------------------------------------------------------------------------- */
/* Text                                                                      */
/* ------------------------------------------------------------------------- */
void ILI9488_SetCP437(bool enable)
{
    s_cp437 = enable;
}

void ILI9488_DrawChar(int16_t x, int16_t y, unsigned char c, uint16_t color, uint16_t bg, uint8_t size)
{
    if (size == 0U)
    {
        size = 1U;
    }

    int32_t cw = 6 * (int32_t)size;
    int32_t ch = 8 * (int32_t)size;

    /* Skip only if no part of the cell is on screen; partial cells are clipped per pixel. */
    if ((x >= s_width) || (y >= s_height) || ((x + cw - 1) < 0) || ((y + ch - 1) < 0))
    {
        return;
    }

    if (!s_cp437 && (c >= 176U) && (c < 255U))
    {
        c++;   /* classic Adafruit glyph table offset */
    }

    const uint8_t *glyph  = &s_font[(uint32_t)c * 5U];
    bool           opaque = (bg != color);
    bool           inside = (x >= 0) && (y >= 0) && ((x + cw) <= s_width) && ((y + ch) <= s_height);

    if (opaque && inside && (size <= ILI9488_TEXT_FAST_MAX_SIZE))
    {
        /* Fast path: stream the whole cell through one address window. */
        uint8_t fg3[3], bg3[3];
        uint8_t line[6 * ILI9488_TEXT_FAST_MAX_SIZE * 3];

        to_rgb666(color, fg3);
        to_rgb666(bg, bg3);

        CS_LOW();
        set_window((uint16_t)x, (uint16_t)y, (uint16_t)(x + cw - 1), (uint16_t)(y + ch - 1));
        for (uint8_t row = 0; row < 8U; row++)
        {
            uint8_t *p = line;
            for (uint8_t col = 0; col < 6U; col++)
            {
                uint8_t bits = (col < 5U) ? glyph[col] : 0U;
                const uint8_t *px = ((bits >> row) & 1U) ? fg3 : bg3;
                for (uint8_t s = 0; s < size; s++)
                {
                    *p++ = px[0];
                    *p++ = px[1];
                    *p++ = px[2];
                }
            }
            for (uint8_t s = 0; s < size; s++)
            {
                spi_write(line, (size_t)cw * 3U);
            }
        }
        CS_HIGH();
        return;
    }

    /* General path: transparent text, clipped cells, or very large sizes. */
    for (uint8_t col = 0; col < 6U; col++)
    {
        uint8_t bits = (col < 5U) ? glyph[col] : 0U;
        for (uint8_t row = 0; row < 8U; row++, bits >>= 1)
        {
            uint16_t px;
            if (bits & 1U)    { px = color; }
            else if (opaque)  { px = bg; }
            else              { continue; }

            int16_t px_x = (int16_t)(x + col * size);
            int16_t px_y = (int16_t)(y + row * size);
            if (size == 1U)
            {
                ILI9488_DrawPixel(px_x, px_y, px);
            }
            else
            {
                ILI9488_FillRect(px_x, px_y, (int16_t)size, (int16_t)size, px);
            }
        }
    }
}

void ILI9488_PrintText(const char *text, int16_t x, int16_t y, uint16_t color, uint16_t bg, uint8_t size)
{
    if (text == NULL)
    {
        return;
    }
    if (size == 0U)
    {
        size = 1U;
    }

    int16_t cx = x;
    for (; *text != '\0'; text++)
    {
        if (*text == '\n')
        {
            cx = x;
            y  = (int16_t)(y + 8 * size);
            continue;
        }
        if (*text == '\r')
        {
            continue;
        }
        ILI9488_DrawChar(cx, y, (unsigned char)*text, color, bg, size);
        cx = (int16_t)(cx + 6 * size);
    }
}

/* ------------------------------------------------------------------------- */
/* Utilities                                                                 */
/* ------------------------------------------------------------------------- */
uint16_t ILI9488_Color565(uint8_t r, uint8_t g, uint8_t b)
{
    return (uint16_t)(((uint16_t)(r & 0xF8U) << 8) | ((uint16_t)(g & 0xFCU) << 3) | (b >> 3));
}

void ILI9488_SetScrollArea(uint16_t top_fixed, uint16_t bottom_fixed)
{
    if ((uint32_t)top_fixed + bottom_fixed > ILI9488_NATIVE_HEIGHT)
    {
        return;
    }
    uint16_t scroll_h = (uint16_t)(ILI9488_NATIVE_HEIGHT - top_fixed - bottom_fixed);
    uint8_t  buf[6] =
    {
        (uint8_t)(top_fixed >> 8),    (uint8_t)top_fixed,
        (uint8_t)(scroll_h >> 8),     (uint8_t)scroll_h,
        (uint8_t)(bottom_fixed >> 8), (uint8_t)bottom_fixed
    };
    cmd_with_data(ILI9488_VSCRDEF, buf, sizeof(buf));
}

void ILI9488_Scroll(uint16_t line)
{
    uint8_t buf[2] = { (uint8_t)(line >> 8), (uint8_t)line };
    cmd_with_data(ILI9488_VSCRSADD, buf, sizeof(buf));
}

/* ------------------------------------------------------------------------- */
/* Low-level access                                                          */
/* ------------------------------------------------------------------------- */
void ILI9488_SendCommand(uint8_t cmd)
{
    cmd_with_data(cmd, NULL, 0);
}

void ILI9488_SendData(const uint8_t *data, size_t len)
{
    if ((data == NULL) || (len == 0U))
    {
        return;
    }
    CS_LOW();
    write_data(data, len);
    CS_HIGH();
}

void ILI9488_SetAddrWindow(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1)
{
    CS_LOW();
    set_window(x0, y0, x1, y1);
    CS_HIGH();
}
