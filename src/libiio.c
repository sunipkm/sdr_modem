#include <iio.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <stdbool.h>
#define __USE_MISC
#include <unistd.h>
#undef __USE_MISC
#include "libiio.h"

#define eprintf(...)              \
    fprintf(stderr, __VA_ARGS__); \
    fflush(stderr)

static char *ensm_mode_str[] = {"sleep", "fdd", "tdd"};

int adradio_init(adradio_t *dev)
{
    if (dev == NULL)
    {
        eprintf("%s: Device memory not allocated, exiting...\n", __func__);
        return EXIT_FAILURE;
    }
    // Make every pointer NULL so that we can address errors
    memset(dev, 0x0, sizeof(adradio_t));
    // try to create context
    dev->ctx = iio_create_default_context();
    if (dev->ctx == NULL)
    {
        eprintf("%s: Could not initialize IIO device context, exiting...", __func__);
        return EXIT_FAILURE;
    }
    // try to open ad9361-phy device
    dev->ad_phy = iio_context_find_device(dev->ctx, "ad9361-phy");
    if (dev->ad_phy == NULL)
    {
        eprintf("%s: Could not find ad9361-phy, exiting...", __func__);
        goto exit_failure_ctx;
    }
    // try to open cf-ad9361-dds-core-lpc
    dev->dds_lpc = iio_context_find_device(dev->ctx, "cf-ad9361-dds-core-lpc");
    if (dev->ad_phy == NULL)
    {
        eprintf("%s: Could not find cf-ad9361-dds-core-lpc, exiting...", __func__);
        goto exit_failure_ctx;
    }
    // try to open the TX LO channel
    dev->tx_lo = iio_device_find_channel(dev->ad_phy, "altvoltage1", true);
    if (dev->tx_lo == NULL)
    {
        eprintf("%s: Could not find channel TX LO, exiting...", __func__);
        goto exit_failure_ctx;
    }
    // try to open the RX LO channel
    dev->rx_lo = iio_device_find_channel(dev->ad_phy, "altvoltage0", true);
    if (dev->rx_lo == NULL)
    {
        eprintf("%s: Could not find channel RX LO, exiting...", __func__);
        goto exit_failure_tx_lo;
    }
    // try to open the TX_IQ channel
    dev->tx_iq = iio_device_find_channel(dev->ad_phy, "voltage0", true);
    if (dev->tx_iq == NULL)
    {
        eprintf("%s: Could not find channel TX IQ, exiting...", __func__);
        goto exit_failure_rx_lo;
    }
    // try to open the RX_IQ channel
    dev->rx_iq = iio_device_find_channel(dev->ad_phy, "voltage0", false);
    if (dev->rx_iq == NULL)
    {
        eprintf("%s: Could not find channel RX IQ, exiting...", __func__);
        goto exit_failure_tx_iq;
    }
    // try to open temp sensor channel
    dev->temp = iio_device_find_channel(dev->ad_phy, "temp0", false);
    if (dev->temp == NULL)
    {
        eprintf("%s: Could not find channel RX IQ, exiting...", __func__);
        goto exit_failure_rx_iq;
    }
    if (adradio_reconfigure_dds(dev) != EXIT_SUCCESS)
        goto exit_failure_tempsensor;
    else
        return EXIT_SUCCESS;
exit_failure_tempsensor:
    iio_channel_disable(dev->temp);
exit_failure_rx_iq:
    iio_channel_disable(dev->rx_iq);
exit_failure_tx_iq:
    iio_channel_disable(dev->tx_iq);
exit_failure_rx_lo:
    iio_channel_disable(dev->rx_lo);
exit_failure_tx_lo:
    iio_channel_disable(dev->tx_lo);
exit_failure_ctx:
    iio_context_destroy(dev->ctx);
    return EXIT_FAILURE;
}

void adradio_destroy(adradio_t *dev)
{
    if (dev == NULL)
        return;
    iio_channel_disable(dev->rx_iq);
    iio_channel_disable(dev->tx_iq);
    iio_channel_disable(dev->rx_lo);
    iio_channel_disable(dev->tx_lo);
    iio_channel_disable(dev->temp);
    iio_context_destroy(dev->ctx);
}

int adradio_set_tx_lo(adradio_t *dev, long long freq)
{
    return iio_channel_attr_write_longlong(dev->tx_lo, "frequency", freq);
}

int adradio_get_tx_lo(adradio_t *dev, long long *freq)
{
    return iio_channel_attr_read_longlong(dev->tx_lo, "frequency", freq);
}

int adradio_set_rx_lo(adradio_t *dev, long long freq)
{
    return iio_channel_attr_write_longlong(dev->rx_lo, "frequency", freq);
}

int adradio_get_rx_lo(adradio_t *dev, long long *freq)
{
    return iio_channel_attr_read_longlong(dev->rx_lo, "frequency", freq);
}

int adradio_set_tx_samp(adradio_t *dev, long long freq)
{
    return iio_channel_attr_write_longlong(dev->tx_iq, "sampling_frequency", freq);
}

int adradio_get_tx_samp(adradio_t *dev, long long *freq)
{
    return iio_channel_attr_read_longlong(dev->tx_iq, "sampling_frequency", freq);
}

int adradio_set_rx_samp(adradio_t *dev, long long freq)
{
    return iio_channel_attr_write_longlong(dev->rx_iq, "sampling_frequency", freq);
}

int adradio_get_rx_samp(adradio_t *dev, long long *freq)
{
    return iio_channel_attr_read_longlong(dev->rx_iq, "sampling_frequency", freq);
}

int adradio_set_tx_bw(adradio_t *dev, long long freq)
{
    return iio_channel_attr_write_longlong(dev->tx_iq, "rf_bandwidth", freq);
}

int adradio_get_tx_bw(adradio_t *dev, long long *freq)
{
    return iio_channel_attr_read_longlong(dev->tx_iq, "rf_bandwidth", freq);
}

int adradio_set_rx_bw(adradio_t *dev, long long freq)
{
    return iio_channel_attr_write_longlong(dev->rx_iq, "rf_bandwidth", freq);
}

int adradio_get_rx_bw(adradio_t *dev, long long *freq)
{
    return iio_channel_attr_read_longlong(dev->rx_iq, "rf_bandwidth", freq);
}

int adradio_set_tx_hardwaregain(adradio_t *dev, double gain)
{
    return iio_channel_attr_write_double(dev->tx_iq, "hardwaregain", gain);
}

int adradio_get_tx_hardwaregain(adradio_t *dev, double *gain)
{
    return iio_channel_attr_read_double(dev->tx_iq, "hardwaregain", gain);
}

int adradio_set_rx_hardwaregain(adradio_t *dev, double gain)
{
    return iio_channel_attr_write_double(dev->rx_iq, "hardwaregain", gain);
}

int adradio_get_rx_hardwaregain(adradio_t *dev, double *gain)
{
    return iio_channel_attr_read_double(dev->rx_iq, "hardwaregain", gain);
}

int adradio_get_temp(adradio_t *dev, long long *temp)
{
    return iio_channel_attr_read_longlong(dev->temp, "input", temp);
}

int adradio_get_rssi(adradio_t *dev, double *rssi)
{
    return iio_channel_attr_read_double(dev->rx_iq, "rssi", rssi);
}

int adradio_ensm_mode(adradio_t *dev, enum ensm_mode mode)
{
    if ((mode < SLEEP) || (mode > TDD))
        return EXIT_FAILURE;
    return iio_device_attr_write(dev->ad_phy, "ensm_mode", ensm_mode_str[mode]);
}

int adradio_load_fir(adradio_t *dev, const char *fname)
{
    int ret;
    if (fname == NULL)
    {
        eprintf("%s: File name invalid!\n", __func__);
        return EXIT_FAILURE;
    }
    if (dev == NULL)
    {
        eprintf("%s: Device not allocated, fatal error!\n", __func__);
        return EXIT_FAILURE;
    }
    ret = iio_channel_attr_write_bool(dev->rx_iq, "filter_fir_en", false);
    if (ret != EXIT_SUCCESS)
    {
        eprintf("%s: Could not disable FIR filter for application, exiting...\n", __func__);
        return ret;
    }
    FILE *fp = fopen(fname, "r");
    if (fp == NULL)
    {
        eprintf("%s: Could not open file %s, please check path\n", __func__, fname);
        return EXIT_FAILURE;
    }
    ret = EXIT_FAILURE; // prime for failed exit
    ssize_t fsize = 0;
    fseek(fp, 0L, SEEK_END);
    fsize = ftell(fp);
    if (fsize <= 0)
    {
        eprintf("%s: Invalid size %ld for file %s, could not load FIR config\n", __func__, fsize, fname);
        goto err_close_file;
    }
    char *buf = NULL;
    buf = (char *)malloc(fsize);
    if (buf == NULL)
    {
        eprintf("%s: ", __func__);
        perror("malloc failed");
        goto err_close_file;
    }
    ssize_t rdsize = fread(buf, sizeof(char), fsize, fp);
    if (rdsize != fsize)
    {
        eprintf("%s: Read %ld out of %ld bytes: ", __func__, rdsize, fsize);
        perror("could not read FIR config from file");
        goto err_free_mem;
    }
    ret = iio_device_attr_write_raw(dev->ad_phy, "filter_fir_config", buf, fsize);
    if (ret != EXIT_SUCCESS)
    {
        eprintf("%s: Could not load FIR filter config into PHY\n", __func__);
        goto err_free_mem;
    }
    ret = iio_channel_attr_write_bool(dev->rx_iq, "filter_fir_en", true);
    if (ret != EXIT_SUCCESS)
    {
        eprintf("%s: Could not enable FIR filter config on PHY\n", __func__);
        goto err_free_mem;
    }
err_free_mem:
    free(buf);
err_close_file:
    fclose(fp);
    return ret;
}

int adradio_reconfigure_dds(adradio_t *dev)
{
    if (dev == NULL)
    {
        eprintf("%s: Device not allocated, fatal error!\n", __func__);
        return EXIT_FAILURE;
    }
    if (dev->dds_lpc == NULL)
    {
        eprintf("%s: Core-LPC not allocated, fatal error!\n", __func__);
        return EXIT_FAILURE;
    }
    int reg = 0x0, val = 0x0;
    reg = 0x40;
    val = 0x0;
    if (iio_device_reg_write(dev->dds_lpc, reg, val) != EXIT_SUCCESS)
        return EXIT_FAILURE;
#ifdef ADPHY_DEBUG
    int dval[1];
    iio_device_reg_read(dev->dds_lpc, reg, dval);
    eprintf("%s: cf-ad9361-dds-core-lpc reg %x value %x\n", reg, dval);
#endif
    usleep(1000);
    reg = 0x40;
    val = 0x2;
    if (iio_device_reg_write(dev->dds_lpc, reg, val) != EXIT_SUCCESS)
        return EXIT_FAILURE;
#ifdef ADPHY_DEBUG
    int dval[1];
    iio_device_reg_read(dev->dds_lpc, reg, dval);
    eprintf("%s: cf-ad9361-dds-core-lpc reg %x value %x\n", reg, dval);
#endif
    usleep(1000);
    reg = 0x40;
    val = 0x3;
    if (iio_device_reg_write(dev->dds_lpc, reg, val) != EXIT_SUCCESS)
        return EXIT_FAILURE;
#ifdef ADPHY_DEBUG
    int dval[1];
    iio_device_reg_read(dev->dds_lpc, reg, dval);
    eprintf("%s: cf-ad9361-dds-core-lpc reg %x value %x\n", reg, dval);
#endif
    usleep(1000);
    reg = 0x4c;
    val = 0x3;
    if (iio_device_reg_write(dev->dds_lpc, reg, val) != EXIT_SUCCESS)
        return EXIT_FAILURE;
#ifdef ADPHY_DEBUG
    int dval[1];
    iio_device_reg_read(dev->dds_lpc, reg, dval);
    eprintf("%s: cf-ad9361-dds-core-lpc reg %x value %x\n", reg, dval);
#endif
    usleep(1000);
    reg = 0x44;
    val = 0x0;
    if (iio_device_reg_write(dev->dds_lpc, reg, val) != EXIT_SUCCESS)
        return EXIT_FAILURE;
#ifdef ADPHY_DEBUG
    int dval[1];
    iio_device_reg_read(dev->dds_lpc, reg, dval);
    eprintf("%s: cf-ad9361-dds-core-lpc reg %x value %x\n", reg, dval);
#endif
    usleep(1000);
    reg = 0x48;
    val = 0x0;
    if (iio_device_reg_write(dev->dds_lpc, reg, val) != EXIT_SUCCESS)
        return EXIT_FAILURE;
#ifdef ADPHY_DEBUG
    int dval[1];
    iio_device_reg_read(dev->dds_lpc, reg, dval);
    eprintf("%s: cf-ad9361-dds-core-lpc reg %x value %x\n", reg, dval);
#endif
    usleep(1000);
    for (int i = 0; i < 4; i++)
    {
        reg = 0x418 + (i * 0x40);
        val = 0x2;
        if (iio_device_reg_write(dev->dds_lpc, reg, val) != EXIT_SUCCESS)
            return EXIT_FAILURE;
#ifdef ADPHY_DEBUG
        int dval[1];
        iio_device_reg_read(dev->dds_lpc, reg, dval);
        eprintf("%s: cf-ad9361-dds-core-lpc reg %x value %x\n", reg, dval);
#endif
        usleep(1000);
    }
    reg = 0x44;
    val = 0x1;
    if (iio_device_reg_write(dev->dds_lpc, reg, val) != EXIT_SUCCESS)
        return EXIT_FAILURE;
#ifdef ADPHY_DEBUG
    int dval[1];
    iio_device_reg_read(dev->dds_lpc, reg, dval);
    eprintf("%s: cf-ad9361-dds-core-lpc reg %x value %x\n", reg, dval);
#endif
    usleep(1000);
    return EXIT_SUCCESS;
}

#ifdef IIO_UNIT_TEST
#include <signal.h>
#include <time.h>

volatile sig_atomic_t done = 0;

void sighandler(__notused int sig)
{
    done = 1;
}

static inline uint64_t get_nsec()
{
    struct timespec mac_ts;
    timespec_get(&mac_ts, TIME_UTC);
    return (uint64_t)mac_ts.tv_sec * 1000000000L + ((uint64_t)mac_ts.tv_nsec);
}

int main()
{
    adradio_t dev[1];
    if (adradio_init(dev) != EXIT_SUCCESS)
    {
        return 0;
    }
    signal(SIGINT, &sighandler);
    while (!done)
    {
        long long lo_freq, samp, bw, temp;
        double gain, rssi;
        adradio_get_tx_lo(dev, &lo_freq);
        adradio_get_tx_samp(dev, &samp);
        adradio_get_tx_bw(dev, &bw);
        adradio_get_tx_hardwaregain(dev, &gain);
        adradio_get_temp(dev, &temp);
        printf("TX LO: %lld | TX Samp: %lld | TX BW: %lld | TX Gain: %lf | Temp: %.3lf\n", lo_freq, samp, bw, gain, temp * 0.001);
        adradio_get_rx_lo(dev, &lo_freq);
        adradio_get_rx_samp(dev, &samp);
        adradio_get_rx_bw(dev, &bw);
        adradio_get_rx_hardwaregain(dev, &gain);
        uint64_t start, end;
        start = get_nsec();
        adradio_get_rssi(dev, &rssi);
        end = get_nsec();
        printf("RX LO: %lld | RX Samp: %lld | RX BW: %lld | RX Gain: %lf | RSSI: %lf\n", lo_freq, samp, bw, gain, rssi);
        sleep(1);
    }
    adradio_destroy(dev);
    return 0;
}
#endif