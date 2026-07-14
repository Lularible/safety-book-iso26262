#include "safe_e2e.h"

/* ─── Header byte packing ─────────────────────────────────────────────────
 * Packs Counter(4 bits, bits 7..4) and DataID(4 bits, bits 3..0) into one
 * byte.  nibble order: [C3 C2 C1 C0 D3 D2 D1 D0] (Counter in high nibble).
 * ───────────────────────────────────────────────────────────────────────── */
uint8_t safe_e2e_build_header_byte(uint8_t counter, uint8_t data_id)
{
    return (uint8_t)(((counter & 0x0FU) << 4) | (data_id & 0x0FU));
}

/* ─── Header byte unpacking ─────────────────────────────────────────────── */
void safe_e2e_parse_header_byte(uint8_t byte, uint8_t *counter, uint8_t *data_id)
{
    if (counter != NULL) {
        *counter = (byte >> 4) & 0x0FU;
    }
    if (data_id != NULL) {
        *data_id = byte & 0x0FU;
    }
}

/* ─── safe_e2e_protect ────────────────────────────────────────────────────
 * Builds a protected E2E frame:
 *   1. Increment counter (wrapping at SAFE_E2E_MAX_COUNTER, 15)
 *   2. Build synthetic buffer: [DataID | Counter | payload]
 *   3. CRC8 over synthetic buffer → byte 0 of header
 *   4. Pack Counter + DataID → byte 1 of header
 *   5. Copy payload after header
 *
 * Total output size = SAFE_E2E_HEADER_SIZE + len.
 *
 * PRODUCTION: A real E2E Protect() also computes an age counter and may
 * prepend a length field for variable-length messages (E2E Profile 4/5/7).
 * ───────────────────────────────────────────────────────────────────────── */
size_t safe_e2e_protect(safe_e2e_ctx_t *ctx,
                        const uint8_t  *data,
                        size_t          len,
                        uint8_t        *out)
{
    if (ctx == NULL || data == NULL || out == NULL) {
        return 0;
    }
    if (len > SAFE_E2E_MAX_DATA_LEN) {
        return 0;
    }

    ctx->counter = (ctx->counter + 1) & SAFE_E2E_MAX_COUNTER;

    /* synthetic buffer for CRC: [DataID | Counter | payload] */
    uint8_t crc_buf[SAFE_E2E_MAX_DATA_LEN + 2];
    crc_buf[0] = ctx->data_id;
    crc_buf[1] = ctx->counter;
    if (len > 0) {
        memcpy(&crc_buf[2], data, len);
    }

    uint8_t crc = safe_crc8_compute(crc_buf, len + 2);

    out[0] = crc;
    out[1] = safe_e2e_build_header_byte(ctx->counter, ctx->data_id);
    if (len > 0) {
        memcpy(&out[2], data, len);
    }
    return SAFE_E2E_HEADER_SIZE + len;
}

/* ─── safe_e2e_check ──────────────────────────────────────────────────────
 * Validates an incoming E2E frame:
 *   1. Parse header byte → received counter, received DataID
 *   2. Recompute CRC8 over [DataID_rx | Counter_rx | payload]
 *   3. Compare CRC (SAFE_CRC_MISMATCH if different)
 *   4. Compare DataID (SAFE_DATAID_MISMATCH if different)
 *   5. Validate counter freshness (SAFE_COUNTER_ERROR if non-increasing)
 *
 * Counter freshness logic (4-bit wrap):
 *   Let diff = (rx_counter - last_counter) & 0x0F  (modulo-16 arithmetic)
 *   Counter is "fresh" iff 1 ≤ diff ≤ 8.
 *   This handles the 0→15→14… wrap naturally: diff(15,0)=1, diff(14,15)=15.
 *   Values 9..15 are considered "too far in the past" (replay attack).
 *
 * PRODUCTION: Real E2E profiles also verify timeout (message age) using
 * a separate timer mechanism.  Counter validation alone can miss messages
 * that arrive very late.
 * ───────────────────────────────────────────────────────────────────────── */
safe_status_t safe_e2e_check(safe_e2e_ctx_t *ctx,
                             const uint8_t  *buf,
                             size_t          total_len,
                             uint8_t        *data_out,
                             size_t         *data_len)
{
    if (ctx == NULL || buf == NULL || data_out == NULL || data_len == NULL) {
        return SAFE_ERROR;
    }
    if (total_len < SAFE_E2E_HEADER_SIZE) {
        return SAFE_ERROR;
    }

    uint8_t rx_crc     = buf[0];
    uint8_t rx_counter = 0;
    uint8_t rx_dataid  = 0;
    safe_e2e_parse_header_byte(buf[1], &rx_counter, &rx_dataid);

    size_t payload_len = total_len - SAFE_E2E_HEADER_SIZE;

    /* ── CRC check ──────────────────────────────────────────────────── */
    uint8_t crc_buf[SAFE_E2E_MAX_DATA_LEN + 2];
    crc_buf[0] = rx_dataid;
    crc_buf[1] = rx_counter;
    if (payload_len > 0) {
        memcpy(&crc_buf[2], &buf[2], payload_len);
    }
    uint8_t computed_crc = safe_crc8_compute(crc_buf, payload_len + 2);
    if (computed_crc != rx_crc) {
        return SAFE_CRC_MISMATCH;
    }

    /* ── DataID check ───────────────────────────────────────────────── */
    if (rx_dataid != ctx->data_id) {
        return SAFE_DATAID_MISMATCH;
    }

    /* ── Counter freshness check ────────────────────────────────────── */
    uint8_t diff = (rx_counter - ctx->counter) & SAFE_E2E_MAX_COUNTER;
    if (diff == 0 || diff > 8) {
        return SAFE_COUNTER_ERROR;
    }

    /* ── Accept ─────────────────────────────────────────────────────── */
    ctx->counter = rx_counter;
    if (payload_len > 0) {
        memcpy(data_out, &buf[2], payload_len);
    }
    *data_len = payload_len;
    return SAFE_OK;
}
