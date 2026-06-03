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
#include <trmnl_lib.h>
#include <termios.h>
#include <stdio.h>
#include <signal.h>
#include <stdexcept>
#include <stdlib.h>
#include <SDL2/SDL.h>
volatile bool bQuit = false;
SDL_Window *win;
SDL_Surface *canvas, *winSurface;

// Set this to the sizze of images you will receive
#define IMAGE_WIDTH 800
#define IMAGE_HEIGHT 480

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
// Draw the current image onto a SDL window
//
void ShowSDLImage(void)
{
uint16_t *d, u16, r, g, b;
uint8_t *s;
int iSrcPitch;
int x, y, rOff = 2, bOff = 0;
uint8_t ucTemp[768]; // temporary palette for grayscale

    if (iBGR) {
	    rOff = 0;
	    bOff = 2;
    }
    if (!pPalette && iBpp <= 8) { // create a grayscale palette if needed
       int iDelta, iCount = 1<<iBpp;
       int iGray=0;
       iDelta = 255/(iCount-1);
       for (x=0; x<iCount; x++) {
          ucTemp[x*3] = (uint8_t)iGray;
          ucTemp[x*3+1] = (uint8_t)iGray;
          ucTemp[x*3+2] = (uint8_t)iGray;
          iGray += iDelta;
       }
       pPalette = ucTemp;
    } else {
    }
    if (iPixelType == PNG_PIXEL_TRUECOLOR) {
	    iBpp = 24;
    } else if (iPixelType == PNG_PIXEL_TRUECOLOR_ALPHA) {
	    iBpp = 32;
    }
    iSrcPitch = (iWidth * iBpp)/8;
    for (y=0; y<iHeight; y++) {
        s = pBitmap + (y*iSrcPitch);
        d = (uint16_t *)canvas->pixels;
        d += y * iWidth;
        switch(iBpp) {
            case 1:
                {
                uint8_t uc;
                uc = *s++;
                for (x=0; x<iWidth; x++) {
                    if (uc & 0x80) {
                       r = pPalette[rOff+3];
                       g = pPalette[4];
                       b = pPalette[bOff+3];
                    } else {
                       r = pPalette[rOff];
                       g = pPalette[1];
                       b = pPalette[bOff];
                    }
                    *d++ = ((r & 0xf8)<<8) | ((g & 0xfc) << 3) | (b >> 3);
                    uc <<= 1;
                    if ((x & 7) == 7) uc = *s++;
                } // for x
                }
                break;
                case 2:
                {
                uint8_t c, uc;
                uc = *s++;
                for (x=0; x<iWidth; x++) {
                    c = uc >> 6;
                    r = pPalette[c*3+rOff];
                    g = pPalette[c*3+1];
                    b = pPalette[c*3+bOff];
                    *d++ = ((r & 0xf8)<<8) | ((g & 0xfc) << 3) | (b >> 3);
                    uc <<= 2;
                    if ((x & 3) == 3) uc = *s++;
                } // for x
                }
                break;
                case 4:
                {
                uint8_t c, uc;
                uc = *s++;
                for (x=0; x<iWidth; x++) {
                    c = uc >> 4;
                    r = pPalette[c*3+rOff];
                    g = pPalette[c*3+1];
                    b = pPalette[c*3+bOff];
                    *d++ = ((r & 0xf8)<<8) | ((g & 0xfc) << 3) | (b >> 3);
                    uc <<= 4;
                    if ((x & 1) == 1) uc = *s++;
                } // for x
                }
                break;
                case 8:
                {
                uint8_t uc;
                uc = *s++;
                for (x=0; x<iWidth; x++) {
                    r = pPalette[uc*3+rOff];
                    g = pPalette[uc*3+1];
                    b = pPalette[uc*3+bOff];
                    *d++ = ((r & 0xf8)<<8) | ((g & 0xfc) << 3) | (b >> 3);
                    uc = *s++;
                } // for x
                }
                break;
                case 24:
                case 32:
                {
                for (x=0; x<iWidth; x++) {
                    u16 = (s[rOff] & 0xf8)<<8; // R
                    u16 |= (s[1] & 0xfc) << 3; // G
                    u16 |= (s[bOff] >> 3); // B
                    *d++ = u16;
                    s += (iBpp/8);
                } // for x
                }
                break;
            } // switch on bpp
        } // for y
    free(pBitmap); // no longer needed
    winSurface = SDL_GetWindowSurface(win);
    SDL_BlitSurface(canvas, NULL, winSurface, NULL);
    SDL_UpdateWindowSurface(win);
} /* ShowSDLImage() */

void ShowHelp(void)
{
    printf("sdl2_viewer - display TRMNL images in a SDL2 window\nwritten by Larry Bank (bitbank@pobox.com)\nCopyright(c) 2026 BitBank Software, inc.\n");
    printf("Usage: ./sdl2_viewer <Device API key> <optional back-end URL>\n");
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
void setRawMode(bool enable) {
    static struct termios oldt, newt;
    if (enable) {
        tcgetattr(STDIN_FILENO, &oldt);
        newt = oldt;
        newt.c_lflag &= ~(ICANON | ECHO); // Disable buffering and echoing
        tcsetattr(STDIN_FILENO, TCSANOW, &newt);
    } else {
        tcsetattr(STDIN_FILENO, TCSANOW, &oldt);
    }
}
//
// Catch CTRL-C here
//
void signal_handler(int signum)
{
        printf("Ctrl-C hit; exiting...\n");
	bQuit = true;
} /* signal_handler() */

//
// Main program entry point
//
int main(int argc, const char * argv[]) {
int rc, iSize;
uint8_t *pImage;
TRMNL trmnl;
time_t now, next_update;
bool bSSH = false; // flag indicating if we're running from an SSH session

    if (argc != 2 && argc != 3) {
        ShowHelp();
        return -1;
    }
    bSSH = (getenv("SSH_CLIENT") != nullptr);
    printf("Running from SSH = %s\n", (bSSH) ? "Yes" : "No");
    signal(SIGINT, signal_handler); // catch CTRL-C
    if (bSSH) {
        setRawMode(true);
    }
    time(&next_update); // get the current time
    trmnl.setDisplaySize(IMAGE_WIDTH, IMAGE_HEIGHT); // dynamic display size is not supported yet; for future use
    // Create the SDL window
    win = SDL_CreateWindow("TRMNL", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, IMAGE_WIDTH, IMAGE_HEIGHT, SDL_WINDOW_SHOWN | SDL_WINDOW_FULLSCREEN | SDL_WINDOW_OPENGL);
    if (win == nullptr) {
        printf("SDL_CreateWindow Error: %s\n", SDL_GetError());
        return -1;
    }
    // Create a surface to hold the image canvas
    canvas = SDL_CreateRGBSurfaceWithFormat(0, IMAGE_WIDTH, IMAGE_HEIGHT, 16, SDL_PIXELFORMAT_RGB565);
    if (canvas == nullptr) {
        printf("SDL_CreateSurface error %s\n", SDL_GetError());
        SDL_DestroyWindow(win);
        SDL_Quit();
        return -1;
    }
    printf("Created SDL window, about to enter event loop\n");
    while (!bQuit) {
	if (bSSH) { // special way to capture keys from SSH
            fd_set set;
            struct timeval timeout = {0, 1000}; // 1ms timeout to keep SDL responsive
            FD_ZERO(&set);
            FD_SET(STDIN_FILENO, &set);
            if (select(STDIN_FILENO + 1, &set, NULL, NULL, &timeout) > 0) {
                if (FD_ISSET(STDIN_FILENO, &set)) {
                    char c = getchar();
                    if (c == '\n' || c == '\r') { // Detect Enter key
                        printf("Enter key pressed, skipping to next in playlist...\n");
		        next_update = now;
                    } else if (c == 0x1b) { // ESC key
                        bQuit = true;
		    }
                }
            }
	} // running from SSH session
        SDL_Event e;
        while (SDL_PollEvent(&e)) { // take care of queued events
            if (e.type == SDL_WINDOWEVENT && e.window.event == SDL_WINDOWEVENT_CLOSE) {
                bQuit = true;
            }
            if (e.type == SDL_QUIT || (e.type == SDL_KEYDOWN && e.key.keysym.sym == SDLK_ESCAPE)) {
                bQuit = true;
            }
            if (e.type == SDL_KEYDOWN && e.key.keysym.sym == SDLK_RETURN) {
                // skip to next image before time expires
                next_update = now;
            }
        } // while SDL events
        SDL_Delay(100);
        time(&now);
        if (now > next_update) {
            if (argc == 2) {
                rc = trmnl.getAPI(argv[1]);
            } else {
	        rc = trmnl.getAPI(argv[1], argv[2]);
	    }
            if (rc == TRMNL_SUCCESS) {
                printf("getAPI succeeded\n");
                next_update = now + trmnl.getSleepTime();
                rc = trmnl.getImage(&pImage, &iSize);
                if (rc == TRMNL_SUCCESS) {
                    printf("getImage succeed, size = %d bytes\n", iSize);
                    if (decodeImage(pImage, iSize)) {
                        ShowSDLImage();
                    }
                    trmnl.freeImage();
                }
            } else {
                printf("getAPI failed with error: %d, exiting...\n", trmnl.getHTTPCode());
		bQuit = true;
	    }
        }
    } // while SDL window displayed
    printf("exiting...\n");
    // Clean up
    if (bSSH) {
        setRawMode(false);
    }
    SDL_FreeSurface(canvas);
    SDL_FreeSurface(winSurface);
    SDL_DestroyWindow(win);
    SDL_Quit();
    return 0;
} /* main() */


