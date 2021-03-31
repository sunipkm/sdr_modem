/**
 * @file rxmodem.c
 * @author Sunip K. Mukherjee (sunipkmukherjee@gmail.com)
 * @brief 
 * @version 0.1
 * @date 2020-12-28
 * 
 * @copyright Copyright (c) 2020
 * 
 */

#include "libuio.h"
#include "adidma.h"
#include "rxmodem.h"
#include "txrx_packdef.h"
#include "gpiodev/gpiodev.h"
#include <string.h>
#include <stdlib.h>
#define _POSIX_C_SOURCE 199309L
#include <time.h>
#include <errno.h>

#define eprintf(...)              \
    fprintf(stderr, __VA_ARGS__); \
    fflush(stderr)

#define likely(x) __builtin_expect(!!(x), 1)
#define unlikely(x) __builtin_expect(!!(x), 0)

static void *rx_irq_thread(void *__dev);
static pthread_cond_t rx_rcv;
static pthread_mutex_t rx_rcv_m;
static pthread_mutex_t rx_write;

#define RX_FIFO_RST 17 // GPIO 17 resets the FIFO contents

int rxmodem_init(rxmodem *dev, int rxmodem_id, int rxdma_id)
{
    if (gpioSetMode(RX_FIFO_RST, GPIO_OUT) < 0)
        return -1;
    if (dev == NULL)
        return -1;
#ifdef RXDEBUG
    eprintf("%s: %d\n", __func__, __LINE__);
#endif
    dev->bus = (uio_dev *)malloc(sizeof(uio_dev));
    if (dev->bus == NULL)
        return -1;
#ifdef RXDEBUG
    eprintf("%s: %d\n", __func__, __LINE__);
#endif
    if (uio_init(dev->bus, rxmodem_id) < 0)
        return -1;
#ifdef RXDEBUG
    eprintf("%s: %d\n", __func__, __LINE__);
#endif
    // dev->dma = (adidma *)malloc(sizeof(adidma));
    if (dev->dma == NULL)
        return -1;
#ifdef RXDEBUG
    eprintf("%s: %d\n", __func__, __LINE__);
#endif
    if (adidma_init(dev->dma, rxdma_id, 0) < 0)
        return -1;
#ifdef RXDEBUG
    eprintf("%s: %d\n", __func__, __LINE__);
#endif
    /* Save default settings for RX IP */
    dev->conf->rx_enable = 0;
    dev->conf->fr_loop_bw = 40;
    dev->conf->eqmu = 200;
    dev->conf->scopesel = 2;
    dev->conf->pd_threshold = 10;
    dev->conf->eq_bypass = 0;
#ifdef RXDEBUG
    eprintf("%s: %d\n", __func__, __LINE__);
#endif
    /* Unmask and clear any interrupts */
    if (uio_unmask_irq(dev->bus) < 0)
    {
        eprintf("%s: Unable to unmask RX interrupt, ");
        perror("uio_unmask_irq");
    }
    else
    {
        // loop and clear all interrupts
        while(uio_wait_irq(dev->bus, 10) > 0); // with 10 ms timeout
    }
    /* mask the RX interrupt */
    if (uio_mask_irq(dev->bus) < 0)
    {
        eprintf("%s: Unable to mask RX interrupt, ");
        perror("uio_mask_irq");
    }
    return 1;
}

static void *rx_irq_thread(void *__dev)
{
    int num_frames = 0, retcode = 1;
    rxmodem *dev = (rxmodem *)__dev;
    rxmodem_reset(dev, dev->conf);
    // clear memory for rx
    memset(dev->dma->mem_virt_addr, 0x0, dev->dma->mem_sz);
    // Clear FIFO contents in the beginning by toggling the RST pin
    gpioWrite(RX_FIFO_RST, GPIO_HIGH);
    usleep(100000); // 100 ms to clear FIFO
    gpioWrite(RX_FIFO_RST, GPIO_LOW);
    // set up for the first interrupt
    if ((dev->retcode = rxmodem_start(dev)) < 0)
    {
        pthread_cond_signal(&rx_rcv);
        goto rx_irq_thread_exit;
    }
    ssize_t ofst = 0;
    uint32_t frame_sz = 0;
    int num_irq_timeout = 0;
    int frame_num = 0;
#ifdef RXDEBUG
    static int loop_id = 0;
#endif
    while (!(dev->rx_done))
    {
        if ((dev->retcode = uio_unmask_irq(dev->bus)) < 0)
        {
            pthread_cond_signal(&rx_rcv);
            goto rx_irq_thread_exit;
        }
        if ((dev->retcode = uio_wait_irq(dev->bus, RXMODEM_TIMEOUT)) < 0)
        {
            pthread_cond_signal(&rx_rcv);
            goto rx_irq_thread_exit;
        }
        else if (dev->retcode == 0)
        {
            num_irq_timeout++;
            if (num_irq_timeout < NUM_IRQ_RETRIES)
                continue;
            else
                goto rx_irq_thread_exit;
        }
        num_irq_timeout = 0;
        uio_read(dev->bus, RXMODEM_PAYLOAD_LEN, &frame_sz);
#ifdef RXDEBUG
        eprintf("%s: Payload length: %u\n", __func__, frame_sz);
#endif
        if (frame_sz == 0 || frame_sz == 0x1ffc)
        {
            pthread_cond_signal(&rx_rcv);
            eprintf("%s: Received invalid frame size %u\n", __func__, frame_sz);
            dev->retcode = RX_FRAME_INVALID;
            goto rx_irq_thread_exit;
        }
        if (frame_num == 0)
            dev->frame_ofst = (ssize_t *)malloc(sizeof(ssize_t));
        else
        {
            eprintf("%s: Realloc: Source %p, size = %u | ", __func__, dev->frame_ofst, frame_num);
            dev->frame_ofst = (ssize_t *)realloc(dev->frame_ofst, sizeof(ssize_t) * (frame_num + 1));
            eprintf("Dest: %p, size = %u\n", dev->frame_ofst, frame_num + 1);
        }
#ifdef RXDEBUG
        eprintf("%s: %d\n", __func__, __LINE__);
#endif
        if (dev->frame_ofst == NULL)
        {
            eprintf("%s: Frame offset array is NULL\n", __func__);
            dev->retcode = RX_MALLOC_FAILED;
            pthread_cond_signal(&rx_rcv);
            goto rx_irq_thread_exit;
        }
        (dev->frame_ofst)[frame_num] = ofst;
#ifdef RXDEBUG
        eprintf("%s: %d\n", __func__, __LINE__);
#endif
        (frame_num)++;
#ifdef RXDEBUG
        eprintf("%s: Frame number: %d, loop ID: %d\n", __func__, frame_num, loop_id++);
#endif
        dev->retcode = adidma_read(dev->dma, ofst, frame_sz + frame_num - 1 + sizeof(uint32_t));
#ifdef RXDEBUG
        eprintf("%s: %d\n", __func__, __LINE__);
        fprint_frame_hdr(stdout, dev->dma->mem_virt_addr + ofst);
        // char fnamebuf[256];
        // snprintf(fnamebuf, 256, "out%d.txt", loop_id++);
        // FILE *fp = fopen(fnamebuf, "wb");
        // fwrite(dev->dma->mem_virt_addr + ofst, 1, frame_sz, fp);
        // fclose(fp);
#endif
        pthread_mutex_lock(&(rx_write));
        dev->frame_num = frame_num;
        pthread_mutex_unlock(&(rx_write));
        pthread_cond_signal(&rx_rcv);
        ofst += frame_sz - (FRAME_PADDING) * sizeof(uint64_t);
    }
rx_irq_thread_exit:
#ifdef RXDEBUG
    eprintf("%s: %d\n", __func__, __LINE__);
#endif
    rxmodem_stop(dev);
    dev->retcode = retcode;
    return (void *)&retcode;
}

int rxmodem_start(rxmodem *dev)
{
    int ret = 0;
    if ((ret = uio_unmask_irq(dev->bus)) < 0)
    {
        return ret;
    }
    uio_write(dev->bus, RXMODEM_RX_ENABLE, 0x1);
    return 1;
}

int rxmodem_stop(rxmodem *dev)
{
    int ret = 0;
    if ((ret = uio_mask_irq(dev->bus)) < 0)
    {
        return ret;
    }
    uio_write(dev->bus, RXMODEM_RX_ENABLE, 0x0);
    return 1;
}

static inline void getwaittime(struct timespec *ts)
{
    clock_gettime(0, ts);
    printf("%s: Wait_sec: %u, wait_nsec: %u\n", __func__, ts->tv_sec, ts->tv_nsec);
    ts->tv_sec += RXMODEM_TIMEOUT / 1000;
    ts->tv_nsec += (RXMODEM_TIMEOUT % 1000) * 1000000;
}

ssize_t rxmodem_receive(rxmodem *dev)
{
    // Initialize timed wait
    static struct timespec waitts; // = {.tv_sec = RXMODEM_TIMEOUT / 1000, .tv_nsec = (RXMODEM_TIMEOUT % 1000) * 1000000};
    getwaittime(&waitts);
    // printf("Wait_sec: %u, wait_nsec: %u\n", waitts.tv_sec, waitts.tv_nsec);
    // initialize the RX thread
    pthread_attr_t rx_thr_attr[1];
    pthread_attr_init(rx_thr_attr);
    pthread_attr_setdetachstate(rx_thr_attr, PTHREAD_CREATE_DETACHED);
    dev->rx_done = 0;
    int rc = pthread_create(&(dev->thr), rx_thr_attr, &rx_irq_thread, (void *)dev);
    if (rc != 0)
    {
        eprintf("%s: Unable to initialize interrupt monitor thread for TX", __func__);
        perror("pthread_create");
        return RX_THREAD_SPAWN;
    }
    // first wait
#ifdef RXDEBUG
    eprintf("%s: Waiting...\n", __func__);
#endif
    int ret = pthread_cond_timedwait(&rx_rcv, &rx_rcv_m, &waitts);
    if (ret > 0)
        return -ret;
    // pthread_cond_wait(&rx_rcv, &rx_rcv_m);
#ifdef RXDEBUG
    eprintf("%s: Wait over!\n", __func__);
#endif
    // check for retcode
    int retcode = dev->retcode;
    if (retcode <= 0)
    {
        eprintf("%s: Received error %d\n", __func__, retcode);
        dev->rx_done = 1;
        goto rxmodem_receive_end;
    }
    // get total number of frames
    modem_frame_header_t frame_hdr[1];
    memcpy(frame_hdr, dev->dma->mem_virt_addr, sizeof(modem_frame_header_t));
    // check for things
    if (frame_hdr->ident != PACKET_GUID)
    {
        eprintf("%s: Packet GUID does not match: 0x%x\n", __func__, frame_hdr->ident);
        dev->rx_done = 1;
        retcode = RX_INVALID_GUID;
        goto rxmodem_receive_end;
    }
    else if (frame_hdr->pack_sz == 0)
    {
        eprintf("%s: Packet size 0\n", __func__);
        dev->rx_done = 1;
        retcode = RX_PACK_SZ_ZERO;
        goto rxmodem_receive_end;
    }
    else if (frame_hdr->num_frames == 0)
    {
        eprintf("%s: Invalid number of frames!\n", __func__);
        dev->rx_done = 1;
        retcode = RX_NUM_FRAMES_ZERO;
        goto rxmodem_receive_end;
    }
    else if (frame_hdr->frame_sz == 0)
    {
        eprintf("%s: Invalid start frame size!\n", __func__);
        dev->rx_done = 1;
        retcode = RX_FRAME_SZ_ZERO;
        goto rxmodem_receive_end;
    }
    else if (frame_hdr->mtu == 0)
    {
        eprintf("%s: Invalid MTU!\n", __func__);
        dev->rx_done = 1;
        retcode = RX_FRAME_INVALID;
        goto rxmodem_receive_end;
    }
    // everything for the first header is a success!
    int num_frames = frame_hdr->num_frames;
    printf("%s: Number of frames to be received: %d\n", __func__, num_frames);
    // getwaittime(&waitts);
    // pthread_cond_timedwait(&rx_rcv, &rx_rcv_m, &waitts); // wait for return
    // if (dev->retcode < 0)
    //     retcode = dev->retcode;
    int frame_num = 0;
    while (1)
    {
        printf("%s: In loop!\n", __func__);
        fflush(stdout);
        printf("%s: %d\n", __func__, __LINE__);
        fflush(stdout);

        pthread_mutex_lock(&(rx_write));
        memcpy(&frame_num, &(dev->frame_num), sizeof(int));
        pthread_mutex_unlock(&(rx_write));

        printf("%s: Frame number = %d, Number of frames = %d\n", __func__, frame_num, num_frames);
        fflush(stdout);
        if (frame_num >= num_frames)
        {
#ifdef RXDEBUG
            printf("%s: %d >= %d triggered\n", __func__, frame_num, num_frames);
            fflush(stdout);
#endif
            break;
        }
        // wait for wakeup
        getwaittime(&waitts);
        printf("%s: %d\n", __func__, __LINE__);
        fflush(stdout);
        pthread_cond_timedwait(&rx_rcv, &rx_rcv_m, &waitts);
        printf("%s: %d\n", __func__, __LINE__);
        fflush(stdout);
#ifdef RXDEBUG
        eprintf("%s: Current frame number: %d\n", __func__, frame_num);
        fflush(stderr);
#endif
        // check retcode
        if (dev->retcode <= 0) // timed out
        {
            eprintf("%s: Device return code %d!\n", __func__, dev->retcode);
            dev->rx_done = 1;
            break;
        }
    }
    retcode = frame_hdr->pack_sz; // on success or timeout, send the proper size
rxmodem_receive_end:
    // pthread_join(dev->thr, NULL);
    return retcode;
}

ssize_t rxmodem_read(rxmodem *dev, void *buf, ssize_t size)
{
    ssize_t valid_read = 0, total_read = 0;
    // for each frame
    for (int i = 0; i < dev->frame_num; i++)
    {
        modem_frame_header_t frame_hdr[1]; // frame header
        ssize_t ofst = (dev->frame_ofst)[i];
#ifdef RXDEBUG
        printf("%s: Offset %d = %ld\n", __func__, i, ofst);
#endif
        // read in frame header
        memcpy(frame_hdr, dev->dma->mem_virt_addr + ofst, sizeof(modem_frame_header_t));
        // copy out data
        memcpy(buf + total_read, dev->dma->mem_virt_addr + ofst + sizeof(modem_frame_header_t), frame_hdr->frame_sz);
        // check CRC
        if (frame_hdr->frame_crc == frame_hdr->frame_crc2)
        {
            uint16_t crcval = crc16(buf + total_read, frame_hdr->frame_sz);
            if (frame_hdr->frame_crc == crcval)
                valid_read += frame_hdr->frame_sz;
            else
            {
                eprintf("%s: Valid CRC = 0x%x, Calculated CRC = 0x%x\n", __func__, frame_hdr->frame_crc, crcval);
            }
        }
        else
        {
            eprintf("%s: CRC invalid in frame header\n", __func__);
        }
        total_read += frame_hdr->frame_sz;
    }
    free(dev->frame_ofst);
    dev->frame_num = 0;
    return valid_read;
}

int rxmodem_reset(rxmodem *dev, rxmodem_conf_t *conf)
{
    uio_write(dev, RXMODEM_RESET, 0x1);
#ifdef RXDEBUG
    eprintf("%s: %d\n", __func__, __LINE__);
#endif
    usleep(10000);
    uio_write(dev, RXMODEM_RX_ENABLE, 0x0);
#ifdef RXDEBUG
    eprintf("%s: %d\n", __func__, __LINE__);
#endif
    usleep(10000);
    uio_write(dev, RXMODEM_FR_LOOP_BW, 40);
#ifdef RXDEBUG
    eprintf("%s: %d\n", __func__, __LINE__);
#endif
    usleep(10000);
    uio_write(dev, RXMODEM_EQ_MU, 200);
#ifdef RXDEBUG
    eprintf("%s: %d\n", __func__, __LINE__);
#endif
    usleep(10000);
    uio_write(dev, RXMODEM_BYPASS_EQ, 0);
#ifdef RXDEBUG
    eprintf("%s: %d\n", __func__, __LINE__);
#endif
    usleep(10000);
    uio_write(dev, RXMODEM_BYPASS_CODING, 0);
#ifdef RXDEBUG
    eprintf("%s: %d\n", __func__, __LINE__);
#endif
    usleep(10000);
    uio_write(dev, RXMODEM_PD_THRESHOLD, 10);
#ifdef RXDEBUG
    eprintf("%s: %d\n", __func__, __LINE__);
#endif
    return 1;
}

#ifdef RX_UNIT_TEST
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <time.h>
int main(int argc, char *argv[])
{
    printf("Starting program...\n");
    printf("Creating message\n");
    rxmodem dev[1];
    if (rxmodem_init(dev, 0, 2) < 0)
        return -1;
    rxmodem_reset(dev, dev->conf);
    ssize_t rcv_sz = rxmodem_receive(dev);
    if (rcv_sz < 0)
    {
        eprintf("%s: Receive size = %ld\n", __func__, rcv_sz);
        return -1;
    }
    printf("%s: Received data size: %d\n", __func__, rcv_sz);
    fflush(stdout);
    char *buf = (char *)malloc(rcv_sz);
    ssize_t rd_sz = rxmodem_read(dev, buf, rcv_sz);
    if (rcv_sz != rd_sz)
    {
        eprintf("%s: Read size = %ld out of %ld\n", __func__, rd_sz, rcv_sz);
    }
    sleep(1);
    printf("Message:");
    for (int i = 0; i < rd_sz; i++)
        printf("%c", buf[i]);
    FILE *fp = fopen("out_rx.txt", "wb");
    for (int i = 0; i < rd_sz; i++)
    {
        fprintf(fp, "%c", ((char *)dev->dma->mem_virt_addr)[i]);
    }
    fclose(fp);
    printf("\n");
    free(buf);
    return 0;
}
#endif
