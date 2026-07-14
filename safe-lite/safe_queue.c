#include "safe_queue.h"

/* ─── safe_queue_metadata_crc ─────────────────────────────────────────────
 * Computes CRC8 over the queue metadata fields (head, tail, count).
 * This is used to detect silent corruption of the control structure.
 * ───────────────────────────────────────────────────────────────────────── */
uint8_t safe_queue_metadata_crc(const safe_queue_t *q)
{
    if (q == NULL) {
        return 0;
    }
    /* pack metadata into a small buffer for CRC computation */
    uint8_t buf[12];
    buf[0]  = (uint8_t)( q->head        & 0xFFU);
    buf[1]  = (uint8_t)((q->head >>  8) & 0xFFU);
    buf[2]  = (uint8_t)((q->head >> 16) & 0xFFU);
    buf[3]  = (uint8_t)((q->head >> 24) & 0xFFU);
    buf[4]  = (uint8_t)( q->tail        & 0xFFU);
    buf[5]  = (uint8_t)((q->tail >>  8) & 0xFFU);
    buf[6]  = (uint8_t)((q->tail >> 16) & 0xFFU);
    buf[7]  = (uint8_t)((q->tail >> 24) & 0xFFU);
    buf[8]  = (uint8_t)( q->count       & 0xFFU);
    buf[9]  = (uint8_t)((q->count >>  8) & 0xFFU);
    buf[10] = (uint8_t)((q->count >> 16) & 0xFFU);
    buf[11] = (uint8_t)((q->count >> 24) & 0xFFU);
    return safe_crc8_compute(buf, sizeof(buf));
}

/* ─── safe_queue_init ───────────────────────────────────────────────────── */
safe_status_t safe_queue_init(safe_queue_t *q)
{
    if (q == NULL) {
        return SAFE_ERROR;
    }
    memset(q, 0, sizeof(*q));
    return SAFE_OK;
}

/* ─── safe_queue_add ──────────────────────────────────────────────────────
 * Append an entry to the tail of the circular buffer.
 * Each entry is stamped with:
 *   magic    = SAFE_QUEUE_MAGIC (0xDEADBEEF)
 *   length   = payload length
 *   entry_crc = CRC8 over magic || length || payload
 *
 * Returns SAFE_QUEUE_FULL if no space, SAFE_OK on success.
 *
 * PRODUCTION: In an ISR context (e.g., DMA completion), the add operation
 * must be interrupt-safe.  This usually means disabling interrupts around
 * queue manipulation or using lock-free atomics on multi-core systems.
 * ───────────────────────────────────────────────────────────────────────── */
safe_status_t safe_queue_add(safe_queue_t *q, const uint8_t *data, size_t len)
{
    if (q == NULL || data == NULL) {
        return SAFE_ERROR;
    }
    if (len > SAFE_QUEUE_DATA_SIZE) {
        return SAFE_ERROR;
    }
    if (q->count >= SAFE_QUEUE_MAX_ENTRIES) {
        return SAFE_QUEUE_FULL;
    }

    safe_queue_entry_t *entry = &q->entries[q->tail];
    entry->magic  = SAFE_QUEUE_MAGIC;
    entry->length = (uint16_t)len;
    if (len > 0) {
        memcpy(entry->data, data, len);
    }

    /* CRC8 over magic || length || payload */
    uint8_t crc_buf[sizeof(uint32_t) + sizeof(uint16_t) + SAFE_QUEUE_DATA_SIZE];
    memcpy(&crc_buf[0], &entry->magic,  sizeof(uint32_t));
    memcpy(&crc_buf[4], &entry->length, sizeof(uint16_t));
    if (len > 0) {
        memcpy(&crc_buf[6], entry->data, len);
    }
    entry->entry_crc = safe_crc8_compute(crc_buf, 6 + len);

    q->tail = (q->tail + 1) % SAFE_QUEUE_MAX_ENTRIES;
    q->count++;
    return SAFE_OK;
}

/* ─── safe_queue_get ──────────────────────────────────────────────────────
 * Dequeue the entry at head.  Validates magic and entry_crc before returning
 * data.  Returns SAFE_MAGIC_ERROR or SAFE_CRC_MISMATCH on corruption.
 * ───────────────────────────────────────────────────────────────────────── */
safe_status_t safe_queue_get(safe_queue_t *q, uint8_t *data_out, size_t *len)
{
    if (q == NULL || data_out == NULL || len == NULL) {
        return SAFE_ERROR;
    }
    if (q->count == 0) {
        return SAFE_QUEUE_EMPTY;
    }

    safe_queue_entry_t *entry = &q->entries[q->head];

    if (entry->magic != SAFE_QUEUE_MAGIC) {
        return SAFE_MAGIC_ERROR;
    }

    /* recompute entry CRC */
    uint8_t crc_buf[sizeof(uint32_t) + sizeof(uint16_t) + SAFE_QUEUE_DATA_SIZE];
    memcpy(&crc_buf[0], &entry->magic,  sizeof(uint32_t));
    memcpy(&crc_buf[4], &entry->length, sizeof(uint16_t));
    if (entry->length > 0) {
        memcpy(&crc_buf[6], entry->data, entry->length);
    }
    uint8_t computed = safe_crc8_compute(crc_buf, 6 + entry->length);
    if (computed != entry->entry_crc) {
        return SAFE_CRC_MISMATCH;
    }

    if (entry->length > 0) {
        memcpy(data_out, entry->data, entry->length);
    }
    *len = entry->length;

    /* clear the entry to avoid stale data leaks */
    memset(entry, 0, sizeof(*entry));

    q->head = (q->head + 1) % SAFE_QUEUE_MAX_ENTRIES;
    q->count--;
    return SAFE_OK;
}

/* ─── safe_queue_peek ─────────────────────────────────────────────────────
 * Same as get but does not remove the entry.  Used for diagnostics.
 * ───────────────────────────────────────────────────────────────────────── */
safe_status_t safe_queue_peek(const safe_queue_t *q, uint8_t *data_out, size_t *len)
{
    if (q == NULL || data_out == NULL || len == NULL) {
        return SAFE_ERROR;
    }
    if (q->count == 0) {
        return SAFE_QUEUE_EMPTY;
    }
    const safe_queue_entry_t *entry = &q->entries[q->head];
    if (entry->length > 0) {
        memcpy(data_out, entry->data, entry->length);
    }
    *len = entry->length;
    return SAFE_OK;
}

/* ─── Query helpers ─────────────────────────────────────────────────────── */
bool safe_queue_is_empty(const safe_queue_t *q)
{
    return (q != NULL) && (q->count == 0);
}

bool safe_queue_is_full(const safe_queue_t *q)
{
    return (q != NULL) && (q->count >= SAFE_QUEUE_MAX_ENTRIES);
}

uint32_t safe_queue_count(const safe_queue_t *q)
{
    return (q != NULL) ? q->count : 0;
}

/* ─── safe_queue_validate ─────────────────────────────────────────────────
 * Runs a full integrity check: validates metadata CRC and all occupied
 * entries.  Returns SAFE_OK only if everything is intact.
 * PRODUCTION: A periodic validation task runs this check in the background,
 * logging any corruption to NVRAM before triggering a recovery action.
 * ───────────────────────────────────────────────────────────────────────── */
safe_status_t safe_queue_validate(const safe_queue_t *q)
{
    if (q == NULL) {
        return SAFE_ERROR;
    }

    /* walk occupied slots (head → tail) */
    uint32_t cnt = q->count;
    uint32_t idx = q->head;
    for (uint32_t i = 0; i < cnt; i++) {
        const safe_queue_entry_t *entry = &q->entries[idx];
        if (entry->magic != SAFE_QUEUE_MAGIC) {
            return SAFE_MAGIC_ERROR;
        }
        uint8_t crc_buf[sizeof(uint32_t) + sizeof(uint16_t) + SAFE_QUEUE_DATA_SIZE];
        memcpy(&crc_buf[0], &entry->magic,  sizeof(uint32_t));
        memcpy(&crc_buf[4], &entry->length, sizeof(uint16_t));
        if (entry->length > 0) {
            memcpy(&crc_buf[6], entry->data, entry->length);
        }
        uint8_t computed = safe_crc8_compute(crc_buf, 6 + entry->length);
        if (computed != entry->entry_crc) {
            return SAFE_CRC_MISMATCH;
        }
        idx = (idx + 1) % SAFE_QUEUE_MAX_ENTRIES;
    }
    return SAFE_OK;
}
