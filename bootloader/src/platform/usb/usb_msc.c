#include "adafruit_802_sd.h"
#include "tusb.h"

#include <string.h>

#define MSC_BLOCK_SIZE 512U
#define SD_DATATIMEOUT ((uint32_t)100000000)

int32_t readDisplayCounter = 0;
int32_t writeDisplayCounter = 0;

extern bool dmaForSDAccess;

/* The SD driver uses DMA, so the buffer must be in DMA-accessible SRAM.
   TinyUSB's MSC endpoint buffer lives in DTCM, which DMA cannot reach. */
__attribute__((section(".ram_d2"), aligned(4))) static uint32_t bounce[MSC_BLOCK_SIZE / 4];

/* Cache the capacity: reading the CSD over SPI from the USB control callback
   is slow and can fail, which used to report garbage to the host. */
static uint32_t mscBlockCount = 0;

void usbMscInit(void) {
    ADAFRUIT_802_SD_CardInfo_t info;
    if (ADAFRUIT_802_SD_GetCardInfo(0, &info) == BSP_ERROR_NONE) {
        mscBlockCount = info.LogBlockNbr;
    }
}

/* Unsupported SCSI commands (everything except the native READ10/WRITE10). */
int32_t tud_msc_scsi_cb(uint8_t lun, uint8_t const scsi_cmd[16], void* buffer, uint16_t bufsize) {
    (void)lun;
    (void)scsi_cmd;
    (void)buffer;
    (void)bufsize;
    return -1;
}

void tud_msc_inquiry_cb(uint8_t lun, uint8_t vendor_id[8], uint8_t product_id[16], uint8_t product_rev[4]) {
    (void)lun;
    memcpy(vendor_id, "IXOX    ", 8);
    memcpy(product_id, "Pfm3SD          ", 16);
    memcpy(product_rev, "1.05", 4);
}

bool tud_msc_test_unit_ready_cb(uint8_t lun) {
    (void)lun;
    return true;
}

void tud_msc_capacity_cb(uint8_t lun, uint32_t* block_count, uint16_t* block_size) {
    (void)lun;
    if (mscBlockCount == 0U) {
        usbMscInit();
    }
    *block_count = mscBlockCount;
    *block_size = MSC_BLOCK_SIZE;
}

bool tud_msc_is_writable_cb(uint8_t lun) {
    (void)lun;
    return true;
}

int32_t tud_msc_read10_cb(uint8_t lun, uint32_t lba, uint32_t offset, void* buffer, uint32_t bufsize) {
    (void)lun;
    uint8_t* dst = (uint8_t*)buffer;
    uint32_t remaining = bufsize;
    uint32_t addr = lba;
    uint32_t off = offset;

    while (remaining > 0U) {
        uint32_t chunk = MSC_BLOCK_SIZE - off;
        if (chunk > remaining) {
            chunk = remaining;
        }
        if (PFM3_SD_ReadBlocks(bounce, addr, 1, SD_DATATIMEOUT, dmaForSDAccess) != BSP_ERROR_NONE) {
            return -1;
        }
        memcpy(dst, (uint8_t*)bounce + off, chunk);
        dst += chunk;
        remaining -= chunk;
        addr++;
        off = 0U;
    }

    readDisplayCounter = 500;
    return (int32_t)bufsize;
}

int32_t tud_msc_write10_cb(uint8_t lun, uint32_t lba, uint32_t offset, uint8_t* buffer, uint32_t bufsize) {
    (void)lun;
    uint8_t* src = buffer;
    uint32_t remaining = bufsize;
    uint32_t addr = lba;
    uint32_t off = offset;

    while (remaining > 0U) {
        uint32_t chunk = MSC_BLOCK_SIZE - off;
        if (chunk > remaining) {
            chunk = remaining;
        }
        if (off != 0U || chunk != MSC_BLOCK_SIZE) {
            if (PFM3_SD_ReadBlocks(bounce, addr, 1, SD_DATATIMEOUT, dmaForSDAccess) != BSP_ERROR_NONE) {
                return -1;
            }
        }
        memcpy((uint8_t*)bounce + off, src, chunk);
        if (PFM3_SD_WriteBlocks(bounce, addr, 1, SD_DATATIMEOUT, dmaForSDAccess) != BSP_ERROR_NONE) {
            return -1;
        }
        src += chunk;
        remaining -= chunk;
        addr++;
        off = 0U;
    }

    uint32_t timeout = 100000U;
    while (ADAFRUIT_802_SD_GetCardState(0) != BSP_ERROR_NONE) {
        if (timeout-- == 0U) {
            return -1;
        }
    }

    writeDisplayCounter = 500;
    return (int32_t)bufsize;
}