/**
 * @file rxmodem.h
 * @author Sunip K. Mukherjee (sunipkmukherjee@gmail.com)
 * @brief 
 * @version 0.1
 * @date 2020-12-28
 * 
 * @copyright Copyright (c) 2020
 * 
 */

#ifndef RX_MODEM_H
#define RX_MODEM_H

#include <libuio.h>
#include <adidma.h>
#include <pthread.h>
#include <stdint.h>
#include "txrx_packdef.h"

#define NUM_IRQ_RETRIES 1

typedef enum
{
    RXMODEM_RESET = 0x0,           /// Reset the entire IP
    RXMODEM_FR_LOOP_BW = 0x100,    /// FR Loop Bandwidth, default: 40
    RXMODEM_EQ_MU = 0x104,         /// Default: 200
    RXMODEM_PD_THRESHOLD = 0x108,  /// Default: 10
    RXMODEM_RX_ENABLE = 0x10c,     /// Enable Decode: 1 to start radio, 0 to stop radio
    RXMODEM_BYPASS_CODING = 0x110, /// Enable coding bypass: 0
    RXMODEM_BYPASS_EQ = 0x114,     /// Enable bypass eq: 0
    RXMODEM_PAYLOAD_LEN = 0x134    /// Read register for payload length

    // RXMODEM_RESET = 0x0,          /// Reset the entire IP
    // RXMODEM_RX_ENABLE = 0x118,    /// Enable RX. Default: 0, Receive: 1
    // RXMODEM_FR_LOOP_BW = 0x100,   /// Default: 40
    // RXMODEM_EQ_MU = 0x104,        /// Default: 200
    // RXMODEM_SCOPE_SEL = 0x108,    /// Default: 2
    // RXMODEM_EQ_BYPASS = 0x114,    /// Default: 0
    // RXMODEM_PD_THRESHOLD = 0x11c, /// Default: 10
    // RXMODEM_PAYLOAD_LEN = 0x134,  /// Payload length in bytes

    // RXMODEM_RESET = 0x0,          /// Reset the entire IP
    // RXMODEM_RX_ENABLE = 0x118,    /// Enable RX. Default: 0, Receive: 1
    // RXMODEM_FR_LOOP_BW = 0x100,   /// Default: 40
    // RXMODEM_EQ_MU = 0x104,        /// Default: 200
    // RXMODEM_SCOPE_SEL = 0x10c,    /// Default: 2
    // RXMODEM_EQ_BYPASS = 0x114,    /// Default: 0
    // RXMODEM_PD_THRESHOLD = 0x11c, /// Default: 10
    // RXMODEM_PAYLOAD_LEN = 0x134,  /// Payload length in bytes
} RXMODEM_REGS;

typedef enum
{
    RX_FRAME_INVALID = -30,
    RX_INVALID_GUID,
    RX_PACK_SZ_ZERO,
    RX_NUM_FRAMES_ZERO,
    RX_FRAME_SZ_ZERO,
    RX_THREAD_SPAWN,
    RX_FRAME_CRC_FAILED,
    RX_MALLOC_FAILED,
} RXMODEM_ERROR;

#define RXMODEM_TIMEOUT 60000 // 60 seconds in ms

typedef struct
{
    int rx_enable;
    int fr_loop_bw;
    int eqmu;
    int scopesel;
    int eq_bypass;
    int pd_threshold;
} rxmodem_conf_t;

typedef struct
{
    uio_dev *bus;                      /// Pointer to uio device struct for the modem
    adidma dma[1];                     /// Pointer to ADI DMA struct
    rxmodem_conf_t conf[1];            /// RX modem configuration
    ssize_t *frame_ofst;               /// RX frame offset
    pthread_t thr;                     /// Pointer to RX thread
    pthread_cond_t rx_write;           /// Condition variable to arm rx thread, called by rxmodem_receiver
    pthread_mutex_t rx_write_m;        /// mutex to protect cond variable
    pthread_cond_t rx_read;            /// Condition variable to signal rx read function
    pthread_mutex_t rx_read_m;         /// Mutex to lock memory during read
    pthread_cond_t rx_rcv;             /// Condition variable to signal rx receive function
    pthread_mutex_t rx_rcv_m;          /// Mutex to protect cond variable
    int retcode;                       /// Return code from the irq thread, check on each wakeup
    int read_done;                     /// indicate read has been done
    int rx_done;                       /// Indicates thr to finish
    int frame_num;                     /// Length of frames on buffer (read up to this offset)
    modem_frame_header_t frame_hdr[1]; /// Frame header stored for checking
    size_t max_pack_sz;
} rxmodem;

int rxmodem_init(rxmodem *dev, int rxmodem_id, int rxdma_id);

int rxmodem_reset(rxmodem *dev, rxmodem_conf_t *conf);

int rxmodem_start(rxmodem *dev);

int rxmodem_stop(rxmodem *dev);

ssize_t rxmodem_receive(rxmodem *dev);

ssize_t rxmodem_read(rxmodem *dev, void *buf, ssize_t size);

void rxmodem_destroy(rxmodem *dev);

#endif
