#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <unistd.h>
#include <time.h>
#include <signal.h>
#include "rxmodem.h"

volatile sig_atomic_t done = 0;
void sighandler(int sig)
{
    done = 1;
}

int main(int argc, char *argv[])
{
    if (argc == 1)
    {
        printf("You can pass FR Loop Bandwidth as a number between 0 and 127 as a command line parameter\nYou can alternatively pass FR Loop BW, damping factor and loop gain as floats\n\n");
    }
    signal(SIGINT | SIGHUP, &sighandler);
    printf("Starting program, press Ctrl + C to exit\n");
    rxmodem dev[1];
    if (rxmodem_init(dev, uio_get_id("rx_ipcore"), uio_get_id("rx_dma")) < 0)
        return -1;
    if (argc == 2)
    {
        int fr_loop_idx = atoi(argv[1]);
        printf("FR Loop BW index provided: %d, ", fr_loop_idx);
        if (fr_loop_idx < 0 || fr_loop_idx > 127)
            fr_loop_idx = 40; // default
        dev->conf->fr_loop_bw = fr_loop_idx;
        printf("FR Loop BW index set: %d\n", dev->conf->fr_loop_bw);
    }
    if (argc == 4)
    {
        float loop_bw = atof(argv[1]);
        float damp = atof(argv[2]);
        float gain = atof(argv[3]);
        if ((loop_bw <= 0) || (damp <= 0) || (gain <= 0))
        {
            printf("Unphysical values %f %f %f, exiting\n", loop_bw, damp, gain);
            rxmodem_destroy(dev);
            return 0;
        }
        else
        {
            printf("Entered %f %f %f, exiting\n", loop_bw, damp, gain);
        }
        rxmodem_enable_ext_fr(dev, loop_bw, damp, gain);
    }
    rxmodem_reset(dev, dev->conf);
    while (!done)
    {
        printf("\n\nWaiting to receive...\n");
        fflush(stdout);
        ssize_t rcv_sz = rxmodem_receive(dev);
        if (rcv_sz < 0)
        {
            eprintf("%s: Receive size = %d\n", __func__, rcv_sz);
            continue;
        }
        printf("%s: Received data size: %d\n", __func__, rcv_sz);
        fflush(stdout);
        char *buf = (char *)malloc(rcv_sz);
        ssize_t rd_sz = rxmodem_read(dev, (uint8_t *)buf, rcv_sz);
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
    if (argc == 4)
    {
        rxmodem_disable_ext_fr(dev);
    }
    rxmodem_destroy(dev);
    return 0;
}