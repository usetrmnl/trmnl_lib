#include <PNGdec.h>
#include <trmnl_lib.h>
#define USE_EPAPER
#ifdef USE_EPAPER
#include <bb_epaper.h>
#include "../Fonts/Roboto_Black_16.h"
extern TRMNL trmnl;
BBEPAPER bbep;
#else
#include <bb_spi_lcd.h>
BB_SPI_LCD lcd;
#endif // !BB_EPAPER
int iImageHeight;
PNG *png;
uint16_t *pTemp;

//
// Draw callback (per scan line) from PNGdec
//
#ifdef USE_EPAPER
void ReduceBpp(int iDestBpp, int iPixelType, uint8_t *pPalette, uint8_t *pSrc, uint8_t *pDest, int w, int iSrcBpp)
{
    int g = 0, x, iDelta;
    uint8_t *s, *d, *pPal, u8, count;
    const uint8_t u8G2ToG8[4] = {0x00, 0x55, 0xaa, 0xff}; // 2-bit to 8-bit gray

    if (iPixelType == PNG_PIXEL_TRUECOLOR) iSrcBpp = 24;
    else if (iPixelType == PNG_PIXEL_TRUECOLOR_ALPHA) iSrcBpp = 32;
    iDelta = iSrcBpp/8; // bytes per pixel
    count = 8; // bits in a byte
    u8 = 0; // start with all black
    d = pDest;
    s = pSrc;
    for (x=0; x<w; x++) {
        u8 <<= iDestBpp;
        switch (iSrcBpp) {
            case 24:
            case 32:
                g = (s[0] + s[1]*2 + s[2])/4; // convert color to gray value
                s += iDelta;
                break;
            case 8:
                if (iPixelType == PNG_PIXEL_INDEXED) {
                    pPal = &pPalette[s[0] * 3];
                    g = (pPal[0] + pPal[1]*2 + pPal[2])/4;
                } else { // must be grayscale
                    g = s[0];
                }
                s++;
                break;
            case 4:
                if (x & 1) {
                    if (iPixelType == PNG_PIXEL_INDEXED) {
                        pPal = &pPalette[(s[0] & 0xf) * 3];
                        g = (pPal[0] + pPal[1]*2 + pPal[2])/4;
                    } else {
                        g = (s[0] & 0xf) | (s[0] << 4);
                    }
                    s++;
                } else {
                    if (iPixelType == PNG_PIXEL_INDEXED) {
                        pPal = &pPalette[(s[0]>>4) * 3];
                        g = (pPal[0] + pPal[1]*2 + pPal[2])/4;
                    } else {
                        g = (s[0] & 0xf0) | (s[0] >> 4);
                    }
                }
                break;
            case 2: // We need to handle this case for 2-bit images with (random) palettes
                g = s[0] >> (6-((x & 3) * 2));
                if (iPixelType == PNG_PIXEL_INDEXED) {
                    pPal = &pPalette[(g & 3)*3];
                    g = (pPal[0] + pPal[1]*2 + pPal[2])/4;
                } else {
                    g = u8G2ToG8[g & 3];
                }
                if ((x & 3) == 3) {
                    s++;
                }
                break;
        } // switch on bpp
        if (iDestBpp == 1) {
            u8 |= (g >> 7); // B/W
        } else if (iDestBpp == 2) { // generate 4 gray levels (2 bits)
            u8 |= (3 ^ (g >> 6)); // 4 gray levels (inverted relative to 1-bit)
        } else { // must be 4-bpp output
            u8 |= (g >> 4);
        }
        count -= iDestBpp;
        if (count == 0) { // byte is full, move on
            *d++ = u8;
            u8 = 0;
            count = 8;
        }
    } // for x
    if (count != 8) { // partial byte remaining
        u8 <<= count;
        *d++ = u8;
    }
} /* ReduceBpp() */
enum {
    PNG_1_BIT = 0,
    PNG_1_BIT_INVERTED,
    PNG_2_BIT_0,
    PNG_2_BIT_1,
    PNG_2_BIT_BOTH,
    PNG_2_BIT_INVERTED,
};

int png_draw(PNGDRAW *pDraw)
{
    int x;
    uint8_t ucBppChanged = 0, ucInvert = 0;
    uint8_t uc, ucMask, src, *s, *d, *pTemp = bbep.getCache(); // get some scratch memory (not from the stack)
    int iPlane = *(int *)pDraw->pUser;
    int iWidth;

    iWidth = pDraw->iWidth;
    if (pDraw->y >= bbep.height()) return 0; // stop decoding if we'll go past the bottom
    if (iWidth > bbep.width()) iWidth = bbep.width(); // crop image width to display size if it's larger

    if (pDraw->iPixelType == PNG_PIXEL_INDEXED || pDraw->iBpp > 2) {
        if (pDraw->iBpp == 1) { // 1-bit output, just see which color is brighter
            uint32_t u32Gray0, u32Gray1;
            u32Gray0 = pDraw->pPalette[0] + (pDraw->pPalette[1]<<2) + pDraw->pPalette[2];
            u32Gray1 = pDraw->pPalette[3] + (pDraw->pPalette[4]<<2) + pDraw->pPalette[5];
          if (u32Gray0 < u32Gray1) {
            ucInvert = 0xff;
          }
        } else {
            // Reduce the source image to 1-bpp or 2-bpp
            ReduceBpp((pDraw->pUser) ? 2:1, pDraw->iPixelType, pDraw->pPalette, pDraw->pPixels, pTemp, iWidth, pDraw->iBpp);
            ucBppChanged = 1;
        }
    } else if (pDraw->iBpp == 2) {
        ucInvert = 0xff; // 2-bit non-palette images need to be inverted colors for 4-gray mode
    }
    s = (ucBppChanged) ? pTemp : (uint8_t *)pDraw->pPixels;
    d = pTemp;
    if (iPlane == PNG_1_BIT || iPlane == PNG_1_BIT_INVERTED) {
        // 1-bit output, decode the single plane and write it
        if (iPlane == PNG_1_BIT_INVERTED) ucInvert = ~ucInvert; // to do PLANE_FALSE_DIFF
        if (iPlane == PNG_1_BIT_INVERTED && (bbep.capabilities() & BBEP_3COLOR)) { // write the red plane as 0's for this case
            memset(d, 0, iWidth/8);
        } else {
            for (x=0; x<iWidth; x+= 8) {
                d[0] = s[0] ^ ucInvert;
                d++; s++;
            }
        }
    } else { // we need to split the 2-bit data into plane 0 and 1
        src = *s++;
        src ^= ucInvert;
        uc = 0; // suppress warning/error
        if (iPlane == PNG_2_BIT_BOTH || iPlane == PNG_2_BIT_INVERTED) { // draw 2bpp data as 1-bit to use for partial update
            if (iPlane == PNG_2_BIT_BOTH) {
                ucInvert = ~ucInvert; // the invert rule is backwards for grayscale data
            }
            src = ~src;
            for (x=0; x<iWidth; x++) {
                uc <<= 1;
                if (src & 0xc0) { // non-white -> black
                    uc |= 1; // high bit of source pair
                }
                src <<= 2;
                if ((x & 3) == 3) { // new input byte
                    src = *s++;
                    src ^= ucInvert;
                }
                if ((x & 7) == 7) { // new output byte
                    *d++ = uc;
                }
            } // for x
        } else { // normal 0/1 split plane
            ucMask = (iPlane == PNG_2_BIT_0) ? 0x40 : 0x80; // lower or upper source bit
            for (x=0; x<iWidth; x++) {
                uc <<= 1;
                if (src & ucMask) {
                    uc |= 1; // high bit of source pair
                }
                src <<= 2;
                if ((x & 3) == 3) { // new input byte
                    src = *s++;
                    src ^= ucInvert;
                }
                if ((x & 7) == 7) { // new output byte
                    *d++ = uc;
                }
            } // for x
        }
    }
    bbep.writeData(pTemp, (iWidth+7)/8);
    if (iWidth < bbep.width()) {
        // the image is narrower than the display, fill in the right edge with white
        int w = (bbep.width() - iWidth)/8;
        if (w) {
            memset(pTemp, 0xff, w); // white
            bbep.writeData(pTemp, w);
        }
    }
    // If we're at the last line of the PNG image, but it's shorter than the display,
    // fill the remaining lines with white
    if (pDraw->y == iImageHeight-1 && iImageHeight < bbep.height()) {
        int i, w = (bbep.width() + 7)/8;
        memset(pTemp, 0xff, w);
        for (i=pDraw->y; i<bbep.height(); i++) {
            // write the remaing lines as white
            bbep.writeData(pTemp, w);
        }
    }
    return 1;
} /* png_draw() */
#else // color LCD
int png_draw(PNGDRAW *pDraw)
{
int iWidth = pDraw->iWidth;

    if (pDraw->y == 0) {
      lcd.setAddrWindow(0, 0, lcd.width(), lcd.height());
    }
    if (pDraw->y >= lcd.height()) {
        return 0; // stop decoding if we'll go past the bottom
    }
    if (iWidth > lcd.width()) iWidth = lcd.width(); // crop image width to display size if it's larger
    png->getLineAsRGB565(pDraw, pTemp, PNG_RGB565_BIG_ENDIAN, 0xffffff);
    lcd.pushPixels(pTemp, iWidth);
    return 1;
} /* png_draw() */
#endif // USE_EPAPER
//
// Display a PNG image on the given display
// crop it if it's too large
//
void displayImage(uint8_t *pImage, int iImageSize)
{
int iPlane;
  png = new PNG();
  int rc = png->openRAM(pImage, iImageSize, png_draw);
  if (rc == PNG_SUCCESS) {
    bbep.begin(EPD_XTEINK_X3);
    iImageHeight = png->getHeight();
    Serial.printf("PNG opened: %d x %d, %d-bpp\n", png->getWidth(), png->getHeight(), png->getBpp());
#ifdef USE_EPAPER
    bbep.setAddrWindow(0, 0, bbep.width(), bbep.height());
    if (png->getBpp() == 1) { // 1-bit image (single plane)
        png->close(); // use a different PNGDraw callback for color matching
        bbep.startWrite(PLANE_0); // start writing image data to plane 0
        iPlane = PNG_1_BIT;
        png->decode(&iPlane, 0);
    } else { // 2-bpp (or greater, but reduced to 2-bpp)
        bbep.setPanelType(EP368_792x528_4GRAY);
        bbep.startWrite(PLANE_0); // start writing image data to plane 0
        iPlane = PNG_2_BIT_0;
        png->decode(&iPlane, 0); // tell PNGDraw to use bits for plane 0
        png->close(); // start over for plane 1
        iPlane = PNG_2_BIT_1;
        png->openRAM((uint8_t *)pImage, iImageSize, png_draw);
        bbep.startWrite(PLANE_1); // start writing image data to plane 1
        png->decode(&iPlane, 0); // decode it again to get plane 1 data
    }
    bbep.refresh(REFRESH_FULL);
    bbep.sleep(LIGHT_SLEEP);
#else // must be color LCD
    pTemp = (uint16_t *)malloc(png->getWidth() * 2); // temporary RGB565 buffer
    lcd.begin(DISPLAY_WS_AMOLED_18); // Waveshare ESP32-S3 AMOLED 1.8" 368x448
    lcd.fillScreen(TFT_BLACK); // clear to black and sets memory window for next pass
    png->decode(NULL, 0);
    free(pTemp);
#endif
  } else {
    Serial.println("Error opening the image!");
  }
  png->close();
  free(png);
} /* displayImage() */

void displaySensorValues(void)
{
  time_t now;
  struct tm *thetime;
#ifdef USE_EPAPER
    bbep.begin(EPD_WAVESHARE_154); // pre-configured for the Waveshare ESP32-S3 1.54" e-Paper
    bbep.allocBuffer();
    bbep.fillScreen(BBEP_WHITE);
    time(&now);
    thetime = gmtime(&now);
    bbep.setTextColor(BBEP_BLACK, BBEP_WHITE);
    bbep.setFont(Roboto_Black_16);
    bbep.setCursor(0, 24); // baseline, not top of font
    bbep.print("TRMNL");
    bbep.setCursor(0,50);
    bbep.print("Sensor node");
    bbep.setFont(FONT_12x16);
    bbep.setCursor(0, 64);
    bbep.printf("Last: %02d:%02d UTC\n", thetime->tm_hour, thetime->tm_min);
    now += trmnl.getSleepTime();
    thetime = gmtime(&now);
    bbep.printf("Next: %02d:%02d UTC\n", thetime->tm_hour, thetime->tm_min);
    bbep.printf("Temp: %.1f C\n", trmnl.getTemperature());
    bbep.printf("Hum: %d %%\n", trmnl.getHumidity());
    if (trmnl.getCo2()) {
        bbep.printf("CO2: %d ppm\n", trmnl.getCo2());
    }
    bbep.writePlane();
    bbep.refresh(REFRESH_FAST);
    bbep.sleep(LIGHT_SLEEP);
#else // LCD
    lcd.begin(DISPLAY_M5STACK_ATOMS3); // pre-configured for the M5Stack AtomS3
    lcd.fillScreen(TFT_BLACK);
    time(&now);
    thetime = gmtime(&now);
    lcd.setTextColor(TFT_GREEN, TFT_BLACK)
    lcd.setCursor(0,0);
    lcd.setFont(FONT_12x16);
    lcd.println("TRMNL\nSensor node");
    lcd.setTextColor(TFT_WHITE, TFT_BLACK);
    lcd.printf("L: %02d:%02d UTC\n", thetime->tm_hour, thetime->tm_min);
    now += trmnl.getSleepTime();
    thetime = gmtime(&now);
    lcd.printf("N: %02d:%02d UTC\n", thetime->tm_hour, thetime->tm_min);
    if (trmnl.temperature()) {
        lcd.printf("Temp: %.1f C\n", trmnl.temperature());
    }
    if (trmnl.humidity()) {
        lcd.printf("Hum: %d %%\n", trmnl.humidity());
    }
    if (trmnl.co2()) {
        lcd.printf("CO2: %d ppm\n", trmnl.co2());
    }
#endif
} /* displaySensorValues() */