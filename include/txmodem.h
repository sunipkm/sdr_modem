/**
 * @file txmodem.h
 * @author Sunip K. Mukherjee (sunipkmukherjee@gmail.com)
 * @brief 
 * @version 0.1
 * @date 2020-12-28
 * 
 * @copyright Copyright (c) 2020
 * 
 */

#ifndef TX_MODEM_H
#define TX_MODEM_H

#ifdef __cplusplus
extern "C" {
#endif

#include <libuio.h>
#include <adidma.h>
#include <pthread.h>
#include <stdint.h>

typedef enum
{
    TXMODEM_RESET = 0x0,                     /// Reset the entire IP
    TXMODEM_DMA_SEL = 0x110,                 /// Default: 0
    TXMODEM_INTERNAL_PACK_TX_TOGGLE = 0x138, /// Default: 0, Switch to 1 and 0 quickly
    TXMODEM_PACKET_TX_ALWAYS = 0x12c,        /// Default: 0
    TXMODEM_SRC_SEL = 0x124,                 /// 0 == DMA, 1 == Internal packet gen
} TXMODEM_REGS;

typedef struct
{
    uio_dev bus[1];
    adidma dma[1];
    size_t mtu;         // MTU of a frame (data size only, TX header size and frame header size has to be accounted for in TX, and frame header size and 8 byte padding has to be accounted for in RX)
    size_t max_pack_sz; // Maximum packet size
} txmodem;
/**
 * @brief Initialize TX Modem IP
 * 
 * @param dev Pointer to txmodem struct
 * @param txmodem_id UIO device ID for the TX Modem 
 * @param txdma_id ADIDMA device ID for the TX path
 * @return int positive on success, negative on failure
 */
int txmodem_init(txmodem *dev, int txmodem_id, int txdma_id);
/**
 * @brief Reset TX modem and set the source parameter
 * 
 * @param dev Pointer to txmodem struct
 * @param src_sel 0 == DMA, 1 == Internal packet generator
 * @return int Positive on success, negative on failure
 */
int txmodem_reset(txmodem *dev, int src_sel);
/**
 * @brief Take supplied data, split into packets and transmit them through the TX path, blocks until transfer is completed
 * 
 * @param dev Pointer to txmodem struct
 * @param buf Pointer to source buffer
 * @param size Size of source buffer
 * @return int positive on success, negative on failure
 */
int txmodem_write(txmodem *dev, uint8_t *buf, ssize_t size);
/**
 * @brief Close device handles and free up memory
 * 
 * @param dev Pointer to txmodem struct
 */
void txmodem_destroy(txmodem *dev);
#ifdef __cplusplus
}
#endif
#endif // TX_MODEM_H