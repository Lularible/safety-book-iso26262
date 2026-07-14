#ifndef FAULT_INJECT_H
#define FAULT_INJECT_H

#include "safe_common.h"
#include "safe_e2e.h"
#include "safe_queue.h"
#include "safe_wdg.h"

/* ─── Fault injection API ─────────────────────────────────────────────────
 * These functions deliberately corrupt data to verify that our safety
 * mechanisms detect the fault.  Each function models a real-world fault.
 *
 * PRODUCTION: Fault injection in production is NEVER done on live data
 * paths.  Instead, it's performed in a SIL/HIL test environment or via
 * a dedicated "test mode" that isolates the system under test.
 * ───────────────────────────────────────────────────────────────────────── */

/* Flip a single bit at bit_offset in a byte buffer (simulates SEU/soft error) */
void fault_bit_flip(uint8_t *buf, size_t buf_len, size_t bit_offset);

/* Corrupt a random byte in a buffer (simulates EMI/voltage glitch) */
void fault_corrupt_byte(uint8_t *buf, size_t buf_len, size_t byte_offset);

/* Reset E2E counter in a context (simulates counter desync / ECU reset) */
void fault_counter_reset(safe_e2e_ctx_t *ctx);

/* Wipe a queue entry's magic number (simulates RAM corruption) */
void fault_queue_magic_corrupt(safe_queue_t *q, uint32_t entry_index);

/* Corrupt a queue entry's CRC (simulates metadata bit-flip) */
void fault_queue_crc_corrupt(safe_queue_t *q, uint32_t entry_index);

/* Corrupt queue metadata (head/tail/count) to simulate structural damage */
void fault_queue_metadata_corrupt(safe_queue_t *q);

/* Stop calling watchdog checkpoint to simulate task hang */
void fault_wdg_starvation(safe_wdg_t *wdg, int slot_id);

#endif /* FAULT_INJECT_H */
