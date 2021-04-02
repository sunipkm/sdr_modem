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

#ifdef __cplusplus
extern "C" {
#endif

#include "libuio.h"
#include "adidma.h"
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
    int fr_loop_bw;
    int eqmu;
    int scopesel;
    int eq_bypass;
    int pd_threshold;
} rxmodem_conf_t;

typedef struct
{
    uio_dev bus[1];                    /// Pointer to uio device struct for the modem
    adidma dma[1];                     /// Pointer to ADI DMA struct
    rxmodem_conf_t conf[1];            /// RX modem configuration
    ssize_t *frame_ofst;               /// RX frame offset
    pthread_t thr[1];                  /// Pointer to RX thread
    int retcode;                       /// Return code from the irq thread, check on each wakeup
    int read_done;                     /// indicate read has been done
    int rx_done;                       /// Indicates thr to finish
    int frame_num;                     /// Length of frames on buffer (read up to this offset)
    modem_frame_header_t frame_hdr[1]; /// Frame header stored for checking
    size_t max_pack_sz;
} rxmodem;

/**
 * @brief Initialize an rxmodem device
 * 
 * @param dev Pointer to rxmodem struct
 * @param rxmodem_id UIO device ID of the modem IP
 * @param rxdma_id UIO device ID of the DMA engine
 * @return int Positive on success, negative on error
 */
int rxmodem_init(rxmodem *dev, int rxmodem_id, int rxdma_id);
/**
 * @brief Restore default configuration of the modem
 * 
 * @param dev rxmodem struct to describe the device
 * @param conf Configuration to write, unused
 * @return int Returns 1, UIO errors will cause memory access errors
 */
int rxmodem_reset(rxmodem *dev, rxmodem_conf_t *conf);
/**
 * @brief Arm the modem to receive by enabling decoding, and unmasking the IRQ
 * 
 * @param dev rxmodem struct to describe the device
 * @return int 
 */
int rxmodem_start(rxmodem *dev);
/**
 * @brief Disarm the modem by disabling decoding and masking the IRQ
 * 
 * @param dev rxmodem struct to describe the device
 * @return int 
 */
int rxmodem_stop(rxmodem *dev);
/**
 * @brief Arm the radio and wait to receive data. Blocks until all frames have been received 
 * (i.e. all expected interrupts are addressed) for a valid frame header, or returns immediately.
 * 
 * @param dev rxmodem struct to describe the device
 * @return ssize_t Size of the packet to be received, to be used to allocate buffer for rxmodem_read.
 */
ssize_t rxmodem_receive(rxmodem *dev);
/**
 * @brief Read N bytes from the internal buffer of the rxmodem after receiving
 * TODO: Better error management on read
 * 
 * @param dev rxmodem struct to describe the device
 * @param buf Pointer to N-byte buffer to store the received data
 * @param size Size of the buffer in bytes
 * @return ssize_t Number of bytes recovered, if ret != N, there is an error
 */
ssize_t rxmodem_read(rxmodem *dev, uint8_t *buf, ssize_t size);
/**
 * @brief Reset and close an rxmodem device
 * 
 * @param dev rxmodem struct to describe the device
 */
void rxmodem_destroy(rxmodem *dev);

#ifdef __cplusplus
}
#endif
#endif
