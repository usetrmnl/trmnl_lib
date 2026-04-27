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
#ifndef __TRMNL_LIB__
#define __TRMNL_LIB__

#ifdef __LINUX__
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <string>
#ifndef __MACH__
#include <linux/types.h>
#include <linux/spi/spidev.h>
#include <linux/i2c-dev.h>
#include <i2c/smbus.h>
#endif // !__MACH__
#include <time.h>

#else // !LINUX

#ifdef ARDUINO
#include <Arduino.h>
#endif // ARDUINO
#endif // !__LINUX__

#define API_STATUS_OK 0
#define API_STATUS_NOT_REGISTERED 202
#define API_STATUS_RESET 500
#define API_STATUS_INVALID 999

enum {
    TRMNL_SUCCESS = 0,
    TRMNL_ERROR
};

class TRMNL
{
  public:
    TRMNL() {_iTemperature = _iHumidity = _iCO2 = _iPressure = 0;}
    ~TRMNL() {}
    
    void setSensorBus(uint8_t sda, uint8_t scl);
    void setDisplaySize(int w, int h) {_iWidth = w; _iHeight = h;}
    bool connectWiFi(const char *ssid, const char *pw);
    int getAPI(const char *szAPIKey, const char *szURL = "https://trmnl.app/api/display", float fVoltage = 4.2f);
    uint64_t getAPIStatus() {return _status;}
    void setWakeButton(uint8_t u8Wake) {_wake_gpio = u8Wake;}
    int getImage(uint8_t **pu8Buffer, int *piSize);
    void disconnectWiFi(void);
    void freeImage() {free(_pImage); _pImage = nullptr;}
    int getHTTPCode() {return _httpCode;}
    void sleep();
    float getTemperature() {return (float)_iTemperature / 10.0f;}
    int getPressure() {return _iPressure;}
    int getHumidity() {return _iHumidity;}
    int getCo2() {return _iCO2;}
    int getSleepTime() {return _refresh_rate;}
    void setTimeSync(bool bSync) {_bTimeSync = bSync;}
protected:
    bool setClock();
    void getSensorSamples();
    
  private:
#ifndef __MACH__
    bool _bCO2, _bTimeSync = false;
    int _iSensorType;
    long _lSensorTime;
    uint32_t _u32SensorEpoch;
#else
    bool _bTimeSync = true;
#endif // !__MACH__
    int _iWidth = 800; // default to OG size
    int _iHeight = 480;
    int _iHumidity, _iTemperature, _iCO2, _iPressure, _httpCode;
#ifdef __LINUX__
    std::string _image_url;
#else // Arduino
    String _image_url;
#endif
    uint8_t *_pImage;
    uint64_t _status = API_STATUS_INVALID;
    uint64_t _refresh_rate = 30; // default to 30 seconds
    uint8_t _wake_gpio = 255;
};

#endif // __TRMNL_LIB__
