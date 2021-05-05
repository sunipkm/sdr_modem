#include "rxmodem.h"
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

int main(int argc, char **argv)
{
    rxmodem RX[1];
    if (rxmodem_init(RX, uio_get_id("rx_ipcore"), uio_get_id("rx_dma")) < 0)
    {
        eprintf("Error initializing RX Modem");
        return 0;
    }

    FILE *fPhoto = NULL;
    char *photoName = argv[1];
    fPhoto = fopen(photoName, "wb");
    if (fPhoto == NULL)
    {
        return 0;
    }

    ssize_t bufferSize = rxmodem_receive(RX);

    if (bufferSize < 1)
    {
        eprintf("Error: Buffer size is less than 1.");
        return -1;
    }

    uint8_t *buffer = malloc(bufferSize);

    ssize_t rd = 0;

    if ((rd = rxmodem_read(RX, buffer, bufferSize)) != bufferSize)
    {
        eprintf("Error: Read %d out of %d", rd, bufferSize);
    }

    fwrite(buffer, bufferSize, 1, fPhoto);

    free(buffer);
    fclose(fPhoto);

    sync();
    return 0;
}