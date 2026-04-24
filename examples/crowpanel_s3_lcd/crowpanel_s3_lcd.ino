//
// TRMNL demo for the Crowpanel Advance ESP32-P4 5" 800x480
//
#include <trmnl_lib.h>
#include <bb_spi_lcd.h>
//#define PNG_MAX_BUFFERED_PIXELS (801 * 4 * 2)
#include <PNGdec.h>
PNG png;
BB_SPI_LCD lcd;
TRMNL trmnl;

// Enter your WiFi credentials and TRMNL Device Key
const char *ssid = "your_ssid";
const char *pw = "your_password";
const char *szAPIKey = "your_api_key"; // N.B. Don't share this publicly
uint16_t *pTemp;

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
    png.getLineAsRGB565(pDraw, pTemp, PNG_RGB565_BIG_ENDIAN, 0xffffff);
    lcd.pushPixels(pTemp, iWidth);
    return 1;
} /* png_draw() */

void displayImage(uint8_t *pImage, int iImageSize)
{
  int rc = png.openRAM(pImage, iImageSize, png_draw);
  if (rc == PNG_SUCCESS) {
    Serial.printf("PNG opened: %d x %d, %d-bpp\n", png.getWidth(), png.getHeight(), png.getBpp());
    pTemp = (uint16_t *)malloc(png.getWidth() * 2); // temporary RGB565 buffer
    png.decode(NULL, 0);
    free(pTemp);
  }
  png.close();
} /* displayImage() */

void setup() {
int rc, iSize;
uint8_t *pImg;

   Serial.begin(115200);
   delay(3000);
   Serial.println("Starting TRMNL demo");
   lcd.begin(DISPLAY_ELECROW_S3_800x480);
   lcd.fillScreen(TFT_BLACK); // clear to black and sets memory window for next pass

   trmnl.setDisplaySize(800, 480); // dynamic display size is not supported yet; for future use
   if (trmnl.connectWiFi(ssid, pw)) {
      rc = trmnl.getAPI(szAPIKey); // also reads and sends the sensor values if valid
      if (rc == TRMNL_SUCCESS) {
         rc = trmnl.getImage(&pImg, &iSize);
         if (rc == TRMNL_SUCCESS) {
            displayImage(pImg, iSize);
            trmnl.freeImage();
         }
      }
      trmnl.disconnectWiFi();
   } // connected
}

void loop() {
}
