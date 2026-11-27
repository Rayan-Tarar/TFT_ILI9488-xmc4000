/*
 * Basic example: ILI9488 on XMC4700 (ModusToolbox)
 *
 * Wiring used here (change to match your board):
 *   SCLK  -> P0.10  (USIC1_CH1 SCLKOUT, configured in the Device Configurator)
 *   MOSI  -> P0.1   (USIC1_CH1 DOUT0,   configured in the Device Configurator)
 *   CS    -> P0.2   (GPIO, driven by the driver)
 *   DC/RS -> P0.4   (GPIO, driven by the driver)
 *   RESET -> P0.3   (GPIO, driven by the driver)
 *   LED   -> 3.3 V  (backlight)
 *   VCC   -> 3.3 V, GND -> GND
 */

#include "cybsp.h"
#include "cy_utils.h"
#include "ili9488.h"

static const ILI9488_Config_t lcd_cfg =
{
    .spi      = XMC_SPI1_CH1,
    .cs_port  = XMC_GPIO_PORT0, .cs_pin  = 2,
    .dc_port  = XMC_GPIO_PORT0, .dc_pin  = 4,
    .rst_port = XMC_GPIO_PORT0, .rst_pin = 3,
};

int main(void)
{
    /* Clocks, pins and the USIC SPI channel come from design.modus */
    if (cybsp_init() != CY_RSLT_SUCCESS)
    {
        CY_ASSERT(0);
    }

    ILI9488_Init(&lcd_cfg);
    ILI9488_SetRotation(1);                     /* landscape, 480 x 320 */

    ILI9488_FillScreen(ILI9488_WHITE);

    ILI9488_FillRect(40, 30, 400, 80, ILI9488_BLACK);
    ILI9488_PrintText("Hello XMC4700", 87, 54, ILI9488_WHITE, ILI9488_BLACK, 4);

    ILI9488_DrawRect(150, 200, 180, 70, ILI9488_BLUE);
    ILI9488_PrintText("ILI9488", 198, 227, ILI9488_BLUE, ILI9488_WHITE, 2);

    ILI9488_DrawLine(0, ILI9488_GetHeight() - 1, ILI9488_GetWidth() - 1, 0, ILI9488_RED);

    for (;;)
    {
    }
}
