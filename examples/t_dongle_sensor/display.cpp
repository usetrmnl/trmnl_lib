#include <trmnl_lib.h>
extern TRMNL trmnl;
#include <bb_spi_lcd.h>
extern BB_SPI_LCD lcd;

void displaySensorValues(void)
{
  time_t now;
  struct tm *thetime;
    time(&now);
    thetime = gmtime(&now);
    lcd.setCursor(0,16);
    lcd.setFont(FONT_12x16);
    lcd.setTextColor(TFT_WHITE, TFT_BLACK);
    lcd.printf("%02d:%02d UTC\n", thetime->tm_hour, thetime->tm_min);
//    now += trmnl.getSleepTime();
//    thetime = gmtime(&now);
//    lcd.printf("N: %02d:%02d UTC\n", thetime->tm_hour, thetime->tm_min);
    lcd.setTextColor(TFT_MAGENTA, TFT_BLACK);
    if (trmnl.getCo2()) {
        lcd.printf("CO2: %d ppm\n", trmnl.getCo2());
    }
    if (trmnl.getTemperature()) {
        lcd.printf("Temp: %.1f C\n", trmnl.getTemperature());
    }
    if (trmnl.getHumidity()) {
        lcd.printf("Hum: %d %%\n", trmnl.getHumidity());
    }
} /* displaySensorValues() */