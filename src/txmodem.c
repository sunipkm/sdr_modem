/**
 * @file txmodem.c
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
#include "txmodem.h"
#include "txrx_packdef.h"
#include <string.h>
#include <unistd.h>

int txmodem_init(txmodem *dev, int txmodem_id, int txdma_id)
{
    if (dev == NULL)
        return -1;
#ifdef TXMODEM_DEBUG
    eprintf();
#endif
    if (dev->bus == NULL)
        return -1;
#ifdef TXMODEM_DEBUG
    eprintf();
#endif
    if (uio_init(dev->bus, txmodem_id) < 0)
        return -1;
#ifdef TXMODEM_DEBUG
    eprintf();
#endif
    if (dev->dma == NULL)
        return -1;
#ifdef TXMODEM_DEBUG
    eprintf();
#endif
    if (adidma_init(dev->dma, txdma_id, 0) < 0)
        return -1;
    dev->dma->tx_check_completion = 0;
#ifdef TXMODEM_DEBUG
    eprintf();
#endif
    dev->max_pack_sz = dev->dma->mem_sz;
    return 1;
}

int txmodem_reset(txmodem *dev, int src_sel)
{
    uio_write(dev->bus, TXMODEM_RESET, 0x1);
    uio_write(dev->bus, TXMODEM_SRC_SEL, src_sel);
    return 1;
}

int txmodem_write(txmodem *dev, uint8_t *buf, ssize_t size)
{
    static uint32_t pack_id = 0;
    if (size < 0)
    {
        eprintf("Data buffer size less than 0: %d\n", size);
        return -1;
    }
    if (dev->mtu == 0 || dev->mtu < TXRX_MTU_MIN || dev->mtu > TXRX_MTU_MAX) // MTU not set or too small or too large, revert to default
    {
        // dev->mtu = 4072 - sizeof(modem_frame_header_t);
        dev->mtu = DEFAULT_FRAME_SZ - sizeof(modem_frame_header_t) - FRAME_PADDING * sizeof(uint64_t); // padding
    }
    // MODEM_BYTE_ALIGN-byte align MTU
    if (dev->mtu % MODEM_BYTE_ALIGN)
        dev->mtu = (dev->mtu / MODEM_BYTE_ALIGN) * MODEM_BYTE_ALIGN;
#ifdef TXDEBUG
    eprintf("MTU: %u\n", dev->mtu);
#endif
    // check how many frames possible at this MTU
    ssize_t max_frame_sz = dev->mtu + sizeof(modem_frame_header_t) + ((FRAME_PADDING + 1) * sizeof(uint64_t)); // mtu + frame header + padding + frame length for TX make up one frame in mem
    int max_num_frames = (dev->max_pack_sz) / (max_frame_sz);
    int num_frames = (size / dev->mtu) + ((size % dev->mtu) > 0);
    if (num_frames * max_frame_sz >= dev->max_pack_sz)
    {
        eprintf("Total required size exceeds buffer memory size", __func__);
        return -1;
    }
#ifdef TXDEBUG
    eprintf("Max Frames: %d | Frames: %d | Size: %ld\n", max_num_frames, num_frames, size);
#endif
    pack_id++; // increment packet ID on each call
    ssize_t frame_ofst = 0;
    ssize_t data_ofst = 0;
    for (int i = 0; i < num_frames; i++) // for each frame
    {
        /* Create header */
        modem_frame_header_t frame_hdr[1];
        frame_hdr->ident = PACKET_GUID;
        frame_hdr->pack_id = pack_id;
        frame_hdr->pack_sz = size;
        frame_hdr->frame_id = i;
        frame_hdr->num_frames = num_frames;
        frame_hdr->mtu = dev->mtu;
        frame_hdr->frame_sz = dev->mtu < (size - data_ofst) ? dev->mtu : size - data_ofst; // data of frame
        frame_hdr->frame_crc = crc16(buf + data_ofst, frame_hdr->frame_sz);                // crc of frame
        frame_hdr->frame_crc2 = frame_hdr->frame_crc;                                      // copy of crc
        /* Calculate frame padding */
        size_t frame_padding = (frame_hdr->frame_sz) % sizeof(uint64_t);            // calculate how many bytes we are off by
        frame_padding = (frame_padding > 0) ? sizeof(uint64_t) - frame_padding : 0; // calculate proper padding
#ifdef TXDEBUG
        if (frame_padding > 0)
            eprintf("Frame padding = %u\n", frame_padding);
#endif
        /* TX IP Core Frame Size */
        uint64_t dma_frame_sz = frame_hdr->frame_sz + frame_padding + sizeof(modem_frame_header_t) + FRAME_PADDING * sizeof(uint64_t);
        // dma_frame_sz += dma_frame_sz % MODEM_BYTE_ALIGN ? MODEM_BYTE_ALIGN - (dma_frame_sz % MODEM_BYTE_ALIGN) : 0; // 4-bytes aligned, will pad DMA buffer with extra zeros at the end if necessary

        /* Copy frame size (used by the TX IP Core) */
        memcpy(dev->dma->mem_virt_addr + frame_ofst, &(dma_frame_sz), sizeof(uint64_t));
        frame_ofst += sizeof(uint64_t);
#ifdef TXDEBUG
        eprintf("Loop %d | Frame sz: %u, Frame ofst: %d, data ofst: %d, wrote frame sz\n", i, frame_hdr->frame_sz, frame_ofst, data_ofst);
#endif

        /* Copy frame header */
        memcpy(dev->dma->mem_virt_addr + frame_ofst, &(frame_hdr), sizeof(modem_frame_header_t)); // copy frame header
        frame_ofst += sizeof(modem_frame_header_t);
#ifdef TXDEBUG
        eprintf("Loop %d | Frame sz: %u, Frame ofst: %d, data ofst: %d, wrote frame hdr\n", i, frame_hdr->frame_sz, frame_ofst, data_ofst);
#endif

        /* Copy frame data */
        memcpy(dev->dma->mem_virt_addr + frame_ofst, buf + data_ofst, frame_hdr->frame_sz); // copy data
        /* Fix frame offset for DMA */
        frame_ofst += frame_hdr->frame_sz;

        /* Padding Frame */
        memset(dev->dma->mem_virt_addr + frame_ofst, 0x0, frame_padding);
        frame_ofst += frame_padding;

        /* Padding FRAME_PADDING * 8 bytes */
        memset(dev->dma->mem_virt_addr + frame_ofst, 0x0, FRAME_PADDING * sizeof(uint64_t));
        frame_ofst += FRAME_PADDING * sizeof(uint64_t);

        /* Data offset */
        data_ofst += frame_hdr->frame_sz;
#ifdef TXDEBUG
        eprintf("Loop %d | Frame sz: %u, Frame ofst: %d, data ofst: %d, CRC: 0x%04x, wrote frame data\n", i, frame_hdr->frame_sz, frame_ofst, data_ofst, frame_hdr->frame_crc);
#endif
        if (num_frames > 5)
        {
            frame_ofst = 0;
            adidma_write(dev->dma, 0x0, dma_frame_sz + sizeof(uint64_t), 0);
            if (((i % 4) == 0) && (i > 0))
                usleep(1000);
        }
    }
#ifdef TXDEBUG
    FILE *fp = fopen("out_tx.txt", "wb");
    fwrite(dev->dma->mem_virt_addr, 0x1, frame_ofst, fp);
    fclose(fp);
#endif
    if (num_frames <= 5)
        return adidma_write(dev->dma, 0x0, frame_ofst, 0);
    else // create back pressure
    {
        memset(dev->dma->mem_virt_addr, 0x0, max_frame_sz);
        max_frame_sz -= sizeof(uint64_t);
        memcpy(dev->dma->mem_virt_addr, &max_frame_sz, sizeof(uint64_t));
        return adidma_write(dev->dma, 0x0, max_frame_sz + sizeof(uint64_t), 0);
    }
    return 0;
}

void txmodem_destroy(txmodem *dev)
{
    adidma_destroy(dev->dma);
    uio_destroy(dev->bus);
}
