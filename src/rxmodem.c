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
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#define _POSIX_C_SOURCE 199309L
#include <time.h>
#include <errno.h>

#define likely(x) __builtin_expect(!!(x), 1)
#define unlikely(x) __builtin_expect(!!(x), 0)

static void *rx_irq_thread(void *__dev);
static pthread_cond_t rx_rcv;
static pthread_mutex_t rx_rcv_m;
static pthread_mutex_t rx_write;
static pthread_mutex_t frame_ofst_m;
static pthread_mutex_t rx_irq_thread_running;

#define RX_FIFO_RST "960"
#define RX_FIFO_RST_TOUT 100000 // us

static int rxmodem_fifo_rst()
{
    FILE *fp;
    ssize_t size;
    fp = fopen("/sys/class/gpio/export", "w");
    if (fp == NULL)
    {
        eprintf("Error opening");
        perror("gpioexport: ");
        goto exitfunc;
    }
    size = fprintf(fp, "%s", RX_FIFO_RST);
    if (size <= 0)
    {
        eprintf("Error writing to ");
        perror("gpioexport: ");
        goto closefile;
    }
    fclose(fp);
    usleep(10000);
    fp = fopen("/sys/class/gpio/gpio" RX_FIFO_RST "/direction", "w");
    if (fp == NULL)
    {
        eprintf("Error opening ");
        perror("gpiodirection: ");
        goto exitfunc;
    }
    size = fprintf(fp, "out");
    if (size <= 0)
    {
        eprintf("Error writing to ");
        perror("gpiodirection: ");
        goto closefile;
    }
    fclose(fp);
    usleep(10000);
    fp = fopen("/sys/class/gpio/gpio" RX_FIFO_RST "/value", "w");
    if (fp == NULL)
    {
        eprintf("Error opening ");
        perror("gpiovalue: ");
        goto exitfunc;
    }
    size = fprintf(fp, "1");
    if (size <= 0)
    {
        eprintf("Error writing to ");
        perror("gpiovalue: ");
        goto closefile;
    }
    fclose(fp);
    usleep(RX_FIFO_RST_TOUT);
    fp = fopen("/sys/class/gpio/gpio" RX_FIFO_RST "/value", "w");
    if (fp == NULL)
    {
        eprintf("Error opening ");
        perror("gpiovalue: ");
        goto exitfunc;
    }
    size = fprintf(fp, "0");
    if (size <= 0)
    {
        eprintf("Error writing to ");
        perror("gpiovalue: ");
        goto closefile;
    }
    fclose(fp);
    return EXIT_SUCCESS;
closefile:
    fclose(fp);
exitfunc:
    return EXIT_FAILURE;
}

int rxmodem_init(rxmodem *dev, int rxmodem_id, int rxdma_id)
{
    if (dev == NULL)
        return -1;
    pthread_mutexattr_t attr;
    pthread_mutexattr_init(&attr);
    pthread_mutex_init(&rx_irq_thread_running, &attr);
    pthread_mutexattr_destroy(&attr);
#ifdef RXDEBUG
    eprintf();
#endif
    // dev->bus = (uio_dev *)malloc(sizeof(uio_dev));
    if (dev->bus == NULL)
        return -1;
#ifdef RXDEBUG
    eprintf();
#endif
    if (uio_init(dev->bus, rxmodem_id) < 0)
        return -1;
#ifdef RXDEBUG
    eprintf();
#endif
    // dev->dma = (adidma *)malloc(sizeof(adidma));
    if (dev->dma == NULL)
        return -1;
#ifdef RXDEBUG
    eprintf();
#endif
    if (adidma_init(dev->dma, rxdma_id, 0) < 0)
        return -1;
#ifdef RXDEBUG
    eprintf();
#endif
    /* Save default settings for RX IP */
    dev->conf->fr_loop_bw = 40; /* int8_t, BW = (k + 1)/100 */
    dev->conf->eqmu = 200;
    dev->conf->scopesel = 2;
    dev->conf->pd_threshold = 10;
    dev->conf->eq_bypass = 0;
#ifdef RXDEBUG
    eprintf();
#endif
    /* Unmask and clear any interrupts */
    if (uio_unmask_irq(dev->bus) < 0)
    {
        eprintf("Unable to unmask RX interrupt");
        perror("uio_unmask_irq");
    }
    else
    {
        // loop and clear all interrupts
        while (uio_wait_irq(dev->bus, 10) > 0)
            ; // with 10 ms timeout
    }
    /* mask the RX interrupt */
    if (uio_mask_irq(dev->bus) < 0)
    {
        eprintf("Unable to mask RX interrupt");
        perror("uio_mask_irq");
    }
    dev->frame_ofst = NULL;
    dev->frame_ofst = (ssize_t *)malloc(dev->dma->mem_sz / TXRX_MTU_MIN); // maximum number of frame offsets
    if (dev->frame_ofst == NULL)
    {
        eprintf("Unable to allocate memory for frame offset");
        perror("malloc");
        return -1;
    }
    return 1;
}

static void *rx_irq_thread(void *__dev)
{
    int retcode = 1;
    rxmodem *dev = (rxmodem *)__dev;
    rxmodem_reset(dev, dev->conf);
    // clear memory for rx
    memset(dev->dma->mem_virt_addr, 0x0, dev->dma->mem_sz);
    // Clear FIFO contents in the beginning by toggling the RST pin
    int fifo_rst_count = 0;
    while ((rxmodem_fifo_rst() == EXIT_FAILURE) && (fifo_rst_count < 10))
        fifo_rst_count++;
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
        eprintf("Payload length: %u", frame_sz);
#endif
        if (frame_sz == 0 || frame_sz == 0x1ffc)
        {
            pthread_cond_signal(&rx_rcv);
            eprintf("Received invalid frame size %u", frame_sz);
            dev->retcode = RX_FRAME_INVALID;
            goto rx_irq_thread_exit;
        }
        pthread_mutex_lock(&frame_ofst_m);
        (dev->frame_ofst)[frame_num] = ofst;
        pthread_mutex_unlock(&frame_ofst_m);
#ifdef RXDEBUG
        eprintf();
#endif
        (frame_num)++;
#ifdef RXDEBUG
        eprintf("Frame number: %d, loop ID: %d\n", frame_num, loop_id++);
#endif
        if (dev->rx_done)
            break;
        dev->retcode = adidma_read(dev->dma, ofst, frame_sz + sizeof(uint32_t));
#ifdef RXDEBUG
        eprintf();
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
    eprintf();
#endif
    rxmodem_stop(dev);
    // dev->retcode = retcode;
    return NULL;
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
#ifdef RXDEBUG
    eprintf("Wait_sec: %u, wait_nsec: %u", ts->tv_sec, ts->tv_nsec);
#endif
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
    int rc = pthread_create((dev->thr), rx_thr_attr, &rx_irq_thread, (void *)dev);
    if (rc != 0)
    {
        eprintf("Unable to initialize interrupt monitor thread for TX");
        perror("pthread_create");
        return RX_THREAD_SPAWN;
    }
    // first wait
#ifdef RXDEBUG
    eprintf("Waiting...");
#endif
    int ret = pthread_cond_timedwait(&rx_rcv, &rx_rcv_m, &waitts);
    if (ret > 0)
        return -ret;
        // pthread_cond_wait(&rx_rcv, &rx_rcv_m);
#ifdef RXDEBUG
    eprintf("Wait over!");
#endif
    // check for retcode
    int retcode = dev->retcode;
    if (retcode <= 0)
    {
        eprintf("Received error %d", retcode);
        dev->rx_done = 1;
        goto rxmodem_receive_end;
    }
    // get total number of frames
    modem_frame_header_t frame_hdr[1];
    memcpy(frame_hdr, dev->dma->mem_virt_addr, sizeof(modem_frame_header_t));
    // check for things
    if (frame_hdr->ident != PACKET_GUID)
    {
        eprintf("Packet GUID does not match: 0x%x", frame_hdr->ident);
        dev->rx_done = 1;
        retcode = RX_INVALID_GUID;
        goto rxmodem_receive_end;
    }
    else if (frame_hdr->pack_sz == 0)
    {
        eprintf("Packet size 0");
        dev->rx_done = 1;
        retcode = RX_PACK_SZ_ZERO;
        goto rxmodem_receive_end;
    }
    else if (frame_hdr->num_frames == 0)
    {
        eprintf("Invalid number of frames!");
        dev->rx_done = 1;
        retcode = RX_NUM_FRAMES_ZERO;
        goto rxmodem_receive_end;
    }
    else if (frame_hdr->frame_sz == 0 || frame_hdr->frame_sz > TXRX_MTU_MAX)
    {
        eprintf("Invalid start frame size!");
        dev->rx_done = 1;
        retcode = RX_FRAME_SZ_ZERO;
        goto rxmodem_receive_end;
    }
    else if (frame_hdr->mtu == 0)
    {
        eprintf("Invalid MTU!\n");
        dev->rx_done = 1;
        retcode = RX_FRAME_INVALID;
        goto rxmodem_receive_end;
    }
    // everything for the first header is a success!
    int num_frames = frame_hdr->num_frames;
#ifdef RXDEBUG
    eprintf("Number of frames to be received: %d\n", num_frames);
#endif
    // getwaittime(&waitts);
    // pthread_cond_timedwait(&rx_rcv, &rx_rcv_m, &waitts); // wait for return
    // if (dev->retcode < 0)
    //     retcode = dev->retcode;
    int frame_num = 0;
    while (1)
    {
        pthread_mutex_lock(&(rx_write));
        memcpy(&frame_num, &(dev->frame_num), sizeof(int));
        pthread_mutex_unlock(&(rx_write));
#ifdef RXDEBUG
        eprintf("Frame number = %d, Number of frames = %d\n", frame_num, num_frames);
        fflush(stdout);
#endif
        if (frame_num >= num_frames)
        {
#ifdef RXDEBUG
            eprintf("%d >= %d triggered\n", frame_num, num_frames);
            fflush(stdout);
#endif
            break;
        }
        // wait for wakeup
        getwaittime(&waitts);
        pthread_cond_timedwait(&rx_rcv, &rx_rcv_m, &waitts);
#ifdef RXDEBUG
        eprintf("Current frame number: %d", frame_num);
        fflush(stderr);
#endif
        // check retcode
        if (dev->retcode <= 0) // timed out
        {
            eprintf("Device return code %d!", dev->retcode);
            dev->rx_done = 1;
            break;
        }
    }
    retcode = frame_hdr->pack_sz; // on success or timeout, send the proper size
rxmodem_receive_end:
    dev->rx_done = 1; // indicate completion
    pthread_cancel(dev->thr[0]);
    return retcode;
}

ssize_t rxmodem_read(rxmodem *dev, uint8_t *buf, ssize_t size)
{
    ssize_t valid_read = 0, total_read = 0;
    // for each frame
    for (int i = 0; i < dev->frame_num; i++)
    {
        modem_frame_header_t frame_hdr[1]; // frame header
        pthread_mutex_lock(&frame_ofst_m);
        ssize_t ofst = (dev->frame_ofst)[i];
        pthread_mutex_unlock(&frame_ofst_m);
#ifdef RXDEBUG
        eprintf("%s: Offset %d = %ld", i, ofst);
#endif
        // read in frame header
        memcpy(frame_hdr, dev->dma->mem_virt_addr + ofst, sizeof(modem_frame_header_t));
        // copy out data, perform CRC etc
        if (total_read + frame_hdr->frame_sz <= size) // memcpy valid only when this is true
        {
            memcpy(buf + total_read, dev->dma->mem_virt_addr + ofst + sizeof(modem_frame_header_t), frame_hdr->frame_sz);
            // check CRC
            if (frame_hdr->frame_crc == frame_hdr->frame_crc2)
            {
                uint16_t crcval = crc16(buf + total_read, frame_hdr->frame_sz);
                if (frame_hdr->frame_crc == crcval)
                    valid_read += frame_hdr->frame_sz;
                else
                {
                    eprintf("Loop %d: Valid CRC = 0x%x, Calculated CRC = 0x%x\n", i, frame_hdr->frame_crc, crcval);
                }
            }
            else
            {
                eprintf("Loop %d: CRC invalid in frame header\n", i);
            }
            total_read += frame_hdr->frame_sz;
        }
        else
            break;
    }
    return valid_read;
}

int rxmodem_reset(rxmodem *dev, rxmodem_conf_t *conf)
{
    dev->frame_num = 0;
    uio_write(dev->bus, RXMODEM_RESET, 0x1);
#ifdef RXDEBUG
    eprintf();
#endif
    // usleep(10000);
    uio_write(dev->bus, RXMODEM_RX_ENABLE, 0x0);
#ifdef RXDEBUG
    eprintf();
#endif
    // usleep(10000);
    uio_write(dev->bus, RXMODEM_FR_LOOP_BW, conf->fr_loop_bw);
#ifdef RXDEBUG
    eprintf();
#endif
    // usleep(10000);
    uio_write(dev->bus, RXMODEM_EQ_MU, conf->eqmu);
#ifdef RXDEBUG
    eprintf();
#endif
    // usleep(10000);
    uio_write(dev->bus, RXMODEM_BYPASS_EQ, conf->eq_bypass);
#ifdef RXDEBUG
    eprintf();
#endif
    // usleep(10000);
    uio_write(dev->bus, RXMODEM_BYPASS_CODING, 0);
#ifdef RXDEBUG
    eprintf();
#endif
    // usleep(10000);
    uio_write(dev->bus, RXMODEM_PD_THRESHOLD, conf->pd_threshold);
#ifdef RXDEBUG
    eprintf();
#endif
    uio_write(dev->bus, RXMODEM_EXT_FR_K1, conf->ext_fr_k1);
#ifdef RXDEBUG
    eprintf();
#endif
    uio_write(dev->bus, RXMODEM_EXT_FR_K2, conf->ext_fr_k2);
#ifdef RXDEBUG
    eprintf();
#endif
    uio_write(dev->bus, RXMODEM_EXT_FR_GAIN, conf->ext_fr_gain);
#ifdef RXDEBUG
    eprintf();
#endif
    uio_write(dev->bus, RXMODEM_EXT_FR_EN, conf->ext_fr_en);
#ifdef RXDEBUG
    eprintf();
#endif
    return 1;
}

void rxmodem_destroy(rxmodem *dev)
{
    if (dev->frame_ofst != NULL)
        free(dev->frame_ofst);
    rxmodem_stop(dev);                       // stop the modem for safety
    uio_write(dev->bus, RXMODEM_RESET, 0x1); // reset the modem IP
    rxmodem_fifo_rst();                      // reset the FIFO
    uio_destroy(dev->bus);                   // close the UIO device handle
    adidma_destroy(dev->dma);                // close the DMA engine handle
}

static char *print_bits(uint32_t num, int tot_bits)
{
    static char buf[33];
    memset(buf, 0x0, 33);
    for (int i = 0; tot_bits > 0; i++)
    {
        tot_bits--;
        buf[i] = (num >> tot_bits) & 0x1 ? '1' : '0';
    }
    return buf;
}

int rxmodem_enable_ext_fr(rxmodem *dev, float loop_bw, float damping, float gain)
{
    // do the maths
    double theta = loop_bw/(damping + 0.25 * damping);
    double d = 1 + 2 * damping * theta + theta * theta;

    float k1 = (4 * damping * theta / d);
    float k2 = theta * theta / d;

    dev->conf->ext_fr_k1 = convert_ufixdt(k1, 16, 15);
    dev->conf->ext_fr_k2 = convert_ufixdt(k2, 16, 15);
    dev->conf->ext_fr_gain = convert_sfixdt(gain, 25, 20);
    dev->conf->ext_fr_en = 0x1;

#ifdef RXDEBUG
    eprintf("Loop BW: %f Damping: %f Gain: %f\n"
            "T: %f d: %f\n"
            "K1: %f K2: %f",
            loop_bw, damping, gain,
            theta, d,
            k1, k2);
    eprintf("K1: %s", print_bits(dev->conf->ext_fr_k1, 16));
    eprintf("K2: %s", print_bits(dev->conf->ext_fr_k2, 16));
    eprintf("Gain: %s", print_bits(dev->conf->ext_fr_gain, 16));
#endif

    return rxmodem_reset(dev, dev->conf); 
}

int rxmodem_disable_ext_fr(rxmodem *dev)
{
    dev->conf->ext_fr_en = 0;
    return rxmodem_reset(dev, dev->conf);
}