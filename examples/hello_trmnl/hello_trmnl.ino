//
// A minimal Arduino example to show how to interface
// your own code to the TRMNL back-end
// written by Larry Bank (bitbank@pobox.com)
// March 25, 2025
//
#include "trmnl_lib.h"
#include "esp_task_wdt.h"
void displayImage(uint8_t *pImage, int iImageSize);
void displaySensorValues(void);
TRMNL trmnl;

// Enter your WiFi credentials and TRMNL Device Key
const char *ssid = "your_ssid";
const char *pw = "your_password";
const char *szAPIKey = "your_api_key"; // N.B. Don't share this publicly

void setup()
{
int rc;
   uint8_t *pImage;
   int iImageSize;

   Serial.begin(115200);
   delay(3000);
//   esp_task_wdt_config_t wdt_config = {30, 3, false};
//   esp_task_wdt_init(&wdt_config); // disable watchdog timer

   Serial.println("Starting TRMNL demo");
   trmnl.setDisplaySize(800, 480); // dynamic display size is not supported yet; for future use
   trmnl.setSensorBus(2, 1); // M5Stack AtomS3 GROVE I2C
//   trmnl.setSensorBus(47, 48); // Waveshare ESP32-S3 ePaper 1.54 internal I2C temp/humidity sensor
   if (trmnl.connectWiFi(ssid, pw)) {
      rc = trmnl.getAPI(szAPIKey); // also reads and sends the sensor values if valid
      if (rc == TRMNL_SUCCESS) {
//          displaySensorValues(); // instead of showing the TRMNL image
         rc = trmnl.getImage(&pImage, &iImageSize);
         if (rc == TRMNL_SUCCESS) {
            displayImage(pImage, iImageSize);
            trmnl.freeImage();
         }
      }
      trmnl.disconnectWiFi();
   }
   // Special setup for the Xteink X3 to keep the battery on, but sleep with the lowest power
  pinMode(13, OUTPUT);
  digitalWrite(13, HIGH);
  gpio_hold_en(GPIO_NUM_13); // MOSFET enabling the battery power
  gpio_hold_en(GPIO_NUM_5); // hold EPD reset high in deep sleep
  gpio_deep_sleep_hold_en(); // Needed to keep the battery power enabled during RTC sleep

   trmnl.sleep();
} /* setup() */

void loop()
{
} /* loop() */
