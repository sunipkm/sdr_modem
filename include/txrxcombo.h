/**
 * @file txrxcombo.h
 * @author Sunip K. Mukherjee (sunipkmukherjee@gmail.com)
 * @brief 
 * @version 0.1
 * @date 2020-10-29
 * 
 * @copyright Copyright (c) 2020
 * 
 */
#ifndef TX_RX_COMBO_H
#define TX_RX_COMBO_H
#include <libuio.h>
#include <adidma.h>
#include <pthread.h>
#include <stdint.h>

typedef enum
{
    COMBO_RESET = 0x0,                     /// Reset the entire IP
    COMBO_RX_ENABLE = 0x118,               /// Enable RX. Default: 0, Receive: 1
    COMBO_FR_LOOP_BW = 0x100,              /// Default: 40
    COMBO_EQ_MU = 0x104,                   /// Default: 200
    COMBO_SCOPE_SEL = 0x108,               /// Default: 2
    COMBO_TX_DMA_SEL = 0x110,              /// Default: 0
    COMBO_EQ_BYPASS = 0x114,               /// Default: 0
    COMBO_PD_THRESHOLD = 0x11c,            /// Default: 10
    COMBO_INTERNAL_PACK_TX_TOGGLE = 0x138, /// Default: 0, Switch to 1 and 0 quickly
    COMBO_PACKET_TX_ALWAYS = 0x12c,        /// Default: 0
    COMBO_TX_SRC_SEL = 0x124,              /// 0 == DMA, 1 == Internal packet gen
    COMBO_TRX_LOOPBACK = 0x128,            /// 1 == Loopback, 0 == RF
    COMBO_PAYLOAD_LEN = 0x134,             /// Payload length in bytes
} TXRXCOMBO_REGS;

typedef struct
{
    uio_dev *bus;
    adidma *rx_dma;
    adidma *tx_dma;
    pthread_mutex_t lock;
} modem_combo;

int modem_combo_init(modem_combo *dev, int modem_uio_id, int tx_uio_id, int rx_uio_id);
int modem_combo_reset(modem_combo *dev, int src_sel, int loopback);
int modem_combo_enable_rx(modem_combo *dev);
ssize_t modem_combo_rx(modem_combo *dev, int tout, int max_count);
void modem_combo_read(modem_combo *dev, unsigned char *buf, ssize_t size);
int modem_combo_tx(modem_combo *dev, unsigned char *buf, ssize_t size);
void modem_combo_destroy(modem_combo *dev);

#endif // TX_RX_COMBO_H