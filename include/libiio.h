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

    enum gain_mode
    {
        SLOW_ATTACK = 1,
        FAST_ATTACK
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
        struct iio_context *ctx; /// IIO Context
        struct iio_device *ad_phy; /// AD9361-Phy device
        struct iio_device *dds_lpc; /// CF-DDS-LPC-CORE device
        struct iio_channel *temp; /// Temperature channel
        struct iio_channel *tx_lo; /// TX Carrier
        struct iio_channel *rx_lo; /// RX Carrier
        struct iio_channel *tx_iq; /// TX IQ
        struct iio_channel *rx_iq; /// RX IQ
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
     * @brief Check if FIR filter is enabled on the radio
     * 
     * @param dev adradio_t struct
     * @param trx TX or RX channel
     * @param cond FIR filter attribute
     * @return int EXIT_SUCCESS or EXIT_FAILURE
     */
    int adradio_check_fir(adradio_t *dev, enum iodev trx, bool *cond);
    /**
     * @brief Enable or disable FIR filter on the radio
     * 
     * @param dev adradio_t struct
     * @param trx TX or RX channel
     * @param cond enabled on true, disabled on false
     * @return int EXIT_SUCCESS or EXIT_FAILURE
     */
    int adradio_enable_fir(adradio_t *dev, enum iodev trx, bool cond);
    /**
     * @brief Load a FIR filter from file into device. NOTE: Does not enable the filters
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
    /**
     * @brief Apply hardware gain
     * 
     * @param dev adradio_t struct
     * @param gain Gain to be applied
     * @return int EXIT_SUCCESS or EXIT_FAILURE
     */
    int adradio_set_tx_hardwaregain(adradio_t *dev, double gain);
    /**
     * @brief Get hardware gain
     * 
     * @param dev adradio_t struct
     * @param gain Gain to be read
     * @return int EXIT_SUCCESS or EXIT_FAILURE
     */
    int adradio_get_tx_hardwaregain(adradio_t *dev, double *gain);
    /**
     * @brief Change RX Gain mode between slow_attack and fast_attack
     * 
     * @param dev adradio_t struct
     * @param gain Gain mode setting
     * @return int EXIT_SUCCESS or EXIT_FAILURE
     */
    int adradio_set_rx_hardwaregainmode(adradio_t *dev, enum gain_mode mode);
    /**
     * @brief Get current RX Gain control mode
     * 
     * @param dev adradio_t struct
     * @param buf Buffer to store gain control mode
     * @param len Length of buffer
     * @return int EXIT_SUCCESS or EXIT_FAILURE
     */
    int adradio_get_rx_hardwaregainmode(adradio_t *dev, char *buf, size_t len);
    /**
     * @brief Get hardware gain
     * 
     * @param dev adradio_t struct
     * @param gain Gain to be read
     * @return int EXIT_SUCCESS or EXIT_FAILURE
     */
    int adradio_get_rx_hardwaregain(adradio_t *dev, double *gain);
    /**
     * @brief Get AD9361 temperature
     * 
     * @param dev adradio_t struct
     * @param temp Temperature of the chip in milicentigrade
     * @return int EXIT_SUCCESS or EXIT_FAILURE
     */
    int adradio_get_temp(adradio_t *dev, long long *temp);
#ifdef __cplusplus
}
#endif
#endif