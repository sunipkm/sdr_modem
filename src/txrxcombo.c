/**
 * @file txrxcombo.c
 * @author Sunip K. Mukherjee (sunipkmukherjee@gmail.com)
 * @brief 
 * @version 0.1
 * @date 2020-10-29
 * 
 * @copyright Copyright (c) 2020
 * 
 */

#include <txrxcombo.h>
#include <adidma.h>
#include <string.h>
#include <stdlib.h>

static inline uint64_t get_nsec()
{
    struct timespec mac_ts;
    timespec_get(&mac_ts, TIME_UTC);
    return (uint64_t)mac_ts.tv_sec * 1000000000L + ((uint64_t)mac_ts.tv_nsec);
}

uint64_t g_start = 0;

int modem_combo_init(modem_combo *dev, int modem_uio_id, int tx_uio_id, int rx_uio_id)
{
    if (dev == NULL)
        return -1;
    printf("%s: %d\n", __func__, __LINE__);
    dev->bus = (uio_dev *)malloc(sizeof(uio_dev));
    if (dev->bus == NULL)
        return -1;
    printf("%s: %d\n", __func__, __LINE__);
    if (uio_init(dev->bus, modem_uio_id) < 0)
        return -1;
    printf("%s: %d\n", __func__, __LINE__);
    dev->rx_dma = (adidma *)malloc(sizeof(adidma));
    if (dev->rx_dma == NULL)
        return -1;
    printf("%s: %d\n", __func__, __LINE__);
    dev->tx_dma = (adidma *)malloc(sizeof(adidma));
    if (dev->tx_dma == NULL)
        return -1;
    printf("%s: %d\n", __func__, __LINE__);
    if (adidma_init(dev->rx_dma, rx_uio_id, 0) < 0)
        return -1;
    printf("%s: %d\n", __func__, __LINE__);
    if (adidma_init(dev->tx_dma, tx_uio_id, 0) < 0)
        return -1;
    printf("%s: %d\n", __func__, __LINE__);
    return 1;
}
int modem_combo_reset(modem_combo *dev, int src_sel, int loopback)
{
    uio_write(dev->bus, COMBO_RESET, 0x1);
    uio_write(dev->bus, COMBO_RX_ENABLE, 0x0); // Receive not enabled by default
    uio_write(dev->bus, COMBO_FR_LOOP_BW, 40);
    uio_write(dev->bus, COMBO_EQ_MU, 200);
    uio_write(dev->bus, COMBO_SCOPE_SEL, 2);
    uio_write(dev->bus, COMBO_TX_DMA_SEL, 0);
    uio_write(dev->bus, COMBO_EQ_BYPASS, 0);
    uio_write(dev->bus, COMBO_PD_THRESHOLD, 10);
    uio_write(dev->bus, COMBO_INTERNAL_PACK_TX_TOGGLE, 0);
    uio_write(dev->bus, COMBO_PACKET_TX_ALWAYS, 0);
    uio_write(dev->bus, COMBO_TX_SRC_SEL, src_sel);
    uio_write(dev->bus, COMBO_TRX_LOOPBACK, loopback);
    uio_write(dev->bus, 0x130, 0);
    return 1;
}
#include <stdio.h>
int modem_rx_reset(modem_combo *dev)
{
    uio_write(dev->bus, COMBO_RESET, 0x1);
    fprintf(stderr, "%s Line %d: Reset modem\n", __func__, __LINE__);
    fflush(stdout);
    uio_write(dev->bus, 0x100, 40);
    fprintf(stderr, "%s Line %d: Loop bw set\n", __func__, __LINE__);
    fflush(stdout);
    uio_write(dev->bus, 0x104, 200);
    fprintf(stderr, "%s Line %d: EQmu set\n", __func__, __LINE__);
    fflush(stdout);
    uio_write(dev->bus, 0x108, 10);
    fprintf(stderr, "%s Line %d: PDThreshold set\n", __func__, __LINE__);
    fflush(stdout);
    uio_write(dev->bus, 0x10c, 0);
    uio_write(dev->bus, 0x110, 0);
    uio_write(dev->bus, 0x114, 0);
    return 1;
}
int modem_tx_reset(modem_combo *dev)
{
    uio_write(dev->bus, COMBO_RESET, 0x1);
    return 1;
}
int modem_combo_enable_rx(modem_combo *dev)
{
    uio_write(dev->bus, COMBO_RX_ENABLE, 0x1);
    return 1;
}
int modem_rx_enable_rx(modem_combo *dev)
{
    uio_write(dev->bus, 0x10c, 1);
    return 1;
}
ssize_t modem_combo_rx(modem_combo *dev, int tout, int max_count)
{
    ssize_t pack_sz = 0;
    int irq_status = -1;
    uint64_t start = 0;
    /*    while (pack_sz < 1 || pack_sz >= 8188)
    {
        adidma_read(dev->rx_dma, NULL, 8188);
        uio_read(dev->bus, COMBO_PAYLOAD_LEN, &pack_sz);
        printf("%s: %d: Payload len = %ld\n", __func__, __LINE__, pack_sz);
    }
    return pack_sz;*/
    // wait for interrupt on the syncRx line
    if (uio_unmask_irq(dev->bus) > 0)
    {
        int counter = 0;
        do
        {
            irq_status = uio_wait_irq(dev->bus, tout); // 60 second timeout
            printf("%s: %d: irq_status = %d\n", __func__, __LINE__, irq_status);
        } while (counter <= max_count && irq_status < 0); // 20 attempts
        start = get_nsec();
        // printf("%s: %d: irq_status = %d\n", __func__, __LINE__, irq_status);
        if (irq_status > 0) // received a packet!
            uio_read(dev->bus, COMBO_PAYLOAD_LEN, &pack_sz);
        if (pack_sz > 0 && pack_sz != 8188) // we got valid packet length
        {
            uint64_t start2 = get_nsec();
            int stat = adidma_read(dev->rx_dma, NULL, pack_sz + 8);
            uint64_t end = get_nsec();
            printf("%s: %d: Payload len = %ld\n", __func__, __LINE__, pack_sz);
            printf("%s Line %d: adidma_read took %lf seconds\n", __func__, __LINE__, (end - start2) * 1e-9);
            printf("%s Line %d: From TX initiation to RX initiation took %lf micro seconds\n", __func__, __LINE__, (start2 - g_start) * 1e-3);
            printf("%s Line %d: From RX inference to complete DMA transfer took %lf micro seconds\n", __func__, __LINE__, (end - start) * 1e-3);
            printf("%s Line %d: From TX initiation to complete DMA transfer took %lf micro seconds\n", __func__, __LINE__, (end - g_start) * 1e-3);
            if (stat > 0)
                return pack_sz;
        }
    }
    return 8188;
}
void modem_combo_read(modem_combo *dev, unsigned char *buf, ssize_t size)
{
    if (size > dev->rx_dma->mem_sz)
        return;
    if (buf == NULL)
        return;
    memcpy(buf, dev->rx_dma->mem_virt_addr, size);
}
int modem_combo_tx(modem_combo *dev, unsigned char *buf, ssize_t size)
{
    if (buf == NULL)
        return -1;
    if (size > dev->tx_dma->mem_sz - 8)
        return -2;
    printf("%s: Line %d\n", __func__, __LINE__);
    *((uint64_t *)(dev->tx_dma->mem_virt_addr)) = size + 8 - (size % 8) + 8; // top of the buffer
    uint64_t *ptr = (uint8_t *)(dev->tx_dma->mem_virt_addr) + 8;
    memcpy(ptr, buf, size);
    *((uint64_t *)(dev->tx_dma->mem_virt_addr) + size + 8) = 0x0;
    printf("TX size: %llu\n", *(uint64_t *)(dev->tx_dma->mem_virt_addr));
    uint8_t *ptr2 = (uint8_t *)(dev->tx_dma->mem_virt_addr) + 8;
    // for (int i = 0; i < size; i++)
    //     printf("%c", ptr2[i]);
    // printf("\n\n--------------------------\n\n");
    
    int ret = adidma_write(dev->tx_dma, NULL, *((uint64_t *)(dev->tx_dma->mem_virt_addr)) + 8, 0);
    g_start = get_nsec();
    printf("%s: Line %d\n", __func__, __LINE__);
    return ret;
}
void modem_combo_destroy(modem_combo *dev)
{
    printf("%s: %d\n", __func__, __LINE__);
    adidma_destroy(dev->tx_dma);
    printf("%s: %d\n", __func__, __LINE__);
    adidma_destroy(dev->rx_dma);
    printf("%s: %d\n", __func__, __LINE__);
    uio_destroy(dev->bus);
    printf("%s: %d\n", __func__, __LINE__);
    free(dev->tx_dma);
    printf("%s: %d\n", __func__, __LINE__);
    free(dev->rx_dma);
    printf("%s: %d\n", __func__, __LINE__);
    free(dev->bus);
    printf("%s: %d\n", __func__, __LINE__);
    return;
}

#define UNIT_TEST
#ifdef UNIT_TEST

#define MTU 256 //80
#define HEADER_DATA_SIZE 8
#define HEADER_FRAME_SIZE 8 //16
#define CRC_SIZE 8
#define PADDING_SIZE 16 //12
#define TX_BUF_SIZE HEADER_DATA_SIZE + HEADER_FRAME_SIZE + MTU + PADDING_SIZE
#define RX_BUF_SIZE (HEADER_FRAME_SIZE + MTU + PADDING_SIZE + CRC_SIZE)

// basically, send size of message and receive size of message + 8, last 8 bytes are going to be CRC

#include <time.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

time_t rawtime;
struct tm *timeinfo;

static char *rcvd_msg;
ssize_t rcvd_sz = 0;

void *rx_thread_fcn(void *dev)
{
#ifndef COMBO_MODEM
    modem_rx_enable_rx((modem_combo *)dev);
#else
    modem_combo_enable_rx((modem_combo *)dev);
#endif
    ssize_t size;
    while ((size = modem_combo_rx(dev, 5 * 1000, 10)) == 8188)
        ; // 20 minutes max
    if (size > 0)
    {
        printf("%s: Received %ld bytes\n", __func__, size);
        rcvd_msg = (char *)malloc(size);
        modem_combo_read(dev, rcvd_msg, size);
        rcvd_sz = size;
    }
    else
        printf("%s: Did not receive anything.\n", __func__);
    printf("%s: returning...\n", __func__);
    return NULL;
}
#ifndef TX_ONLY
int main()
{
    printf("Starting program...\n");
    printf("Creating message\n");
    time(&rawtime);
    timeinfo = localtime(&rawtime);
    char msg[(1 << 16) - 1];
    const char fmt_str[] = "\n\nTesting SPACE HAUC SDR.\n"
                           "The date is: %d-%d-%d, time is: %d:%d:%d\n"
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
                           "\n"
                           "Oh Ariadne, I am coming, I just need to work this maze inside my head\n"
                           "I came here like you asked, I killed the beast, that part of me is dead\n"
                           "Oh Ariadne, I just need to work this maze inside my head\n"
                           "If only I'd have listened to you when you " /*"offered me that thread\n"
                           /*"\n"
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
                           "Your smiling eyes are just a mirror for"*/
                           "\n\n\n"
                           "End of transmission";

    ssize_t size = snprintf(msg, (1 << 16) - 1, fmt_str, timeinfo->tm_year + 1900, timeinfo->tm_mon + 1, timeinfo->tm_mday, timeinfo->tm_hour, timeinfo->tm_min, timeinfo->tm_sec);

    modem_combo dev, dev1;
    if (modem_combo_init(&dev, 1, 3, 2) < 0)
        return -1;
    if (modem_combo_init(&dev1, 0, 3, 2) < 0)
        return -1;
    fflush(stderr);
    fflush(stdout);
#ifndef COMBO_MODEM
    modem_tx_reset(&dev); // external packet, no loopback
    printf("%s: %d\n", __func__, __LINE__);
    fflush(stderr);
    fflush(stdout);
    modem_rx_reset(&dev1); // external packet, no loopback
    printf("%s: %d\n", __func__, __LINE__);
    fflush(stderr);
    fflush(stdout);
#else
    modem_combo_reset(&dev, 0, 0);
#endif
    pthread_t rx_thread;
#ifndef COMBO_MODEM
    if (pthread_create(&rx_thread, NULL, rx_thread_fcn, (void *)&dev1))
#else
    if (pthread_create(&rx_thread, NULL, rx_thread_fcn, (void *)&dev))
#endif
    {
        perror("Error creating receive thread");
        return -1;
    }
    sleep(1);  // offset by 1 second
    sleep(10); // after 10 more seconds
    printf("main: Sending message!\n");
    fflush(stdout);
    fflush(stderr);
    uint64_t start = get_nsec();
    modem_combo_tx(&dev, msg, size);
    uint64_t end = get_nsec();
    printf("%s Line %d: adidma_write took %lf seconds\n", __func__, __LINE__, (end - start) * 1e-9);
    pthread_join(rx_thread, NULL);
    fflush(stdout);
    fflush(stderr);
    printf("Received:\n");
    fflush(stdout);
    fflush(stderr);
    int count = 0;
    for (int i = 0; i < rcvd_sz; i++)
    {
        printf("%c", rcvd_msg[i]);
        count++;
        fflush(stdout);
        fflush(stderr);
    }
    printf("\n\n%s: %d\n", __func__, __LINE__);
    printf("main: Printed %d characters\n", count);
    free(rcvd_msg);
    modem_combo_destroy(&dev);
#ifndef COMBO_MODEM
    modem_combo_destroy(&dev1);
#endif
    return 0;
}
#else
int main()
{
    printf("Starting program...\n");
    printf("Creating message\n");
    time(&rawtime);
    timeinfo = localtime(&rawtime);
    char msg[(1 << 16) - 1];
    const char fmt_str[] = "\n\nTesting SPACE HAUC SDR.\n"
                           "The date is: %d-%d-%d, time is: %d:%d:%d\n"
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
                           "\n"
                           "Oh Ariadne, I am coming, I just need to work this maze inside my head\n"
                           "I came here like you asked, I killed the beast, that part of me is dead\n"
                           "Oh Ariadne, I just need to work this maze inside my head\n"
                           "If only I'd have listened to you when you " /*"offered me that thread\n"
                           /*"\n"
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
                           "Your smiling eyes are just a mirror for"*/
                           "\n\n\n"
                           "End of transmission";

    ssize_t size = snprintf(msg, (1 << 16) - 1, fmt_str, timeinfo->tm_year + 1900, timeinfo->tm_mon + 1, timeinfo->tm_mday, timeinfo->tm_hour, timeinfo->tm_min, timeinfo->tm_sec);
    modem_combo dev;
     if (modem_combo_init(&dev, 0, 1, 1) < 0)
        return -1;
    fflush(stderr);
    fflush(stdout);
    modem_tx_reset(&dev); // external packet, no loopback
    printf("%s: %d\n", __func__, __LINE__);
    fflush(stderr);
    fflush(stdout);
    printf("main: Sending message!\n");
    fflush(stdout);
    fflush(stderr);
    uint64_t start = get_nsec();
    modem_combo_tx(&dev, msg, size);
    uint64_t end = get_nsec();
    printf("%s Line %d: adidma_read took %lf seconds\n", __func__, __LINE__, (end - start) * 1e-9);
    modem_combo_destroy(&dev);
    return 0;
}
#endif // TX_ONLY
#endif
