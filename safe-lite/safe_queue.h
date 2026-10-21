#ifndef SAFE_QUEUE_H
#define SAFE_QUEUE_H

#include "safe_common.h"

/* ─── Safe queue entry ────────────────────────────────────────────────────
 * Each entry is integrity-protected: magic number + length + payload CRC.
 * The queue struct itself does NOT store a metadata checksum; a helper
 * (safe_queue_metadata_crc) can compute one over (head, tail, count) on
 * demand for diagnostic snapshots, but nothing compares against it.
 *
 * PRODUCTION: In real ECU firmware, queue entries reside in non-cacheable
 * shared memory (e.g., between Cortex-R5 and Cortex-A53 via RPMsg).  The
 * CRC protects against bit-flips in DDR due to EMI or aging.
 * ───────────────────────────────────────────────────────────────────────── */
typedef struct {
    uint32_t magic;
    uint16_t length;
    uint8_t  data[SAFE_QUEUE_DATA_SIZE];
    uint8_t  entry_crc;
} safe_queue_entry_t;

typedef struct {
    safe_queue_entry_t entries[SAFE_QUEUE_MAX_ENTRIES];
    uint32_t           head;
    uint32_t           tail;
    uint32_t           count;
} safe_queue_t;

/* ─── API ───────────────────────────────────────────────────────────────── */
safe_status_t safe_queue_init(safe_queue_t *q);
safe_status_t safe_queue_add(safe_queue_t *q, const uint8_t *data, size_t len);
safe_status_t safe_queue_get(safe_queue_t *q, uint8_t *data_out, size_t *len);
safe_status_t safe_queue_peek(const safe_queue_t *q, uint8_t *data_out, size_t *len);
bool          safe_queue_is_empty(const safe_queue_t *q);
bool          safe_queue_is_full(const safe_queue_t *q);
uint32_t      safe_queue_count(const safe_queue_t *q);
uint8_t       safe_queue_metadata_crc(const safe_queue_t *q);
safe_status_t safe_queue_validate(const safe_queue_t *q);

#endif /* SAFE_QUEUE_H */
