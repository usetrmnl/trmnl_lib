//
// TRMNL library
// Written by Larry Bank
// project started March 25, 2026
//
// SPDX-FileCopyrightText: 2026 TRMNL LLC
// SPDX-License-Identifier: Apache-2.0
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//    http://www.apache.org/licenses/LICENSE-2.0
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//===========================================================================
//
#include "trmnl_lib.h"
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <Preferences.h>
#include "esp_sntp.h"
#include <bb_scd41.h>
#include <bb_temperature.h>
SCD41 scd41;
BBTemp bbt;
static HTTPClient https;
// 10 seconds WIFI connection timeout
#define TIMEOUT 20
const char *szDevices[] = {"None", "AHT20", "BMP180", "BME280", "BMP388", "SHT3X", "HDC1080", "HTS221", "MCP9808","BME68x","SHTC3"};
const char *szMakers[] = {"None", "ASAIR", "Bosch", "Bosch", "Bosch", "Sensirion", "TI", "STMicro","MicroChip","Bosch","Sensirion"};
//
// Initialize the I2C bus and find+start any sensors that are found
//
void TRMNL::setSensorBus(uint8_t sda, uint8_t scl)
{
    _bCO2 = false;
    _bTimeSync = true; // time stamp needs to have the correct time
    _iSensorType = -1; // assume no supported sensors
    
    // check if there is a SCD41 or supported temperature sensor attached
    if (bbt.init(sda, scl) == BBT_SUCCESS) {
      _iSensorType = bbt.type();
      Serial.printf("%s [%d]: supported sensor found! (%d)\r\n", __FILE__, __LINE__, _iSensorType);
      bbt.start(); // start the sensor
    }
    if (scd41.init(sda, scl) == SCD41_SUCCESS) {
      _bCO2 = true;
      Serial.printf("%s [%d]: SCD41 sensor found!\r\n", __FILE__, __LINE__);
        scd41.wakeup();
      // The SCD41 needs to be re-initialized after big Vcc variations from the last wakeup
      // put it in a 'confused' state. If we don't re-initialize it, it won't generate more samples
      scd41.sendCMD(SCD41_CMD_REINIT);
      vTaskDelay(3); // allow time to reinitialize
      scd41.triggerSample(); // trigger a 'one-shot' sample that takes about 5 seconds to complete
    }
    if (!_bCO2 && _iSensorType < 0) {
      Serial.printf("%s [%d]: No sensor found on I2C bus %d/%d\r\n", __FILE__, __LINE__, sda, scl);
    }
    _lSensorTime = millis(); // mark the time when we triggered sensor samples
} /* setSensorBus() */

bool TRMNL::connectWiFi(const char *ssid, const char *pw)
{
    int iTimeout = 0;
    
    Serial.print("Connecting to wifi");
    
    WiFi.begin(ssid, pw);
    while (WiFi.status() != WL_CONNECTED && WiFi.status() != WL_CONNECT_FAILED && iTimeout < TIMEOUT) {
      delay(500); // allow up to 10 seconds to connect
      iTimeout++;
      Serial.print(".");
    }
    if (iTimeout == TIMEOUT || WiFi.status() != WL_CONNECTED) {
      Serial.println("\nConnection timed out!");
    } else {
      Serial.println("\nConnected!");
      return true;
    }
    return false; // failed

} /* connectWiFi() */

bool TRMNL::setClock()
{
  bool sync_status = false;
  struct tm timeinfo;
  time_t now;
  int iDeltaTime;
  Preferences prefs;

  prefs.begin("data");
  uint32_t u32Epoch = prefs.getUInt("last_sync", 0); // Get the last time sync time
  time(&now);
  iDeltaTime = (uint32_t)now - u32Epoch; // Number of seconds since the last sync
  Serial.printf("%s [%d]: epoch time: %lu iDelta: %d\r\n", __FILE__, __LINE__, (uint32_t)now, iDeltaTime);
  if (u32Epoch != 0 && iDeltaTime > 0 && iDeltaTime < 24*60*60) { // Less than 24h, no need to sync the time
      Serial.printf("%s [%d]: Skipping time sync\r\n", __FILE__, __LINE__);
      prefs.end();
      return true;
  }
  String ntp = prefs.getString("ntp_server", "time.google.com");

  Serial.printf("%s [%d]: Using NTP: %s, fallback: time.cloudflare.com\r\n", __FILE__, __LINE__, ntp.c_str());
  configTime(0, 0, ntp.c_str(), "time.cloudflare.com");
  
  for (int i = 0; i < SNTP_MAX_SERVERS; i++) {
    const char *srv = esp_sntp_getservername(i);
    if (srv && strlen(srv) > 0) {
      Serial.printf("%s [%d]: SNTP server[%d]: %s\r\n", __FILE__, __LINE__, i, srv);
    }
  }

  Serial.printf("%s [%d]: Time synchronization...\r\n", __FILE__, __LINE__);

  // Wait for time to be set
  if (getLocalTime(&timeinfo)) {
    sync_status = true;
    Serial.printf("%s [%d]: Time synchronization succeed!\r\n", __FILE__, __LINE__);
    prefs.putUInt("last_sync", (uint32_t)now); // save epoch time of last sync
  } else {
    Serial.printf("%s [%d]: Time synchronization failed...\r\n", __FILE__, __LINE__);
  }

  Serial.printf("%s [%d]: Current time - %s\r\n", __FILE__, __LINE__, asctime(&timeinfo));

  prefs.end();
  return sync_status;
} /* setClock() */

void TRMNL::getSensorSamples()
{
    long lSleepTime;
    lSleepTime = (_bCO2) ? 5000 : 1000; // sleep for 5 or 1 seconds depending on the sensor type
    lSleepTime -= (millis() - _lSensorTime); // how much time passed since we triggered samples?
    if (lSleepTime > 0) {
        delay(lSleepTime);
    }
    if (_bCO2 && scd41.getSample() == SCD41_SUCCESS) {
        time((time_t *)&_u32SensorEpoch); // get the UTC epoch time that the sample was captured
        _iCO2 = scd41.co2();
        _iTemperature = scd41.temperature();
        _iHumidity = scd41.humidity();
    }
    if (_iSensorType >= 0) {
        BBT_SAMPLE bbts;
        if (bbt.getSample(&bbts) == BBT_SUCCESS) {
            uint32_t u32Caps = bbt.caps();
            time((time_t *)&_u32SensorEpoch); // get the UTC epoch time that the same was captured
            if (u32Caps & BBT_CAP_TEMPERATURE) {
                _iTemperature = bbts.temperature;
            }
            if (u32Caps & BBT_CAP_HUMIDITY) {
                _iHumidity = bbts.humidity;
            }
            if (u32Caps & BBT_CAP_PRESSURE) {
                _iPressure = bbts.pressure;
            }
          Serial.printf("%s [%d]: Got bb_temperature sample: Temp = %d.%dC\r\n", __FILE__, __LINE__, _iTemperature/10, _iTemperature % 10);
        } else {
          Serial.printf("%s [%d]: bb_temperature sample failed\r\n", __FILE__, __LINE__);
        }
        bbt.stop(); // turn off the sensor to conserve power
    }
} /* getSensorSamples() */

int TRMNL::getAPI(const char *szAPIKey, const char *szURL, float fVoltage)
{
int rc = TRMNL_ERROR;
bool bTimeValid = false;
    
    if (_bTimeSync) {
        bTimeValid = setClock(); // Get the current time if not already set
    }
    https.begin(szURL);
    https.setTimeout(15000);
    https.setConnectTimeout(15000);
    if ((_bCO2 || _iSensorType >= 0) && bTimeValid) { // Add HTTP headers for the sensor samples
        char *szTemp, szPart[128];
        // Sensors are detected and active, get the latest sample value(s)
        uint32_t u32SampleTime;
        time((time_t *)&u32SampleTime); // get the UTC epoch time that the samples were captured
        getSensorSamples();
        szTemp = (char *)malloc(1024); // make sure we have enough space, but don't use the stack because it's small
        if (_iCO2 != 0) { // valid data from SCD4x for CO2, Temperature and Humidity
          // create the multi-value string to pass as a HTTP header
          sprintf(szTemp, "make=Sensirion;model=SCD41;kind=carbon_dioxide;value=%d;unit=parts_per_million;created_at=%lu,make=Sensirion;model=SCD41;kind=temperature;value=%f;unit=celsius;created_at=%lu,make=Sensirion;model=SCD41;kind=humidity;value=%d;unit=percent;created_at=%lu", _iCO2, u32SampleTime, (float)_iTemperature / 10.0f, u32SampleTime, _iHumidity, u32SampleTime);
          Serial.printf("%s [%d] Adding SCD41 data to api request: CO2: %d, Temp: %d.%dC, Humidity: %d%%", __FILE__, __LINE__, _iCO2, _iTemperature/10, _iTemperature % 10, _iHumidity);
        }
        if (_iSensorType >= 0) { // we have data from another bb_temperature supported sensor too; add it
          if (_iCO2 != 0) {
            strcat(szTemp, ","); // separate from CO2 data
          } else {
            szTemp[0] = 0;
          }
          Serial.printf("%s [%d] Adding bb_temperature data to api request: pressure: %d, Temp: %d.%dC, Humidity: %d%%", __FILE__, __LINE__, _iPressure, _iTemperature/10, _iTemperature % 10, _iHumidity);
          sprintf(szPart, "make=%s;model=%s;kind=temperature;value=%f;unit=celsius;created_at=%lu",szMakers[_iSensorType], szDevices[_iSensorType], (float)_iTemperature / 10.0f, u32SampleTime);
          strcat(szTemp, szPart);
          if (_iHumidity > 0) { // add humidity
            sprintf(szPart, ",make=%s;model=%s;kind=humidity;value=%d;unit=percent;created_at=%lu",szMakers[_iSensorType], szDevices[_iSensorType], _iHumidity, u32SampleTime);
            strcat(szTemp, szPart);
          }
          if (_iPressure > 0) {
            sprintf(szPart, ",make=%s;model=%s;kind=pressure;value=%d;unit=hectopascal;created_at=%lu",szMakers[_iSensorType], szDevices[_iSensorType], _iPressure, u32SampleTime);
            strcat(szTemp, szPart);
          }
        }
        if (_iCO2 != 0 || _iSensorType >= 0) {
          https.addHeader("SENSORS", szTemp);
        } else {
          Serial.printf("%s [%d] Sensor data not available", __FILE__, __LINE__);
        }
        free(szTemp);
    } // add sensor info to HTTP headers
 // add the HTTP headers
 // https.addHeader("ID", inputs.macAddress);
  https.addHeader("Content-Type", "application/json");
  https.addHeader("Access-Token", szAPIKey);
//  https.addHeader("Refresh-Rate", String(inputs.refreshRate));
  https.addHeader("Battery-Voltage", String(fVoltage));
//  https.addHeader("FW-Version", inputs.firmwareVersion);
  https.addHeader("Model", "byod");
  https.addHeader("RSSI", String(WiFi.RSSI()));
  https.addHeader("Width", String(_iWidth));
  https.addHeader("Height", String(_iHeight));
  _httpCode = https.GET();
   // httpCode will be negative on error
  Serial.printf("https GET returned: %d\n", _httpCode);
  if (_httpCode >= 200 && _httpCode < 300) { // success, get payload
      String payload = https.getString();
      size_t size = https.getSize();
      Serial.printf("Content size: %d\n", size);
      Serial.printf("Payload: %s\n", payload.c_str());
      JsonDocument doc;
      DeserializationError error = deserializeJson(doc, payload);
      _status = doc["status"];
      _image_url = doc["image_url"] | "";
      _refresh_rate = doc["refresh_rate"];
      rc = TRMNL_SUCCESS;
  } else {
      _refresh_rate = 900; // default to 15 minutes
      _status = API_STATUS_INVALID;
  }
    https.end();
  return rc;
} /* getAPI() */

int TRMNL::getImage(uint8_t **pBuffer, int *pSize)
{
    int rc = TRMNL_ERROR;
    
    Serial.println("TRMNL getImage()");
    
    if (_status == API_STATUS_OK && _image_url.length() > 0) {
        https.begin(_image_url);
        https.setTimeout(15000);
        https.setConnectTimeout(15000);
        _httpCode = https.GET();
        Serial.printf("%s [%d]: [HTTPS] GET... code: %d\r\n", __FILE__, __LINE__, _httpCode);
        Serial.printf("%s [%d]: RSSI: %d\r\n", __FILE__, __LINE__, WiFi.RSSI());
        if (_httpCode == HTTP_CODE_OK) {
            Serial.printf("%s [%d]: Content size: %d\r\n", __FILE__, __LINE__, https.getSize());
            String payload = https.getString();
            *pSize = payload.length();
            if (*pSize) {
                Serial.println("Got the image!");
                *pBuffer = _pImage = (uint8_t *)malloc(*pSize);
                memcpy(*pBuffer, payload.c_str(), *pSize);
                rc = TRMNL_SUCCESS;
            }
        }
        https.end();
    }
    return rc;
} /* getImage() */

void TRMNL::disconnectWiFi(void)
{
    Serial.println("TRMNL disconnectWiFi()");
    if (WiFi.status() == WL_CONNECTED) {
        WiFi.disconnect(true, true, 500);
    }
} /* disconnectWiFi() */

void TRMNL::sleep()
{
    Serial.println("TRMNL sleep()");
    esp_sleep_enable_timer_wakeup((uint64_t)_refresh_rate * 1000000L);
    // If the user defined a button to wake us up (besides the timer)
    if (_wake_gpio != 255) {
#if CONFIG_IDF_TARGET_ESP32
        esp_sleep_enable_ext1_wakeup(1 << _wake_gpio, ESP_EXT1_WAKEUP_ALL_LOW);
#elif CONFIG_IDF_TARGET_ESP32C3
        esp_deep_sleep_enable_gpio_wakeup(1 << _wake_gpio, ESP_GPIO_WAKEUP_GPIO_LOW);
#elif CONFIG_IDF_TARGET_ESP32S3
        esp_sleep_enable_ext0_wakeup((gpio_num_t)_wake_gpio, 0);
#endif
    }
    esp_deep_sleep_start();
} /* sleep() */
