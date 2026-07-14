#ifndef SAFE_COMMON_H
#define SAFE_COMMON_H

#define _POSIX_C_SOURCE 199309L
#define _DEFAULT_SOURCE

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

/* ─── E2E constants ─────────────────────────────────────────────────────── */
#define SAFE_E2E_DATAID_BITS     4
#define SAFE_E2E_COUNTER_BITS    4
#define SAFE_E2E_MAX_DATAID      ((1 << SAFE_E2E_DATAID_BITS) - 1)
#define SAFE_E2E_MAX_COUNTER     ((1 << SAFE_E2E_COUNTER_BITS) - 1)
#define SAFE_E2E_HEADER_SIZE     2
#define SAFE_E2E_MAX_DATA_LEN    64

/* ─── Queue constants ───────────────────────────────────────────────────── */
#define SAFE_QUEUE_MAGIC         0xDEADBEEFU
#define SAFE_QUEUE_MAX_ENTRIES   8
#define SAFE_QUEUE_DATA_SIZE     32

/* ─── Watchdog constants ────────────────────────────────────────────────── */
#define SAFE_WDG_MAX_SLOTS       8

/* ─── Status codes ──────────────────────────────────────────────────────── */
typedef enum {
    SAFE_OK                 = 0,
    SAFE_ERROR              = 1,
    SAFE_CRC_MISMATCH       = 2,
    SAFE_COUNTER_ERROR      = 3,
    SAFE_DATAID_MISMATCH    = 4,
    SAFE_QUEUE_FULL         = 5,
    SAFE_QUEUE_EMPTY        = 6,
    SAFE_MAGIC_ERROR        = 7,
    SAFE_TIMEOUT            = 8,
    SAFE_METADATA_CORRUPT   = 9,
    SAFE_NOT_INITIALIZED    = 10,
} safe_status_t;

/* ─── CRC8 lookup table (polynomial 0x1D, CRC-8/AUTOSAR) ────────────────── */
extern const uint8_t safe_crc8_table[256];

/* ─── API ───────────────────────────────────────────────────────────────── */
uint8_t safe_crc8_compute(const uint8_t *data, size_t len);

#endif /* SAFE_COMMON_H */
