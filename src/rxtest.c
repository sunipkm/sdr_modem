#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <time.h>
#include "rxmodem.h"

#define eprintf(...)              \
    fprintf(stderr, __VA_ARGS__); \
    fflush(stderr)
    
int main(int argc, char *argv[])
{
    printf("Starting program...\n");
    rxmodem dev[1];
    if (rxmodem_init(dev, uio_get_id("rx_ipcore"), uio_get_id("rx_dma")) < 0)
        return -1;
    rxmodem_reset(dev, dev->conf);
    ssize_t rcv_sz = rxmodem_receive(dev);
    if (rcv_sz < 0)
    {
        eprintf("%s: Receive size = %d\n", __func__, rcv_sz);
        return -1;
    }
    printf("%s: Received data size: %d\n", __func__, rcv_sz);
    fflush(stdout);
    char *buf = (char *)malloc(rcv_sz);
    ssize_t rd_sz = rxmodem_read(dev, buf, rcv_sz);
    if (rcv_sz != rd_sz)
    {
        eprintf("%s: Read size = %d out of %d\n", __func__, rd_sz, rcv_sz);
    }
    printf("Message:");
    for (int i = 0; i < rd_sz; i++)
        printf("%c", buf[i]);
#ifdef RXDEBUG
    FILE *fp = fopen("out_rx.txt", "wb");
    for (int i = 0; i < rcv_sz; i++)
    {
        fprintf(fp, "%c", ((char *)dev->dma->mem_virt_addr)[i]);
    }
    fclose(fp);
#endif
    printf("\n");
    free(buf);
    rxmodem_destroy(dev);
    return 0;
}