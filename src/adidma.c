/**
 * @file adidma.c
 * @author Sunip K. Mukherjee (sunipkmukherjee@gmail.com)
 * @brief 
 * @version 0.1
 * @date 2020-10-29
 * 
 * @copyright Copyright (c) 2020
 * 
 */
#include <stdlib.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <sys/mman.h>
#include <time.h>
#include <unistd.h>
#include <string.h>
#include <adidma.h>
#include <poll.h>

const int ADIDMA_RX_DMA_TIMEOUT = 10000; /// Defines the interrupt timeout for
                                         /// RX DMA in milliseconds. Setting
                                         /// this variable to -1 would imply
                                         /// that the poll() call hang forever.
                                         /// NOT RECOMMENDED. Use a positive
                                         /// value for this constant ONLY.

static inline uint64_t get_nsec()
{
    struct timespec mac_ts;
    timespec_get(&mac_ts, TIME_UTC);
    return (uint64_t)mac_ts.tv_sec * 1000000000L + ((uint64_t)mac_ts.tv_nsec);
}

int adidma_init(adidma *dev, int uio_id, unsigned char ext_buffer_enb)
{
    if (ext_buffer_enb < 0 || ext_buffer_enb > ADIDMA_MEMCPY_ALL)
        ext_buffer_enb = ADIDMA_MEMCPY_ALL;
    if (dev == NULL)
        printf("%s: device ID null\n", __func__);
    dev->bus = (uio_dev *)malloc(sizeof(uio_dev));
    if (dev->bus == NULL)
    {
#ifdef ADIDMA_DEBUG
        fprintf(stderr, "%s Line %d: ", __func__, __LINE__);
        fprintf(stderr, "Error allocating memory for UIO device struct, aborting...\n");
#endif
        return ADIDMA_UIO_MALLOC_ERROR;
    }
    int ret;
    if ((ret = uio_init(dev->bus, uio_id)) < 0)
    {
#ifdef ADIDMA_DEBUG
        fprintf(stderr, "%s Line %d: ", __func__, __LINE__);
        fprintf(stderr, "Error initiating register interface, aborting...\n");
#endif
        free(dev->bus);
        return ret;
    }

    char fname[256];
    if (snprintf(fname, 256, "/sys/class/uio/uio%d/maps/map1/size", uio_id) < 0)
    {
#ifdef ADIDMA_DEBUG
        fprintf(stderr, "%s Line %d: %s", __func__, __LINE__, "Buffer size fname could not be generated, aborting...\n");
#endif
        uio_destroy(dev->bus);
        free(dev->bus);
        return ADIDMA_FNAME_ERROR;
    }

    FILE *fp;
    ssize_t size;

    fp = fopen(fname, "r");
    if (!fp)
    {
#ifdef ADIDMA_DEBUG
        fprintf(stderr, "%s Line %d: %s", __func__, __LINE__, "Buffer size file could not be opened, aborting...\n");
#endif
        uio_destroy(dev->bus);
        free(dev->bus);
        return ADIDMA_FD_OPEN_ERROR;
    }

    ret = fscanf(fp, "0x%lx", &size);
    fclose(fp);
    if (ret < 0)
    {
#ifdef ADIDMA_DEBUG
        fprintf(stderr, "%s Line %d: %s", __func__, __LINE__, "Buffer size read error, aborting...\n");
#endif
        uio_destroy(dev->bus);
        free(dev->bus);
        return ADIDMA_FILE_READ_ERROR;
    }

    if (size < 0)
    {
#ifdef ADIDMA_DEBUG
        fprintf(stderr, "%s Line %d: %s", __func__, __LINE__, "Buffer size negative, aborting...\n");
#endif
        return ADIDMA_BUF_SIZE_ERROR;
    }

    dev->mem_sz = size;

    uint32_t mem_addr;

    if (snprintf(fname, 256, "/sys/class/uio/uio%d/maps/map1/addr", uio_id) < 0)
    {
#ifdef ADIDMA_DEBUG
        fprintf(stderr, "%s Line %d: %s", __func__, __LINE__, "Buffer address fname could not be generated, aborting...\n");
#endif
        uio_destroy(dev->bus);
        free(dev->bus);
        return ADIDMA_FNAME_ERROR;
    }

    fp = fopen(fname, "r");
    if (!fp)
    {
#ifdef ADIDMA_DEBUG
        fprintf(stderr, "%s Line %d: %s", __func__, __LINE__, "Buffer address file could not be opened, aborting...\n");
#endif
        uio_destroy(dev->bus);
        free(dev->bus);
        return ADIDMA_FD_OPEN_ERROR;
    }

    ret = fscanf(fp, "0x%x", &mem_addr);
    fclose(fp);
    if (ret < 0)
    {
#ifdef ADIDMA_DEBUG
        fprintf(stderr, "%s Line %d: %s", __func__, __LINE__, "Buffer address read error, aborting...\n");
#endif
        uio_destroy(dev->bus);
        free(dev->bus);
        return ADIDMA_FILE_READ_ERROR;
    }

    dev->mem_addr = mem_addr;
#ifdef ADIDMA_DEBUG
    fprintf(stderr, "%s: %d: %s -> 0x%lx\n:", __func__, __LINE__, fname, mem_addr);
#endif
    dev->mem_fd = open("/dev/mem", O_RDWR | O_SYNC);
    if (dev->mem_fd < 0)
    {
#ifdef ADIDMA_DEBUG
        fprintf(stderr, "%s Line %d: %s", __func__, __LINE__, "Can not open /dev/mem, aborting...\n");
#endif
        uio_destroy(dev->bus);
        free(dev->bus);
        return dev->mem_fd;
    }

    uint32_t mapping_len, page_mask, page_sz;

    page_sz = sysconf(_SC_PAGESIZE);
    mapping_len = (((dev->mem_sz / page_sz) + 1) * page_sz);
    page_mask = page_sz - 1;
    dev->mapping_addr = mmap(NULL, mapping_len, PROT_READ | PROT_WRITE, MAP_SHARED, dev->mem_fd, (dev->mem_addr & ~page_mask));
    if (dev->mapping_addr == MAP_FAILED)
    {
#ifdef ADIDMA_DEBUG
        fprintf(stderr, "%s Line %d: %s", __func__, __LINE__, "Can not map DMA buffer, aborting...\n");
#endif
        close(dev->mem_fd);
        uio_destroy(dev->bus);
        free(dev->bus);
        return ADIDMA_BUF_MMAP_ERROR;
    }
    dev->mem_virt_addr = (dev->mapping_addr + (dev->mem_addr & page_mask));
#ifdef ADIDMA_DEBUG
    fprintf(stderr, "%s: Physical address 0x%08x, mmap address 0x%p, virtual adderss 0x%p\n", __func__, dev->mem_addr, dev->mapping_addr, dev->mem_virt_addr);
#endif
    return 1;
}

void adidma_destroy(adidma *dev)
{
    if (dev->mapping_addr != NULL)
    {
        uint32_t mapping_len, page_sz;

        page_sz = sysconf(_SC_PAGESIZE);
        mapping_len = (((dev->mem_sz / page_sz) + 1) * page_sz);
        munmap(dev->mapping_addr, mapping_len);
    }

    if (dev->mem_fd > 0)
        close(dev->mem_fd);

    uio_destroy(dev->bus);
    free(dev->bus);
}

int adidma_write(adidma *dev, unsigned int offset, ssize_t size, unsigned char cyclic)
{
    if (size < 0 || size + offset > dev->mem_sz)
    {
#ifdef ADIDMA_DEBUG
        fprintf(stderr, "%s Line %d: %s", __func__, __LINE__, "Error doing transfer, size error...\n");
#endif
        return ADIDMA_XFER_SZ_ERR;
    }

    uint32_t reg_val, xfer_id;

#ifdef ADIDMA_DEBUG
    fprintf(stderr, "%s Line %d: %s", __func__, __LINE__, "Resetting DMA for TX...\n");
#endif
    uio_write(dev->bus, DMAC_REG_CTRL, 0x0);
#ifdef ADIDMA_DEBUG
    fprintf(stderr, "%s Line %d: %s", __func__, __LINE__, "Enabling DMA for TX...\n");
#endif
    uio_write(dev->bus, DMAC_REG_CTRL, DMAC_CTRL_ENABLE);
#ifdef ADIDMA_DEBUG
    fprintf(stderr, "%s Line %d: %s", __func__, __LINE__, "Unmasking IRQ for TX...\n");
#endif
    uio_write(dev->bus, DMAC_REG_IRQ_MASK, 0x0);
#ifdef ADIDMA_DEBUG
    fprintf(stderr, "%s Line %d: %s", __func__, __LINE__, "Getting Xfer ID for TX...\n");
#endif
    uio_read(dev->bus, DMAC_REG_XFER_ID, &xfer_id);
#ifdef ADIDMA_DEBUG
    fprintf(stderr, "%s Line %d: %s", __func__, __LINE__, "Setting cyclic/non cyclic flag...\n");
#endif
    uio_write(dev->bus, DMAC_REG_FLAGS, cyclic);
#ifdef ADIDMA_DEBUG
    fprintf(stderr, "%s Line %d: %s", __func__, __LINE__, "Setting TX address...\n");
#endif
    uio_write(dev->bus, DMAC_REG_SRC_ADDR, dev->mem_addr + offset);
#ifdef ADIDMA_DEBUG
    fprintf(stderr, "%s Line %d: %s", __func__, __LINE__, "Setting TX stride...\n");
#endif
    uio_write(dev->bus, DMAC_REG_SRC_STRIDE, 0x0);
#ifdef ADIDMA_DEBUG
    fprintf(stderr, "%s Line %d: %s", __func__, __LINE__, "Setting TX length...\n");
#endif
    uio_write(dev->bus, DMAC_REG_X_LEN, size - 1);
    uio_write(dev->bus, DMAC_REG_Y_LEN, 0x0);
#ifdef ADIDMA_DEBUG
    fprintf(stderr, "%s Line %d: %s", __func__, __LINE__, "Starting TX transfer...\n");
#endif
    uio_write(dev->bus, DMAC_REG_START_XFER, 0x1);

    if (!cyclic)
    {
        uint64_t start_irq, end_irq;
        uint32_t counter = 1;
#ifdef ADIDMA_DEBUG
        start_irq = get_nsec();
#endif
        // prepare to get interrupt from start_xfer and loop until we do
#ifndef ADIDMA_NOIRQ
        do
        {
            if (uio_unmask_irq(dev->bus) > 0)
            {
                uio_wait_irq(dev->bus, ADIDMA_RX_DMA_TIMEOUT);
            }
            uio_read(dev->bus, DMAC_REG_IRQ_PENDING, &reg_val);
#ifdef ADIDMA_DEBUG
            fprintf(stderr, "%s Line %d: ", __func__, __LINE__);
            fprintf(stderr, "After %u IRQ return: 0x%01x\n", counter++, reg_val);
#endif
            // attempt to clear the interrupt
            uio_write(dev->bus, DMAC_REG_IRQ_PENDING, DMAC_IRQ_SOT);
        } while (reg_val & DMAC_IRQ_SOT != DMAC_IRQ_SOT);
#ifdef ADIDMA_DEBUG
        end_irq = get_nsec();
        fprintf(stderr, "%s Line %d: ", __func__, __LINE__);
        fprintf(stderr, "Looped: %d times, time taken to start transfer: %lf usec\n", counter, (end_irq - start_irq) * 1e-3);
        counter = 1; // reset counter
        start_irq = get_nsec();
#endif
        if (!dev->tx_check_completion)
            goto write_eot;
        // Get end of transfer interrupt
        do
        {
            if (reg_val & DMAC_IRQ_EOT) // already finished
                break;
            if (uio_unmask_irq(dev->bus) > 0)
            {
                uio_wait_irq(dev->bus, ADIDMA_RX_DMA_TIMEOUT);
            }
            uio_read(dev->bus, DMAC_REG_IRQ_PENDING, &reg_val);
#ifdef ADIDMA_DEBUG
            fprintf(stderr, "%s Line %d: ", __func__, __LINE__);
            fprintf(stderr, "After %u IRQ return: 0x%01x\n", counter++, reg_val);
#endif
            // attempt to clear the interrupt
            uio_write(dev->bus, DMAC_REG_IRQ_PENDING, DMAC_IRQ_EOT);
        } while (reg_val & DMAC_IRQ_EOT != DMAC_IRQ_EOT);
#else
        do
        {
            uio_read(dev->bus, DMAC_REG_IRQ_PENDING, &reg_val);
#ifdef ADIDMA_DEBUG
            counter++;
#endif
        } while((reg_val & 0x3) != 0x3); // keep reading on busy-wait until you hit both SOT and EOT
        uio_write(dev->bus, DMAC_REG_IRQ_PENDING, 0x3);
#endif // ADIDMA_NOIRQ
#ifdef ADIDMA_DEBUG
        end_irq = get_nsec();
        fprintf(stderr, "%s Line %d: ", __func__, __LINE__);
        fprintf(stderr, "Looped: %d times, time taken to complete transfer: %lf usec\n", counter, (end_irq - start_irq) * 1e-3);
        start_irq = get_nsec();
#endif
        do
        {
            uio_read(dev->bus, DMAC_REG_XFER_DONE, &reg_val);
        } while ((reg_val & (1 << xfer_id)) != (uint32_t)(1 << xfer_id));
#ifdef ADIDMA_DEBUG
        end_irq = get_nsec();
        fprintf(stderr, "%s Line %d: ", __func__, __LINE__);
        fprintf(stderr, "Time taken to confirm transfer completion: %lf usec\n", (end_irq - start_irq) * 1e-3);
#endif
    }
write_eot:
    return size;
}

int adidma_read(adidma *dev, unsigned int offset, ssize_t size)
{
    uint32_t reg_val, xfer_id;

    if (size < 0 || offset + size > dev->mem_sz)
    {
#ifdef ADIDMA_DEBUG
        fprintf(stderr, "%s Line %d: %s", __func__, __LINE__, "Error doing transfer, size or memory access violation error...\n");
#endif
        return ADIDMA_XFER_SZ_ERR;
    }
#ifdef ADIDMA_DEBUG
    fprintf(stderr, "%s Line %d: %s", __func__, __LINE__, "Resetting DMA for RX...\n");
#endif
    printf("Executing UIO write\n");
    fflush(stdout);
    uio_write(dev->bus, DMAC_REG_CTRL, 0x0);
    printf("Executed UIO write\n");
    fflush(stdout);
#ifdef ADIDMA_DEBUG
    fprintf(stderr, "%s Line %d: %s", __func__, __LINE__, "Enabling DMA for RX...\n");
#endif
    uio_write(dev->bus, DMAC_REG_CTRL, DMAC_CTRL_ENABLE);
#ifdef ADIDMA_DEBUG
    fprintf(stderr, "%s Line %d: %s", __func__, __LINE__, "Unmasking IRQ for RX...\n");
#endif
    uio_write(dev->bus, DMAC_REG_IRQ_MASK, 0x0);
#ifdef ADIDMA_DEBUG
    fprintf(stderr, "%s Line %d: %s", __func__, __LINE__, "Getting Xfer ID for RX...\n");
#endif
    uio_read(dev->bus, DMAC_REG_XFER_ID, &xfer_id);
#ifdef ADIDMA_DEBUG
    fprintf(stderr, "%s Line %d: %s", __func__, __LINE__, "Clearing any pending interrupts...\n");
#endif
    uio_read(dev->bus, DMAC_REG_IRQ_PENDING, &reg_val);
    uio_write(dev->bus, DMAC_REG_IRQ_PENDING, reg_val);

#ifdef ADIDMA_DEBUG
    fprintf(stderr, "%s Line %d: %s 0x%lx\n", __func__, __LINE__, "Setting RX address...", dev->mem_addr);
#endif
    uio_write(dev->bus, DMAC_REG_DEST_ADDR, dev->mem_addr + offset);
#ifdef ADIDMA_DEBUG
    fprintf(stderr, "%s Line %d: %s", __func__, __LINE__, "Setting RX stride...\n");
#endif
    uio_write(dev->bus, DMAC_REG_DEST_STRIDE, 0x0);
#ifdef ADIDMA_DEBUG
    fprintf(stderr, "%s Line %d: %s", __func__, __LINE__, "Setting RX length...\n");
#endif
    uio_write(dev->bus, DMAC_REG_X_LEN, size - 1);
    uio_write(dev->bus, DMAC_REG_Y_LEN, 0x0);

#ifdef ADIDMA_DEBUG
    fprintf(stderr, "%s Line %d: %s", __func__, __LINE__, "Starting RX transfer...\n");
#endif
    uio_write(dev->bus, DMAC_REG_START_XFER, 0x1);

    uint64_t start_irq, end_irq;
    uint32_t counter = 1;
#ifdef ADIDMA_DEBUG
    start_irq = get_nsec();
#endif
    // prepare to get interrupt from start_xfer and loop until we do
#ifndef ADIDMA_NOIRQ
    do
    {
        if (uio_unmask_irq(dev->bus) > 0)
        {
            uio_wait_irq(dev->bus, ADIDMA_RX_DMA_TIMEOUT);
        }
        uio_read(dev->bus, DMAC_REG_IRQ_PENDING, &reg_val);
#ifdef ADIDMA_DEBUG
        fprintf(stderr, "%s Line %d: ", __func__, __LINE__);
        fprintf(stderr, "After %u IRQ return: 0x%01x\n", counter++, reg_val);
#endif
        // attempt to clear the interrupt
        uio_write(dev->bus, DMAC_REG_IRQ_PENDING, DMAC_IRQ_SOT);
    } while (!(reg_val & DMAC_IRQ_SOT));
#ifdef ADIDMA_DEBUG
    end_irq = get_nsec();
    fprintf(stderr, "%s Line %d: ", __func__, __LINE__);
    fprintf(stderr, "Looped: %d times, time taken to start transfer: %lf usec\n", counter, (end_irq - start_irq) * 1e-3);
    counter = 1; // reset counter
    start_irq = get_nsec();
#endif
    // Get end of transfer interrupt
    do
    {
        if (reg_val & DMAC_IRQ_EOT) // in case transfer was already completed
            break;
        if (uio_unmask_irq(dev->bus) > 0)
        {
            uio_wait_irq(dev->bus, ADIDMA_RX_DMA_TIMEOUT);
        }
        uio_read(dev->bus, DMAC_REG_IRQ_PENDING, &reg_val);
#ifdef ADIDMA_DEBUG
        fprintf(stderr, "%s Line %d: ", __func__, __LINE__);
        fprintf(stderr, "After %u IRQ return: 0x%01x\n", counter++, reg_val);
#endif
        // attempt to clear the interrupt
        uio_write(dev->bus, DMAC_REG_IRQ_PENDING, DMAC_IRQ_EOT);
        // check if we are already at end of transfer
        uio_read(dev->bus, DMAC_REG_XFER_DONE, &reg_val);
        if ((reg_val & (1 << xfer_id)) == (uint32_t)(1 << xfer_id))
            goto read_eot;
    } while (reg_val & DMAC_IRQ_EOT != DMAC_IRQ_EOT);
#else
    do
    {
        uio_read(dev->bus, DMAC_REG_IRQ_PENDING, &reg_val);
#ifdef ADIDMA_DEBUG
        counter++;
#endif
    } while((reg_val & 0x3) != 0x3); // keep reading on busy-wait until you hit both SOT and EOT
    uio_write(dev->bus, DMAC_REG_IRQ_PENDING, 0x3);
#endif // ADIDMA_NOIRQ
#ifdef ADIDMA_DEBUG
    end_irq = get_nsec();
    fprintf(stderr, "%s Line %d: ", __func__, __LINE__);
    fprintf(stderr, "Looped: %d times, time taken to complete transfer: %lf usec\n", counter, (end_irq - start_irq) * 1e-3);
    start_irq = get_nsec();
#endif
    do
    {
        uio_read(dev->bus, DMAC_REG_XFER_DONE, &reg_val);
    } while ((reg_val & (1 << xfer_id)) != (uint32_t)(1 << xfer_id));
read_eot:
#ifdef ADIDMA_DEBUG
    end_irq = get_nsec();
    fprintf(stderr, "%s Line %d: ", __func__, __LINE__);
    fprintf(stderr, "Time taken to confirm transfer completion: %lf usec\n", (end_irq - start_irq) * 1e-3);
    start_irq = get_nsec();
#ifdef ADIDMA_ENABLING_UIO_DEBUG
#undef UIO_DEBUG
#undef ADIDMA_ENABLING_UIO_DEBUG
#endif
#endif
    return 1;
}
