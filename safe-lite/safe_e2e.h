#ifndef SAFE_E2E_H
#define SAFE_E2E_H

#include "safe_common.h"

/* ─── E2E context (per sender-receiver pair) ──────────────────────────────
 * Simplified E2E Profile 1: CRC8 + 4-bit counter + 4-bit DataID.
 * Each protected message carries a 2-byte header followed by payload.
 *
 * Header layout (big-endian on the wire):
 *   Byte 0: CRC8, computed over [DataID(1B) | Counter(1B) | payload(N B)].
 *           The CRC byte itself is NOT covered by the CRC computation.
 *   Byte 1: [Counter(4 bits) | DataID(4 bits)], MSB first within byte.
 *
 * Wire format:
 *   +------+------+------------...------------+
 *   | CRC8 | Ctr|DID |        payload        |
 *   +------+------+------------...------------+
 *     byte0  byte1    byte2 ... byteN+1
 *
 * The CRC8 is computed over a synthetic buffer:
 *   [DataID(1B) | Counter(1B) | payload(0..N bytes)]
 * So CRC8 covers identity, freshness, and content in one pass.
 *
 * PRODUCTION: AUTOSAR E2E Profile 1 uses CRC-8 with init value 0x00 and
 * optionally an XOR checksum.  Real profiles also support longer counters
 * (up to 32-bit) and timeout monitoring.  Our 4-bit counter wraps at 15,
 * which is adequate for teaching but would be too short for production.
 * ───────────────────────────────────────────────────────────────────────── */
typedef struct {
    uint8_t data_id;
    uint8_t counter;    /* wraps 0..15 */
} safe_e2e_ctx_t;

/* ─── API ───────────────────────────────────────────────────────────────── */

/* Protect: build E2E header and prepend to payload.
 * out must be at least len + SAFE_E2E_HEADER_SIZE bytes.
 * Returns total frame length on success, 0 on error.
 * Counter is auto-incremented (wrapping at 15). */
size_t safe_e2e_protect(safe_e2e_ctx_t *ctx,
                        const uint8_t  *data,
                        size_t          len,
                        uint8_t        *out);

/* Check: validate E2E header against payload.
 * buf = complete frame (header + payload), total_len = frame length.
 * On success, copies payload to data_out (must be at least total_len-2 bytes)
 * and sets *data_len.  Returns SAFE_OK or a specific error code.
 *
 * Counter validation: the received counter must be STRICTLY greater than
 * the last valid counter (modulo 16, with wrap detection).  Equal or
 * regressed counters are flagged as SAFE_COUNTER_ERROR. */
safe_status_t safe_e2e_check(safe_e2e_ctx_t *ctx,
                             const uint8_t  *buf,
                             size_t          total_len,
                             uint8_t        *data_out,
                             size_t         *data_len);

/* ─── Helpers (exposed for testing/diagnostics) ─────────────────────────── */
uint8_t safe_e2e_build_header_byte(uint8_t counter, uint8_t data_id);
void    safe_e2e_parse_header_byte(uint8_t byte, uint8_t *counter, uint8_t *data_id);

#endif /* SAFE_E2E_H */
