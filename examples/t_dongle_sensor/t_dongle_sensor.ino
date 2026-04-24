//
// Example sketch for the TRMNL library
// written by Larry Bank
// April 23, 2026
//
// This sketch shows how to use the LilyGo T-Dongle C5 as a TRMNL sensor node
// What this means is that an I2C sensor attached to the T-Dongle will push
// Sensor readings to your TRMNL account for use in your plugins. The values
// will also be displayed on the color LCD of the T-Dongle
//
#include <trmnl_lib.h>
#include <bb_spi_lcd.h>
BB_SPI_LCD lcd;
TRMNL trmnl;
void displaySensorValues(void);

// Enter your WiFi credentials and TRMNL Device Key
const char *ssid = "your_ssid";
const char *pw = "your_password";
const char *szAPIKey = "your_api_key"; // N.B. Don't share this publicly

void setup()
{
   Serial.begin(115200);
   delay(3000);
   Serial.println("Starting TRMNL demo");

   lcd.begin(DISPLAY_T_DONGLE_C5); // pre-configured for the LilyGo T-Dongle C5
   lcd.fillScreen(TFT_BLACK);
   lcd.setTextColor(TFT_GREEN, TFT_BLACK);
   lcd.setCursor(0,0);
   lcd.setFont(FONT_12x16);
   lcd.println("TRMNL Sensor");

   trmnl.setSensorBus(11, 12); // T-Dongle C5 QWIIC I2C Bus
}

void loop()
{
  int rc;

   if (trmnl.connectWiFi(ssid, pw)) {
      rc = trmnl.getAPI(szAPIKey); // also reads and sends the sensor values if valid
      if (rc == TRMNL_SUCCESS) {
          displaySensorValues(); // instead of showing the TRMNL image
      }
      trmnl.disconnectWiFi();
   }
   delay(15 * 60 * 1000); // push new values every 15 minutes
}

