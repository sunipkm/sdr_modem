#include "txmodem.h"
#include <stdlib.h>

int main(int argc, char **argv)
{
    txmodem TX[1];
    if (txmodem_init(TX, uio_get_id("tx_ipcore"), uio_get_id("tx_dma")) < 0)
    {
        eprintf("Error initializing TX modem");
        return 0;
    }
    FILE *fPhoto = NULL;
    char *photoName = argv[1];
    fPhoto = fopen(photoName, "rb");
    if (fPhoto == NULL)
    {
        return -1;
    }

    // Open file, get size.
    fseek(fPhoto, 0L, SEEK_END);
    ssize_t size = ftell(fPhoto);
    fseek(fPhoto, 0, SEEK_SET);

    uint8_t *buffer = malloc(size);
    fread(buffer, size, 1, fPhoto);

    // Transmit
    if (txmodem_write(TX, buffer, size) < 0)
    {
        eprintf("Check size");
    }

    free(buffer);
    fclose(fPhoto);
}