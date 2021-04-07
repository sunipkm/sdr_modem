#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <time.h>
#include <signal.h>
#include "rxmodem.h"

#define eprintf(...)              \
    fprintf(stderr, __VA_ARGS__); \
    fflush(stderr)

volatile sig_atomic_t done = 0;
int sighandler(int sig)
{
    done = 1;
}

int main(int argc, char *argv[])
{
    signal(SIGINT | SIGHUP, &sighandler);
    printf("Starting program, press Ctrl + C to exit\n");
    rxmodem dev[1];
    if (rxmodem_init(dev, uio_get_id("rx_ipcore"), uio_get_id("rx_dma")) < 0)
        return -1;
    rxmodem_reset(dev, dev->conf);
    while (!done)
    {
        ssize_t rcv_sz = rxmodem_receive(dev);
        if (rcv_sz < 0)
        {
            eprintf("%s: Receive size = %d\n", __func__, rcv_sz);
            continue;
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
    }
    rxmodem_destroy(dev);
    return 0;
}