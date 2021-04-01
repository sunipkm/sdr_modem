/**
 * @file libiio.h
 * @author Sunip K. Mukherjee (sunipkmukherjee@gmail.com)
 * @brief 
 * @version 0.1
 * @date 2021-04-01
 * 
 * @copyright Copyright (c) 2021
 * 
 */
#ifndef __LIBIIO_H
#define __LIBIIO_H
#ifdef __cplusplus
extern "C"
{
#endif
#include <iio.h>

/* helper macros */
#define MHZ(x) ((long long)(x * 1000000.0 + .5))
#define GHZ(x) ((long long)(x * 1000000000.0 + .5))

    /* RX is input, TX is output */
    enum iodev
    {
        RX,
        TX
    };

    enum ensm_mode
    {
        SLEEP,
        FDD,
        TDD
    };

    /* common RX and TX streaming params */
    typedef struct
    {
        long long bw_hz;    // Analog banwidth in Hz
        long long fs_hz;    // Baseband sample rate in Hz
        long long lo_hz;    // Local oscillator frequency in Hz
        const char *rfport; // Port name
    } ad_stream_cfg;

    typedef struct
    {
        struct iio_context *ctx;
        struct iio_device *ad_phy;
        struct iio_device *dds_lpc;
        struct iio_channel *tx_lo;
        struct iio_channel *rx_lo;
        struct iio_channel *tx_iq;
        struct iio_channel *rx_iq;
    } adradio_t;

    /**
     * @brief Initialize AD9361 Radio devices
     * 
     * @param dev adradio_t struct
     * @return int EXIT_SUCCESS or EXIT_FAILURE
     */
    int adradio_init(adradio_t *dev);
    /**
     * @brief Close AD9361 Radio devices
     * 
     * @param dev adradio_t struct
     */
    void adradio_destroy(adradio_t *dev);
    /**
     * @brief Set TX carrier frequency
     * 
     * @param dev adradio_t struct
     * @param freq Carrier frequency
     * @return int EXIT_SUCCESS or EXIT_FAILURE
     */
    int adradio_set_tx_lo(adradio_t *dev, long long freq);
    /**
     * @brief Set RX carrier frequency
     * 
     * @param dev adradio_t struct
     * @param freq 
     * @return int EXIT_SUCCESS or EXIT_FAILURE
     */
    int adradio_set_rx_lo(adradio_t *dev, long long freq);
    /**
     * @brief 
     * 
     * @param dev adradio_t struct
     * @param freq 
     * @return int EXIT_SUCCESS or EXIT_FAILURE
     */
    int adradio_set_tx_samp(adradio_t *dev, long long freq);
    /**
     * @brief 
     * 
     * @param dev adradio_t struct
     * @param freq 
     * @return int EXIT_SUCCESS or EXIT_FAILURE
     */
    int adradio_set_rx_samp(adradio_t *dev, long long freq);
    /**
     * @brief 
     * 
     * @param dev adradio_t struct
     * @param freq 
     * @return int EXIT_SUCCESS or EXIT_FAILURE
     */
    int adradio_set_tx_bw(adradio_t *dev, long long freq);
    /**
     * @brief 
     * 
     * @param dev adradio_t struct
     * @param freq 
     * @return int EXIT_SUCCESS or EXIT_FAILURE
     */
    int adradio_set_rx_bw(adradio_t *dev, long long freq);
    /**
     * @brief Set TX carrier frequency
     * 
     * @param dev adradio_t struct
     * @param freq Carrier frequency
     * @return int EXIT_SUCCESS or EXIT_FAILURE
     */
    int adradio_get_tx_lo(adradio_t *dev, long long *freq);
    /**
     * @brief Set RX carrier frequency
     * 
     * @param dev adradio_t struct
     * @param freq 
     * @return int EXIT_SUCCESS or EXIT_FAILURE
     */
    int adradio_get_rx_lo(adradio_t *dev, long long *freq);
    /**
     * @brief 
     * 
     * @param dev adradio_t struct
     * @param freq 
     * @return int EXIT_SUCCESS or EXIT_FAILURE
     */
    int adradio_get_tx_samp(adradio_t *dev, long long *freq);
    /**
     * @brief 
     * 
     * @param dev adradio_t struct
     * @param freq 
     * @return int EXIT_SUCCESS or EXIT_FAILURE
     */
    int adradio_get_rx_samp(adradio_t *dev, long long *freq);
    /**
     * @brief 
     * 
     * @param dev adradio_t struct
     * @param freq 
     * @return int EXIT_SUCCESS or EXIT_FAILURE
     */
    int adradio_get_tx_bw(adradio_t *dev, long long *freq);
    /**
     * @brief 
     * 
     * @param dev adradio_t struct
     * @param freq 
     * @return int EXIT_SUCCESS or EXIT_FAILURE
     */
    int adradio_get_rx_bw(adradio_t *dev, long long *freq);
    /**
     * @brief 
     * 
     * @param dev adradio_t struct
     * @param rssi 
     * @return int EXIT_SUCCESS or EXIT_FAILURE
     */
    int adradio_get_rssi(adradio_t *dev, double *rssi);
    /**
     * @brief 
     * 
     * @param dev adradio_t struct 
     * @param mode 
     * @return int EXIT_SUCCESS or EXIT_FAILURE
     */
    int adradio_ensm_mode(adradio_t *dev, enum ensm_mode mode);
    /**
     * @brief 
     * 
     * @param dev adradio_t struct
     * @param fname 
     * @return int EXIT_SUCCESS or EXIT_FAILURE
     */
    int adradio_load_fir(adradio_t *dev, const char *fname);
    /**
     * @brief Reconfigure the DDS LPC registers
     * 
     * @param dev adradio_t struct
     * @return int EXIT_SUCCESS or EXIT_FAILURE
     */
    int adradio_reconfigure_dds(adradio_t *dev);
#ifdef __cplusplus
}
#endif
#endif