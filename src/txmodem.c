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

#define eprintf(...)              \
    fprintf(stderr, __VA_ARGS__); \
    fflush(stderr);

int txmodem_init(txmodem *dev, int txmodem_id, int txdma_id)
{
    if (dev == NULL)
        return -1;
#ifdef TXMODEM_DEBUG
    eprintf("%s: %d\n", __func__, __LINE__);
#endif
    if (dev->bus == NULL)
        return -1;
#ifdef TXMODEM_DEBUG
    eprintf("%s: %d\n", __func__, __LINE__);
#endif
    if (uio_init(dev->bus, txmodem_id) < 0)
        return -1;
#ifdef TXMODEM_DEBUG
    eprintf("%s: %d\n", __func__, __LINE__);
#endif
    if (dev->dma == NULL)
        return -1;
#ifdef TXMODEM_DEBUG
    eprintf("%s: %d\n", __func__, __LINE__);
#endif
    if (adidma_init(dev->dma, txdma_id, 0) < 0)
        return -1;
    dev->dma->tx_check_completion = 0;
#ifdef TXMODEM_DEBUG
    eprintf("%s: %d\n", __func__, __LINE__);
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
        eprintf("%s: Data buffer size less than 0: %d\n", __func__, size);
        return -1;
    }
    if (dev->mtu == 0 || dev->mtu < TXRX_MTU_MIN) // MTU not set or too small, revert to default
    {
        // dev->mtu = 4072 - sizeof(modem_frame_header_t);
        dev->mtu = DEFAULT_FRAME_SZ - sizeof(modem_frame_header_t) - FRAME_PADDING * sizeof(uint64_t); // padding
    }
    // MODEM_BYTE_ALIGN-byte align MTU
    if (dev->mtu % MODEM_BYTE_ALIGN)
        dev->mtu = (dev->mtu / MODEM_BYTE_ALIGN) * MODEM_BYTE_ALIGN;
#ifdef TXDEBUG
    eprintf("%s: MTU: %u\n", __func__, dev->mtu);
#endif
    // check how many frames possible at this MTU
    ssize_t max_frame_sz = dev->mtu + sizeof(modem_frame_header_t) + ((FRAME_PADDING + 1) * sizeof(uint64_t)); // mtu + frame header + padding + frame length for TX make up one frame in mem
    int max_num_frames = (dev->max_pack_sz) / (max_frame_sz);
    int num_frames = (size / dev->mtu) + ((size % dev->mtu) > 0);
#ifdef TXDEBUG
    eprintf("%s: Max Frames: %d | Frames: %d | Size: %ld\n", __func__, max_num_frames, num_frames, size);
#endif
    if (num_frames > max_num_frames)
    {
        eprintf("%s: Data size exceeds maximum allowed packet size, returning...\n", __func__);
        return -1;
    }
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
            printf("%s: Frame padding = %u\n", __func__, frame_padding);
#endif
        /* TX IP Core Frame Size */
        uint64_t dma_frame_sz = frame_hdr->frame_sz + frame_padding + sizeof(modem_frame_header_t) + FRAME_PADDING * sizeof(uint64_t);
        // dma_frame_sz += dma_frame_sz % MODEM_BYTE_ALIGN ? MODEM_BYTE_ALIGN - (dma_frame_sz % MODEM_BYTE_ALIGN) : 0; // 4-bytes aligned, will pad DMA buffer with extra zeros at the end if necessary

        /* Copy frame size (used by the TX IP Core) */
        memcpy(dev->dma->mem_virt_addr + frame_ofst, &(dma_frame_sz), sizeof(uint64_t));
        frame_ofst += sizeof(uint64_t);
#ifdef TXDEBUG
        eprintf("%s: Loop %d | Frame sz: %u, Frame ofst: %d, data ofst: %d, wrote frame sz\n", __func__, i, frame_hdr->frame_sz, frame_ofst, data_ofst);
#endif

        /* Copy frame header */
        memcpy(dev->dma->mem_virt_addr + frame_ofst, &(frame_hdr), sizeof(modem_frame_header_t)); // copy frame header
        frame_ofst += sizeof(modem_frame_header_t);
#ifdef TXDEBUG
        eprintf("%s: Loop %d | Frame sz: %u, Frame ofst: %d, data ofst: %d, wrote frame hdr\n", __func__, i, frame_hdr->frame_sz, frame_ofst, data_ofst);
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
        eprintf("%s: Loop %d | Frame sz: %u, Frame ofst: %d, data ofst: %d, CRC: 0x%04x, wrote frame data\n", __func__, i, frame_hdr->frame_sz, frame_ofst, data_ofst, frame_hdr->frame_crc);
#endif
#ifdef TX_EVERY_FRAME
        frame_ofst = 0;
        adidma_write(dev->dma, 0x0, dma_frame_sz + sizeof(uint64_t), 0);
#endif
    }
#ifndef TX_EVERY_FRAME
#ifdef TXDEBUG
    FILE *fp = fopen("out_tx.txt", "wb");
    fwrite(dev->dma->mem_virt_addr, 0x1, frame_ofst, fp);
    fclose(fp);
#endif
    return adidma_write(dev->dma, 0x0, frame_ofst, 0);
#else
    return 0;
#endif
}

void txmodem_destroy(txmodem *dev)
{
    adidma_destroy(dev->dma);
    uio_destroy(dev->bus);
}

#ifdef TX_UNIT_TEST
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <time.h>
int main(int argc, char *argv[])
{
    printf("Starting program...\n");
    printf("Creating message\n");
    time_t rawtime;
    struct tm *timeinfo;
    time(&rawtime);
    timeinfo = localtime(&rawtime);
    char msg[(1 << 16) - 1];
    const char fmt_str[] = "\n\nTesting SPACE HAUC SDR.\n"
                           "The date is: %04d-%02d-%02d, time is: %02d:%02d:%02d\n"
                           "Had I the heavens' embroidered cloths,\n"
                           "Enwrought with golden and silver light,\n"
                           "The blue and the dim and the dark cloths\n"
                           "Of night and light and the half light,\n"
                           "I would spread the cloths under your feet:\n"
                           "But I, being poor, have only my dreams;\n"
                           "I have spread my dreams under your feet;\n"
                           "Tread softly because you tread on my dreams.\n"
                           "\t--William Butler Yeats\n\n"

                           "Easily let's get carried away\n"
                           "Easily let's get married today\n"
                           "\n"
                           "Shao Lin shouted a rose\n"
                           "From his throat\n"
                           "Everything must go\n"
                           "\n"
                           "A lickin' stick is thicker\n"
                           "When you break it to show\n"
                           "Everything must go\n"
                           "\n"
                           "The story of a woman on the morning of a war\n"
                           "Remind me if you will exactly what we're fighting for\n"
                           "Calling calling for something in the air\n"
                           "Calling calling I know you must be there\n"
                           "\n"
                           "Easily let's get caught in a wave\n"
                           "Easily we won't get caught in a cage\n"
                           "\n"
                           "Shao Lin shakin' for the sake\n"
                           "Of his soul \n"
                           "Everything must go\n"
                           "\n"
                           "Lookin' mighty tired of\n"
                           "All the things that you own\n"
                           "Everything must go\n"
                           "\n"
                           "I can't tell you who to idolize\n"
                           "You think it's almost over\n"
                           "But it's only on the rise\n"
                           "\n"
                           "Calling calling for something in the air\n"
                           "Calling calling I know you must be there\n"
                           "\n"
                           "The story of a woman on the morning of a war\n"
                           "Remind me if you will exactly what we're fighting for\n"
                           "\n"
                           "Throw me to the wolves\n"
                           "Because there's order in the pack\n"
                           "Throw me to the sky\n"
                           "Because I know I'm coming back\n"
                           "\n"
                           "Shao Lin shakin' for the sake\n"
                           "Of his soul \n"
                           "Everything must go\n"
                           "Lookin' mighty tired of\n"
                           "All the things that you own\n"
                           "Everything must go\n"
                           "\n"
                           "The story of a woman on the morning of a war\n"
                           "Remind me if you will exactly what we're fighting for\n"
                           "\n"
                           "Calling calling for something in the air\n"
                           "Calling calling I know you must be there\n"
                           "\n"
                           "I don't want to be your little research monkey boy\n"
                           "The creature that I am is only going to destroy\n"
                           "Throw me to the wolves\n"
                           "Because there's order in the pack\n"
                           "Throw me to the sky\n"
                           "Because I know I'm coming back"
                           "\n\n\n"
                           "Getting born in the state of Mississippi\n"
                           "Papa was a copper, and her mama was a hippy\n"
                           "In Alabama she would swing a hammer\n"
                           "Price you got to pay when you break the panorama\n"
                           "She never knew that there was anything more than poor\n"
                           "What in the world does your company take me for?\n"
                           "\n"
                           "Black bandanna, sweet Louisiana\n"
                           "Robbing on a bank in the state of Indiana\n"
                           "She's a runner\n"
                           "Rebel, and a stunner\n"
                           "On her merry way saying baby, watcha gonna?\n"
                           "Looking down the barrel of a hot metal forty-five\n"
                           "Just another way to survive\n"
                           "\n"
                           "California, rest in peace\n"
                           "Simultaneous release\n"
                           "California, show your teeth\n"
                           "She's my priestess\n"
                           "I'm your priest\n"
                           "Yeah, yeah, yeah\n"
                           "\n"
                           "She's a lover, baby, and a fighter\n"
                           "Should've seen it coming when I got a little brighter\n"
                           "With a name like Dani California\n"
                           "Day was gonna come when I was gonna mourn ya\n"
                           "A little loaded, she was stealing another breath\n"
                           "I love my baby to death\n"
                           "\n"
                           "California, rest in peace\n"
                           "Simultaneous release\n"
                           "California, show your teeth\n"
                           "She's my priestess\n"
                           "I'm your priest\n"
                           "Yeah, yeah, yeah\n"
                           "\n"
                           "Who knew the other side of you?\n"
                           "Who knew that others died to prove?\n"
                           "Too true to say goodbye to you\n"
                           "Too true to say, say, say\n"
                           "\n"
                           "Pushed the fader, gifted animator\n"
                           "One for the now, and eleven for the later\n"
                           "Never made it up to Minnesota\n"
                           "North Dakota man\n"
                           "Wasn't gunnin' for the quota\n"
                           "Down in the Badlands she was saving the best for last\n"
                           "It only hurts when I laugh\n"
                           "Gone too fast\n"
                           "\n"
                           "California, rest in peace\n"
                           "Simultaneous release\n"
                           "California, show your teeth\n"
                           "She's my priestess\n"
                           "I'm your priest\n"
                           "Yeah, yeah, yeah\n"
                           "\n"
                           "California, rest in peace\n"
                           "Simultaneous release\n"
                           "California, show your teeth\n"
                           "She's my priestess\n"
                           "I'm your priest\n"
                           "Yeah, yeah, yeah"
                           "\n\n\n"
                           "Evening rises, darkness threatens to engulf us all\n"
                           "But there's a moon above it's shining and I think I hear a call\n"
                           "It's just a whisper through the trees, my ears can hardly make it out\n"
                           "But I can hear it in my heart, vibrating strong as if she shouts\n"
                           "\n";

        static char msg2[] = "Oh Ariadne, I am coming, I just need to work this maze inside my head\n"
                             "I came here like you asked, I killed the beast, that part of me is dead\n"
                             "Oh Ariadne, I just need to work this maze inside my head\n"
                             "If only I'd have listened to you when you offered me that thread\n"
                             "\n"
                             "Everything is quiet and I'm not exactly sure\n"
                             "If it really was your voice I heard or maybe it is a door\n"
                             "That's closing up some hero's back on his track to be a man\n"
                             "Can it be that all us heroes have a path, but not a plan?\n"
                             "\n"
                             "Oh Ariadne, I am coming, I just need to work this maze inside my mind\n"
                             "I wish I had that string, it's so damn dark, I think I'm going blind\n"
                             "Oh Ariadne, I just need to work this maze inside my mind\n"
                             "For the life of me, I don't remember what I came to find\n"
                             "\n"
                             "Now tell me, princess, are you strolling through your sacred grove?\n"
                             "And is the moon still shining? You're the only thing I'm thinking of\n"
                             "The sword you gave me, it was heavy, I just had to lay it down\n"
                             "It's funny how defenseless I can feel here when there's nobody around\n"
                             "\n"
                             "Oh Ariadne, I am coming, I just need to work this maze inside my heart\n"
                             "I was blind, I thought you'd bind me, but you offered me a chart\n"
                             "Oh Ariadne, I just need to work this maze inside my heart\n"
                             "If I'd known that you could guide me, I'd have listened from the start\n"
                             "\n"
                             "Somewhere up there midnight strikes, I think I hear the fall\n"
                             "Of little drops of water, magnified against the barren wall\n"
                             "It's more a feeling than a substance, but there's nobody around\n"
                             "And when I'm in here all alone, it's just enough to let me drown\n"
                             "\n"
                             "Oh Ariadne, I was coming, but I failed you in this labyrinth of my past\n"
                             "Oh Ariadne, let me sing you and we'll make each other last\n"
                             "Oh Ariadne, I have failed you in this labyrinth of my past\n"
                             "Oh Ariadne, let me sing you and we'll make each other last"
                             "\n\n\n"
                             "Road trippin' with my two favorite allies\n"
                             "Fully loaded we got snacks and supplies\n"
                             "It's time to leave this town it's time to steal away\n"
                             "Let's go get lost anywhere in the U.S.A.\n"
                             "Let's go get lost, let's go get lost\n"
                             "Blue you sit so pretty West of the one\n"
                             "Sparkles light with yellow icing, just a mirror for the sun\n"
                             "Just a mirror for the sun\n"
                             "Just a mirror for the sun\n"
                             "These smiling eyes are just a mirror for\n"
                             "So much as come before those battles lost and won\n"
                             "This life is shining more forever in the sun\n"
                             "Now let us check our heads and let us check the surf\n"
                             "Staying high and dry's More trouble than it's worth in the sun\n"
                             "Just a mirror for the sun\n"
                             "Just a mirror for the sun\n"
                             "These smiling eyes are just a mirror for\n"
                             "In Big Sur we take some time to linger on\n"
                             "We three hunky dory's got our snake finger on\n"
                             "Now let us drink the stars it's time to steal away\n"
                             "Let's go get lost right here in the U.S.A\n"
                             "Let's go get lost, let's go get lost\n"
                             "Blue you sit so pretty west of the one\n"
                             "Sparkles light with yellow icing, just a mirror for the sun\n"
                             "Just a mirror for the sun\n"
                             "Just a mirror for the sun\n"
                             "These smiling eyes are just a mirror for\n"
                             "These smiling eyes are just a mirror for\n"
                             "Your smiling eyes are just a mirror for"
                             "\n\n\n"
                             "End of transmission";

    ssize_t size = snprintf(msg, (1 << 16) - 1, fmt_str, timeinfo->tm_year + 1900, timeinfo->tm_mon + 1, timeinfo->tm_mday, timeinfo->tm_hour, timeinfo->tm_min, timeinfo->tm_sec);
    memcpy(msg + size, msg2, strlen(msg2));
    size += strlen(msg2);
    txmodem dev[1];
    if (txmodem_init(dev, uio_get_id("tx_ipcore"), uio_get_id("tx_dma")) < 0)
        return -1;
    txmodem_reset(dev, 0);
    printf("Enter MTU to transmit, enter . to continue with default MTU: ");
    char buf[10];
    scanf(" %s", buf);
    dev->mtu = strtol(buf, 0x0, 10);
    txmodem_write(dev, (uint8_t *)msg, size);
    return 0;
}
#endif
