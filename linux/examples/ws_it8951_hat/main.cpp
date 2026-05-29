//
// C++ example for TRMNL library
// written by Larry Bank (bitbank@pobox.com)
// Project started 5/19/2026
// Copyright (c) 2026 BitBank Software, Inc.
//
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
#include <PNGdec.h>
#include <JPEGDEC.h>
#include <FastEPD.h>
#include <trmnl_lib.h>
#include <pthread.h>
#include <stdio.h>
#include <stdexcept>
#include <stdlib.h>
FASTEPD epd;
volatile bool bQuit;
time_t now, next_update;
pthread_t tinfo;
char szAPI[64], szURL[256];
int iArgs;
// Set this to the size of images you will receive
#define IMAGE_WIDTH 1872
#define IMAGE_HEIGHT 1404
// BCM GPIO numbers for the RPI HAT
#define IT8951_CS 8
#define IT8951_SPI 0
#define IT8951_RST 17
#define IT8951_BUSY 24
#define IT8951_ITE_EN -1
#define IT8951_EN -1

int iBGR = 0; // reversed R/B order

PNG png;
JPEGDEC jpg;
int iWidth, iHeight, iBpp, iPixelType;
uint8_t *pBitmap, *pPalette=NULL;

const char *szPNGErrors[] = {"Success", "Invalid Parameter", "Decoding", "Out of memory", "No buffer allocated", "Unsupported feature", "Invalid file", "Too big", "Quit early"};
const char *szJPEGErrors[] = {"Success", "Invalid Parameter", "Decoding", "Unsupported feature", "Invalid file", "Out of memory"};

//
// Decode the BMP file
//
int DecodeBMP(uint8_t *pData, int iSize)
{
    int iOffBits; // offset to bitmap data
    int y, iDestPitch=0, iPitch;
    uint8_t bFlipped = 0;
    uint8_t *s, *d;

    iWidth = *(int16_t *)&pData[18];
    iHeight = *(int16_t *)&pData[22];
    if (iHeight < 0) {
        iHeight = -iHeight;
    } else {
	bFlipped = 1;
    }
    iBpp = *(int16_t *)&pData[28];
    iOffBits = *(uint16_t *)&pData[10];
    switch (iBpp) {
        case 1:
	    iDestPitch = ((iWidth+7)>>3);
	    iPixelType = PNG_PIXEL_INDEXED;
            break;
	case 4:
	    iDestPitch = ((iWidth+1)>>1);
	    iPixelType = PNG_PIXEL_INDEXED;
	    break;
	case 8:
	    iDestPitch = iWidth;
	    iPixelType = PNG_PIXEL_INDEXED;
	    break;
	case 24:
	    iDestPitch = iWidth*3;
	    iPixelType = PNG_PIXEL_TRUECOLOR;
	    iBGR = 1; // reversed R/B order
	    break;
	case 32:
	    iDestPitch = iWidth*4;
	    iPixelType = PNG_PIXEL_TRUECOLOR_ALPHA;
	    iBGR = 1;
	    break;
    } // switch on bpp
    iPitch = (iDestPitch + 3) & 0xfffc; // must be DWORD aligned
    if (bFlipped)
    {
        iOffBits += ((iHeight-1) * iPitch); // start from bottom
        iPitch = -iPitch;
    }
    pBitmap = (uint8_t *)malloc(iHeight * iDestPitch);
    s = &pData[iOffBits];
    d = pBitmap;
    for (y=0; y<iHeight; y++) { // copy the bitmap to the common format
        memcpy(d, s, iDestPitch);
	s += iPitch;
	d += iDestPitch;
    }
    // Adjust the palette for 3-byte entries (if there is one)
    if (iBpp <= 8) {
	int iColors = 1<<iBpp;
	d = pPalette = pData;
        iOffBits = *(uint16_t *)&pData[10];
        s = &pData[iOffBits - (4 * iColors)];
        for (y=0; y<iColors; y++) {
            d[0] = s[0]; d[1] = s[1]; d[2] = s[2];
	    s += 4;
	    d += 3;
	}
    } else {
        pPalette = NULL;
    }
    return PNG_SUCCESS; // re-use this return code
} /* DecodeBMP() */
//
// Decode the JPEG file into an uncompressed bitmap
//
int DecodeJPEG(uint8_t *pData, int iSize)
{
int rc, iPitch;
    rc = jpg.openRAM(pData, iSize, NULL);
    if (!rc) {
        rc = jpg.getLastError();
        printf("JPEG open returned error: %s\n", szJPEGErrors[rc]);
        return -1; // only show the error once
    }
    iWidth = jpg.getWidth();
    iHeight = jpg.getHeight();
    iBpp = jpg.getBpp();
    if (iBpp == 8) {
        iPixelType = PNG_PIXEL_GRAYSCALE;
        jpg.setPixelType(EIGHT_BIT_GRAYSCALE);
        iPitch = iWidth;
    } else {
        iPixelType = PNG_PIXEL_TRUECOLOR_ALPHA;
        jpg.setPixelType(RGB8888);
        iPitch = iWidth*4;
        iBpp = 32; // output is 32-bits
    }
    pBitmap = (uint8_t *)malloc(iPitch * (iHeight+15));
    jpg.setFramebuffer(pBitmap);
    jpg.decode(0, 0, 0);
    return jpg.getLastError();
} /* DecodeJPEG() */

//
// Decode the PNG file into an uncompressed bitmap
//
int DecodePNG(uint8_t *pData, int iSize)
{
int rc;
    rc = png.openRAM(pData, iSize, NULL);
    if (rc != PNG_SUCCESS) {
        printf("PNG open returned error: %s\n", szPNGErrors[rc]);
        return -1; // only show the error once
    }
    iBGR = 1; // reversed R/B order
    iWidth = png.getWidth();
    iHeight = png.getHeight();
    iBpp = png.getBpp();
    pPalette = png.getPalette();
    iPixelType = png.getPixelType();
    if (iPixelType != PNG_PIXEL_INDEXED) pPalette = NULL; // tell other code that there's no palette present
    pBitmap = (uint8_t *)malloc(png.getBufferSize());
    png.setBuffer(pBitmap);
    rc = png.decode(NULL, 0);
    return rc;
} /* DecodePNG() */
//
// Draw the current image onto the eink display
//
void ShowImage(void)
{
uint8_t *d, *s;
int iSrcPitch, iPitch;
int x, y, rOff = 2, bOff = 0;
uint8_t ucTemp[256]; // temporary palette for grayscale

    if (iBGR) {
	    rOff = 0;
	    bOff = 2;
    }
    if (!pPalette && iBpp <= 8) { // create a grayscale palette if needed
       int iDelta, iCount = 1<<iBpp;
       int iGray=0;
       iDelta = 255/(iCount-1);
       for (x=0; x<iCount; x++) {
          ucTemp[x] = (uint8_t)iGray;
          iGray += iDelta;
       }
       pPalette = ucTemp;
       if (iBpp == 1) {
           iPitch = (epd.width()+7)/8;
           epd.setMode(BB_MODE_1BPP);
       } else if (iBpp == 2) {
           iPitch = (epd.width()+3)/4;
           epd.setMode(BB_MODE_2BPP);
       } else {
           iPitch = epd.width()/2;
           epd.setMode(BB_MODE_4BPP);
       }
    } else if (pPalette) {
       int iGray, iCount = 1<<iBpp;
       for (x=0; x<iCount; x++) {
          iGray = pPalette[x*3] + pPalette[(x*3)+1]*2 + pPalette[(x*3)+2];
          ucTemp[x] = (iGray+2)/4; // grayscale
       }
    }
    if (iPixelType == PNG_PIXEL_TRUECOLOR) {
	    iBpp = 24;
    } else if (iPixelType == PNG_PIXEL_TRUECOLOR_ALPHA) {
	    iBpp = 32;
    }
    iSrcPitch = (iWidth * iBpp)/8;
    for (y=0; y<iHeight; y++) {
        s = pBitmap + (y*iSrcPitch);
        d = epd.currentBuffer();
        d += y * iPitch;
        switch(iBpp) {
            case 1:
                memcpy(d, s, iPitch);
                break;
            case 2:
                {
                uint8_t c, uc, uc2;
                uc = *s++;
                uc2 = 0;
                for (x=0; x<iWidth; x++) {
                    c = uc >> 6;
                    uc2 <<= 2;
                    uc2 |= (ucTemp[c]>>6);
                    uc <<= 2;
                    if ((x & 3) == 3) {
                        uc = *s++;
                        *d++ = uc2;
                    }
                } // for x
                }
                break;
                case 4:
                {
                uint8_t c, uc, uc2;
                uc = *s++;
                uc2 = 0;
                for (x=0; x<iWidth; x++) {
                    c = uc >> 4;
                    uc2 <<= 4;
                    uc2 |= (ucTemp[c] >> 4);
                    uc <<= 4;
                    if ((x & 1) == 1) {
                        uc = *s++;
                        *d++ = uc2;
                    }
                } // for x
                }
                break;
                case 8:
                {
                uint8_t uc;
                uc = 0;
                for (x=0; x<iWidth; x++) {
                    uc <<= 4;
                    uc |= (ucTemp[*s++] >> 4);
                    if (x & 1) {
                       *d++ = uc;
                    }
                } // for x
                }
                break;
                case 24: // future
                case 32:
                break;
            } // switch on bpp
        } // for y
    free(pBitmap); // no longer needed
    epd.fullUpdate(CLEAR_SLOW, false);
} /* ShowImage() */

void ShowHelp(void)
{
    printf("IT8951 HAT Viewer - display TRMNL images on a Waveshare IT8951 Eink display\nwritten by Larry Bank (bitbank@pobox.com)\nCopyright(c) 2026 BitBank Software, inc.\n");
    printf("Usage: ./trmnl_hat <Device API key> <optional back-end URL>\n");
} /* ShowHelp() */
//
// Figure out the image type and decode it
// returns 1 for success, 0 for failure
//
int decodeImage(uint8_t *pData, int iSize) {
    int rc;
    
    if (iSize < 64) { // invalid file
        printf("Invalid image file\n");
        return 0;
    }
    if (pData[0] == 'B' && pData[1] == 'M') { // it's a BMP file
        rc = DecodeBMP(pData, iSize);
    } else if (pData[0] == 0xff && pData[1] == 0xd8) { // JPEG
        rc = DecodeJPEG(pData, iSize);
        if (rc != JPEG_SUCCESS) {
            if (rc > 0) {
                printf("JPEG decode returned error: %s\n", szJPEGErrors[rc]);
            }
            return 0;
        }
    } else {
        rc = DecodePNG(pData, iSize);
        if (rc != PNG_SUCCESS) {
            if (rc > 0) {
                printf("PNG decode returned error: %s\n", szPNGErrors[rc]);
            }
            return 0;
        } else {
            printf("PNG decode succeeded\n");
        }
    }
    return 1;
} /* decodeImage() */

void *TRMNL_Thread(void *p)
{       
int rc, iSize;
uint8_t *pImage;
TRMNL trmnl;

    trmnl.setDisplaySize(IMAGE_WIDTH, IMAGE_HEIGHT); // dynamic display size is not supported yet; for future use
    // Initialize the FastEPD library
    rc = epd.initIT8951(IT8951_SPI, 0, 0, IT8951_CS, IT8951_BUSY, IT8951_RST, IT8951_EN, IT8951_ITE_EN);
    if (rc != BBEP_SUCCESS) {
         printf("initIT8951 returned error: %d\n", rc);
         return nullptr;
    }
    rc = epd.setPanelSize(BBEP_DISPLAY_ED078KC2); // 7.8" 1872x1404 panel
    if (rc != BBEP_SUCCESS) {
        printf("setPanelSize returned %d\n", rc);
        return nullptr;
    }
    epd.fillScreen(BBEP_WHITE);
    bQuit = false;
    while (!bQuit) {
        time(&now);
        if (now > next_update) {
            if (iArgs == 2) {
                rc = trmnl.getAPI(szAPI);
            } else {
                rc = trmnl.getAPI(szAPI, szURL);
            }
            if (rc == TRMNL_SUCCESS) {
                printf("getAPI succeeded\n");
                next_update = now + trmnl.getSleepTime();
                rc = trmnl.getImage(&pImage, &iSize);
                if (rc == TRMNL_SUCCESS) {
                    printf("getImage succeed, size = %d bytes\n", iSize);
                    if (decodeImage(pImage, iSize)) {
                        ShowImage();
                    }
                    trmnl.freeImage();
                }
            } else {
                printf("getAPI failed with error: %d, exiting...\n", trmnl.getHTTPCode());
                bQuit = true;
            }
        } // update time
        usleep(100000); // loop 10x per second looking for updates
    } // while (!bQuit)
    return nullptr;
} /* TRMNL_Thread() */

//
// Main program entry point
//
int main(int argc, const char * argv[]) {
    
    if (argc != 2 && argc != 3) {
        ShowHelp();
        return -1;
    }
    iArgs = argc;
    strncpy(szAPI, argv[1], sizeof(szAPI));
    if (argc == 3) {
        strncpy(szURL, argv[2], sizeof(szURL));
    }
    pthread_create(&tinfo, NULL, TRMNL_Thread, NULL); // create a background thread to update the external display
    while (1) {
        if (getchar()) {
            printf("ENTER pressed - skip to next item in playlist...\n");
            next_update = 0; // force an update now
        }
    } // while 1
    printf("exiting...\n");
    // Clean up
    epd.deInit();
    return 0;
} /* main() */

