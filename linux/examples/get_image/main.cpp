#include <trmnl_lib.h>
#include <stdio.h>
#include <stdlib.h>

TRMNL trmnl;

void showImage(const char *szDir, uint8_t *pImage, int iImageSize)
{
char szCMD[256], szName[256];
FILE *f;

    strcpy(szName, szDir);
    if (szName[strlen(szName)-1] != '/') strcat(szName, "/");
    strcat(szName, "temp_image.png");
    f = fopen(szName, "w+b");
    if (f) {
        fwrite(pImage, iImageSize, 1, f);
        fflush(f);
        fclose(f);
    } else {
        printf("Error opening %s\n", szName);
        return;
    }
    snprintf(szCMD, sizeof(szCMD), "open %s", szName);
    system(szCMD);

} /* showImage() */

int main(int argc, char *argv[])
{
int rc;

    uint8_t *pImage;
    int iImageSize;

    printf("Starting TRMNL demo\n");
    if (argc != 3) {
        printf("Usage: trmnl <API Key> <temp image dir>\n");
        printf("Example: ./trmnl_demo abc_123 ~/home/Downloads\n");
        return 0;
    }
    rc = trmnl.getAPI(argv[1]); // also reads and sends the sensor values if valid
    if (rc == TRMNL_SUCCESS) {
        printf("API request succeeded\n");
        rc = trmnl.getImage(&pImage, &iImageSize);
        if (rc == TRMNL_SUCCESS) {
            printf("Image download succeeded (%d bytes)\n", iImageSize);
            showImage(argv[2], pImage, iImageSize); // save and use "open" shell cmd
            trmnl.freeImage();
        }
    }
    return 0;
} /* main() */
