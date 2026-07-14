#include "fault_inject.h"
#include "safe_e2e.h"
#include "safe_queue.h"
#include "safe_wdg.h"

/* ─── fault_bit_flip ──────────────────────────────────────────────────────
 * Flips a single bit in a byte buffer.
 * bit_offset is absolute: bit 0 = LSB of buf[0], bit 7 = MSB of buf[0],
 * bit 8 = LSB of buf[1], etc.
 * PRODUCTION: This models a Single Event Upset (SEU) caused by neutron
 * or alpha particle strike in SRAM cells.  Real-world SEU rates for
 * automotive-grade SRAM at sea level are ~100-1000 FIT/Mbit.
 * ───────────────────────────────────────────────────────────────────────── */
void fault_bit_flip(uint8_t *buf, size_t buf_len, size_t bit_offset)
{
    if (buf == NULL || buf_len == 0) {
        return;
    }
    size_t byte_idx = bit_offset / 8;
    size_t bit_idx  = bit_offset % 8;
    if (byte_idx >= buf_len) {
        return;
    }
    buf[byte_idx] ^= (uint8_t)(1U << bit_idx);
}

/* ─── fault_corrupt_byte ──────────────────────────────────────────────────
 * Overwrites a single byte with 0xFF (all-ones fault model).
 * PRODUCTION: Stuck-at faults (SA0/SA1) are common in aging flash memory.
 * DRAM "row hammer" can also flip entire bytes at vulnerable addresses.
 * ───────────────────────────────────────────────────────────────────────── */
void fault_corrupt_byte(uint8_t *buf, size_t buf_len, size_t byte_offset)
{
    if (buf == NULL || byte_offset >= buf_len) {
        return;
    }
    buf[byte_offset] = 0xFF;
}

/* ─── fault_counter_reset ─────────────────────────────────────────────────
 * Resets the E2E counter to simulate an ECU that rebooted mid-sequence.
 * The receiver will see a counter regression and flag SAFE_COUNTER_ERROR.
 * ───────────────────────────────────────────────────────────────────────── */
void fault_counter_reset(safe_e2e_ctx_t *ctx)
{
    if (ctx == NULL) {
        return;
    }
    ctx->counter = 0;
}

/* ─── fault_queue_magic_corrupt ───────────────────────────────────────────
 * Overwrites the magic number of a queue entry.  The next safe_queue_get()
 * or safe_queue_validate() will return SAFE_MAGIC_ERROR.
 * ───────────────────────────────────────────────────────────────────────── */
void fault_queue_magic_corrupt(safe_queue_t *q, uint32_t entry_index)
{
    if (q == NULL || entry_index >= SAFE_QUEUE_MAX_ENTRIES) {
        return;
    }
    q->entries[entry_index].magic = 0xBAADF00DU;
}

/* ─── fault_queue_crc_corrupt ─────────────────────────────────────────────
 * Flips a bit in the entry CRC.  This simulates a bit-flip that occurs
 * AFTER the CRC was computed (e.g., in a shared memory buffer between
 * two cores where only one core can compute a fresh CRC).
 * ───────────────────────────────────────────────────────────────────────── */
void fault_queue_crc_corrupt(safe_queue_t *q, uint32_t entry_index)
{
    if (q == NULL || entry_index >= SAFE_QUEUE_MAX_ENTRIES) {
        return;
    }
    q->entries[entry_index].entry_crc ^= 0x01;
}

/* ─── fault_queue_metadata_corrupt ────────────────────────────────────────
 * Corrupts the queue's head pointer (increments it arbitrarily).
 * This simulates a race condition or ISR corruption that damages the
 * queue's structural integrity.  The next validate() will detect that
 * the entries no longer match their CRC expectations (if head points
 * into empty/corrupt memory).
 * ───────────────────────────────────────────────────────────────────────── */
void fault_queue_metadata_corrupt(safe_queue_t *q)
{
    if (q == NULL) {
        return;
    }
    q->head = (q->head + 3) % SAFE_QUEUE_MAX_ENTRIES;
}

/* ─── fault_wdg_starvation ────────────────────────────────────────────────
 * Stops calling safe_wdg_checkpoint for a specific slot, simulating a
 * task that hangs or enters an infinite loop.  The watchdog's service()
 * will detect the missed deadline and fire the timeout callback.
 *
 * This function itself does nothing — the "fault" is implemented in the
 * integration test by simply omitting the checkpoint call.
 * ───────────────────────────────────────────────────────────────────────── */
void fault_wdg_starvation(safe_wdg_t *wdg, int slot_id)
{
    (void)wdg;
    (void)slot_id;
    /* intentional no-op: starvation = absence of checkpoint call */
}
