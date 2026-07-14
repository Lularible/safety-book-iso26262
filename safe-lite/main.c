#include "safe_common.h"
#include "safe_wdg.h"
#include "safe_e2e.h"
#include "safe_queue.h"
#include "fault_inject.h"
#include <unistd.h>

/* ─── Forward declarations ──────────────────────────────────────────────── */
static void test_watchdog_baseline(void);
static void test_watchdog_timeout(void);
static void test_e2e_normal_flow(void);
static void test_e2e_crc_corruption(void);
static void test_e2e_counter_error(void);
static void test_e2e_dataid_mismatch(void);
static void test_queue_normal_flow(void);
static void test_queue_corruption(void);
static void test_queue_metadata_corruption(void);
static void test_queue_full(void);
static void test_full_integration(void);

/* ─── Watchdog timeout callback ─────────────────────────────────────────── */
static void on_timeout(int slot_id, const char *name)
{
    printf("  *** WATCHDOG TIMEOUT: slot %d (%s) unresponsive ***\n",
           slot_id, name);
}

/* ─── Status string helper ──────────────────────────────────────────────── */
static const char *status_str(safe_status_t s)
{
    switch (s) {
    case SAFE_OK:               return "OK";
    case SAFE_ERROR:            return "ERROR";
    case SAFE_CRC_MISMATCH:     return "CRC_MISMATCH";
    case SAFE_COUNTER_ERROR:    return "COUNTER_ERROR";
    case SAFE_DATAID_MISMATCH:  return "DATAID_MISMATCH";
    case SAFE_QUEUE_FULL:       return "QUEUE_FULL";
    case SAFE_QUEUE_EMPTY:      return "QUEUE_EMPTY";
    case SAFE_MAGIC_ERROR:      return "MAGIC_ERROR";
    case SAFE_TIMEOUT:          return "TIMEOUT";
    case SAFE_METADATA_CORRUPT: return "METADATA_CORRUPT";
    default:                    return "???";
    }
}

#define PASS()  printf("  [PASS]\n")
#define FAIL()  printf("  [FAIL]\n")

/* ═══════════════════════════════════════════════════════════════════════════
 * main — run all safety integration tests
 * ═════════════════════════════════════════════════════════════════════════ */
int main(void)
{
    printf("\n");
    printf("╔══════════════════════════════════════════════════════════════╗\n");
    printf("║             safe-lite — Safety Education Framework           ║\n");
    printf("╚══════════════════════════════════════════════════════════════╝\n");
    printf("\n");

    test_watchdog_baseline();
    test_watchdog_timeout();
    test_e2e_normal_flow();
    test_e2e_crc_corruption();
    test_e2e_counter_error();
    test_e2e_dataid_mismatch();
    test_queue_normal_flow();
    test_queue_corruption();
    test_queue_metadata_corruption();
    test_queue_full();
    test_full_integration();

    printf("\n═══ All tests completed ═══\n\n");
    return 0;
}

/* ─── test_watchdog_baseline ──────────────────────────────────────────────
 * Verify that a properly-checkpointed slot does NOT trigger timeout.
 * ───────────────────────────────────────────────────────────────────────── */
static void test_watchdog_baseline(void)
{
    printf("─── Test: Watchdog Baseline (no timeout) ───\n");

    safe_wdg_t wdg;
    safe_wdg_init(&wdg, on_timeout);
    safe_wdg_register_slot(&wdg, 0, "main_loop", 200, 50, 2);
    safe_wdg_register_slot(&wdg, 1, "comm_task", 500, 100, 3);
    safe_wdg_start(&wdg);

    safe_wdg_checkpoint(&wdg, 0);
    safe_wdg_checkpoint(&wdg, 1);

    /* simulate 5 service cycles — checkpoints are fresh */
    safe_status_t st = SAFE_OK;
    for (int i = 0; i < 5; i++) {
        safe_wdg_checkpoint(&wdg, 0);
        safe_wdg_checkpoint(&wdg, 1);
        usleep(30000);  /* 30ms — well within deadlines */
        st = safe_wdg_service(&wdg);
        if (st == SAFE_TIMEOUT) break;
    }
    safe_wdg_stop(&wdg);

    if (st == SAFE_OK) {
        printf("  Watchdog did NOT fire (expected): %s\n", status_str(st));
        PASS();
    } else {
        printf("  Unexpected timeout: %s\n", status_str(st));
        FAIL();
    }
}

/* ─── test_watchdog_timeout ───────────────────────────────────────────────
 * Stop checkpointing slot 1 and verify the watchdog fires.
 * ───────────────────────────────────────────────────────────────────────── */
static void test_watchdog_timeout(void)
{
    printf("─── Test: Watchdog Timeout Detection ───\n");

    safe_wdg_t wdg;
    safe_wdg_init(&wdg, on_timeout);
    safe_wdg_register_slot(&wdg, 0, "main_loop", 100, 20, 2);
    safe_wdg_register_slot(&wdg, 1, "sensor_task", 150, 30, 2);
    safe_wdg_start(&wdg);

    safe_wdg_checkpoint(&wdg, 0);
    safe_wdg_checkpoint(&wdg, 1);

    safe_status_t st = SAFE_OK;
    for (int i = 0; i < 20; i++) {
        safe_wdg_checkpoint(&wdg, 0);  /* main_loop still alive */
        /* slot 1 (sensor_task) intentionally NOT checkpointed → starvation */
        usleep(60000);  /* 60ms — accumulating sensor_task lag */
        st = safe_wdg_service(&wdg);
        if (st == SAFE_TIMEOUT) {
            break;
        }
    }
    safe_wdg_stop(&wdg);

    if (st == SAFE_TIMEOUT) {
        printf("  Watchdog fired as expected: %s\n", status_str(st));
        PASS();
    } else {
        printf("  Watchdog did NOT fire (expected TIMEOUT): %s\n", status_str(st));
        FAIL();
    }
}

/* ─── test_e2e_normal_flow ────────────────────────────────────────────────
 * Protect a message, then check it — should succeed with counter incremented.
 * ───────────────────────────────────────────────────────────────────────── */
static void test_e2e_normal_flow(void)
{
    printf("─── Test: E2E Normal Flow ───\n");

    safe_e2e_ctx_t tx_ctx = { .data_id = 5, .counter = 0 };
    safe_e2e_ctx_t rx_ctx = { .data_id = 5, .counter = 0 };

    const char *msg = "Hello Safety!";
    uint8_t frame[SAFE_E2E_MAX_DATA_LEN + SAFE_E2E_HEADER_SIZE];
    size_t  frame_len = safe_e2e_protect(&tx_ctx, (const uint8_t *)msg,
                                         strlen(msg) + 1, frame);
    printf("  Protected: %zu bytes (data=%zu + header=%d)\n",
           frame_len, strlen(msg) + 1, SAFE_E2E_HEADER_SIZE);
    printf("  TX counter: %u\n", tx_ctx.counter);

    uint8_t rx_data[SAFE_E2E_MAX_DATA_LEN];
    size_t  rx_len = 0;
    safe_status_t st = safe_e2e_check(&rx_ctx, frame, frame_len, rx_data, &rx_len);
    printf("  Check result: %s, data=\"%s\", RX counter=%u\n",
           status_str(st), rx_data, rx_ctx.counter);

    if (st == SAFE_OK && strcmp((const char *)rx_data, msg) == 0) {
        PASS();
    } else {
        FAIL();
    }
}

/* ─── test_e2e_crc_corruption ─────────────────────────────────────────────
 * Flip a bit in the protected frame and verify CRC mismatch detection.
 * ───────────────────────────────────────────────────────────────────────── */
static void test_e2e_crc_corruption(void)
{
    printf("─── Test: E2E CRC Corruption Detection ───\n");

    safe_e2e_ctx_t tx_ctx = { .data_id = 2, .counter = 3 };
    safe_e2e_ctx_t rx_ctx = { .data_id = 2, .counter = 3 };

    uint8_t data[] = { 0xAA, 0xBB, 0xCC, 0xDD };
    uint8_t frame[SAFE_E2E_MAX_DATA_LEN + SAFE_E2E_HEADER_SIZE];
    size_t  frame_len = safe_e2e_protect(&tx_ctx, data, sizeof(data), frame);

    printf("  Original frame (hex): ");
    for (size_t i = 0; i < frame_len; i++) printf("%02X ", frame[i]);
    printf("\n");

    /* inject fault: flip bit 3 of payload byte 1 (frame byte 3) */
    fault_bit_flip(frame, frame_len, (2 + 1) * 8 + 3);
    printf("  Corrupted frame  (hex): ");
    for (size_t i = 0; i < frame_len; i++) printf("%02X ", frame[i]);
    printf("\n");

    uint8_t rx_data[SAFE_E2E_MAX_DATA_LEN];
    size_t  rx_len = 0;
    safe_status_t st = safe_e2e_check(&rx_ctx, frame, frame_len, rx_data, &rx_len);
    printf("  Check result: %s\n", status_str(st));

    if (st == SAFE_CRC_MISMATCH) {
        PASS();
    } else {
        FAIL();
    }
}

/* ─── test_e2e_counter_error ──────────────────────────────────────────────
 * Force counter regression and verify it's flagged.
 * ───────────────────────────────────────────────────────────────────────── */
static void test_e2e_counter_error(void)
{
    printf("─── Test: E2E Counter Error Detection ───\n");

    safe_e2e_ctx_t tx_ctx = { .data_id = 3, .counter = 7 };
    safe_e2e_ctx_t rx_ctx = { .data_id = 3, .counter = 7 };

    uint8_t data[] = { 0x10, 0x20 };
    uint8_t frame[SAFE_E2E_MAX_DATA_LEN + SAFE_E2E_HEADER_SIZE];

    /* send message 8 → receiver advances to 8 */
    safe_e2e_protect(&tx_ctx, data, sizeof(data), frame);
    uint8_t rx_data[SAFE_E2E_MAX_DATA_LEN];
    size_t  rx_len;
    safe_e2e_check(&rx_ctx, frame, sizeof(data) + SAFE_E2E_HEADER_SIZE,
                   rx_data, &rx_len);
    printf("  After first message: TX=%u RX=%u\n", tx_ctx.counter, rx_ctx.counter);

    /* inject fault: reset TX counter (simulates sender reboot) */
    fault_counter_reset(&tx_ctx);
    printf("  After counter reset:   TX=%u RX=%u\n", tx_ctx.counter, rx_ctx.counter);

    /* send message with counter=1 — receiver should reject (not >8 in window) */
    size_t f2 = safe_e2e_protect(&tx_ctx, data, sizeof(data), frame);
    safe_status_t st = safe_e2e_check(&rx_ctx, frame, f2, rx_data, &rx_len);
    printf("  Check result: %s\n", status_str(st));

    if (st == SAFE_COUNTER_ERROR) {
        PASS();
    } else {
        FAIL();
    }
}

/* ─── test_e2e_dataid_mismatch ────────────────────────────────────────────
 * Send with DataID=4, receive expecting DataID=9 → mismatch.
 * ───────────────────────────────────────────────────────────────────────── */
static void test_e2e_dataid_mismatch(void)
{
    printf("─── Test: E2E DataID Mismatch Detection ───\n");

    safe_e2e_ctx_t tx_ctx = { .data_id = 4, .counter = 1 };
    safe_e2e_ctx_t rx_ctx = { .data_id = 9, .counter = 1 };  /* wrong ID */

    uint8_t data[] = { 0x42 };
    uint8_t frame[SAFE_E2E_MAX_DATA_LEN + SAFE_E2E_HEADER_SIZE];
    size_t frame_len = safe_e2e_protect(&tx_ctx, data, sizeof(data), frame);

    uint8_t rx_data[SAFE_E2E_MAX_DATA_LEN];
    size_t  rx_len;
    safe_status_t st = safe_e2e_check(&rx_ctx, frame, frame_len, rx_data, &rx_len);
    printf("  TX DataID=%u, RX expects DataID=%u → %s\n",
           tx_ctx.data_id, rx_ctx.data_id, status_str(st));

    if (st == SAFE_DATAID_MISMATCH) {
        PASS();
    } else {
        FAIL();
    }
}

/* ─── test_queue_normal_flow ──────────────────────────────────────────────
 * Add entries, retrieve them, verify FIFO order and CRC integrity.
 * ───────────────────────────────────────────────────────────────────────── */
static void test_queue_normal_flow(void)
{
    printf("─── Test: Safe Queue Normal Flow ───\n");

    safe_queue_t q;
    safe_queue_init(&q);

    const char *msgs[] = { "alpha", "beta", "gamma" };
    for (int i = 0; i < 3; i++) {
        safe_status_t st = safe_queue_add(&q, (const uint8_t *)msgs[i],
                                          strlen(msgs[i]) + 1);
        printf("  Add \"%s\": %s (count=%u)\n", msgs[i], status_str(st),
               safe_queue_count(&q));
    }

    for (int i = 0; i < 3; i++) {
        uint8_t buf[SAFE_QUEUE_DATA_SIZE];
        size_t  len;
        safe_status_t st = safe_queue_get(&q, buf, &len);
        printf("  Get: %s, data=\"%s\"\n", status_str(st), buf);
        if (st != SAFE_OK || strcmp((const char *)buf, msgs[i]) != 0) {
            printf("  ERROR: expected \"%s\"\n", msgs[i]);
            FAIL();
            return;
        }
    }
    PASS();
}

/* ─── test_queue_corruption ───────────────────────────────────────────────
 * Add entry, corrupt its magic number, verify detection on get.
 * ───────────────────────────────────────────────────────────────────────── */
static void test_queue_corruption(void)
{
    printf("─── Test: Safe Queue Corruption Detection ───\n");

    safe_queue_t q;
    safe_queue_init(&q);

    const char *msg = "sensitive_data";
    safe_queue_add(&q, (const uint8_t *)msg, strlen(msg) + 1);
    printf("  Added entry, count=%u\n", safe_queue_count(&q));

    /* inject fault: corrupt magic of entry at head */
    fault_queue_magic_corrupt(&q, 0);

    uint8_t buf[SAFE_QUEUE_DATA_SIZE];
    size_t  len;
    safe_status_t st = safe_queue_get(&q, buf, &len);
    printf("  Get after corruption: %s\n", status_str(st));

    if (st == SAFE_MAGIC_ERROR) {
        PASS();
    } else {
        FAIL();
    }
}

/* ─── test_queue_metadata_corruption ──────────────────────────────────────
 * Add entries, corrupt head pointer, verify validate() catches it.
 * ───────────────────────────────────────────────────────────────────────── */
static void test_queue_metadata_corruption(void)
{
    printf("─── Test: Safe Queue Metadata Corruption ───\n");

    safe_queue_t q;
    safe_queue_init(&q);

    safe_queue_add(&q, (const uint8_t *)"AAAA", 5);
    safe_queue_add(&q, (const uint8_t *)"BBBB", 5);
    printf("  Added 2 entries, count=%u, head=%u, tail=%u\n",
           safe_queue_count(&q), q.head, q.tail);

    /* inject fault: shift head pointer */
    fault_queue_metadata_corrupt(&q);
    printf("  After metadata corrupt: head=%u, tail=%u\n", q.head, q.tail);

    safe_status_t st = safe_queue_validate(&q);
    printf("  Validate result: %s\n", status_str(st));

    if (st != SAFE_OK) {
        PASS();
    } else {
        FAIL();
    }
}

/* ─── test_queue_full ─────────────────────────────────────────────────────
 * Fill queue to capacity and verify overflow is rejected.
 * ───────────────────────────────────────────────────────────────────────── */
static void test_queue_full(void)
{
    printf("─── Test: Safe Queue Full Rejection ───\n");

    safe_queue_t q;
    safe_queue_init(&q);

    safe_status_t st = SAFE_OK;
    int added = 0;
    for (int i = 0; i < SAFE_QUEUE_MAX_ENTRIES + 2; i++) {
        st = safe_queue_add(&q, (const uint8_t *)&i, sizeof(i));
        if (st == SAFE_OK) {
            added++;
        }
    }
    printf("  Attempted %d adds, accepted %d (capacity=%d)\n",
           SAFE_QUEUE_MAX_ENTRIES + 2, added, SAFE_QUEUE_MAX_ENTRIES);

    if (added == SAFE_QUEUE_MAX_ENTRIES && st == SAFE_QUEUE_FULL) {
        PASS();
    } else {
        FAIL();
    }
}

/* ─── test_full_integration ───────────────────────────────────────────────
 * Simulate a complete safety loop:
 *   watchdog monitors 3 tasks → E2E protects messages → safe queue buffers
 *   → fault injection corrupts data → safety mechanisms detect faults.
 * ───────────────────────────────────────────────────────────────────────── */
static void test_full_integration(void)
{
    printf("─── Test: Full Integration (End-to-End Safety Loop) ───\n");

    /* ── Phase 1: Initialize safety infrastructure ──────────────────── */
    safe_wdg_t wdg;
    safe_wdg_init(&wdg, on_timeout);
    safe_wdg_register_slot(&wdg, 0, "sensor",    100, 20, 3);
    safe_wdg_register_slot(&wdg, 1, "controller", 80, 15, 2);
    safe_wdg_register_slot(&wdg, 2, "actuator",  120, 30, 3);
    safe_wdg_start(&wdg);

    safe_e2e_ctx_t sensor_tx = { .data_id = 1, .counter = 0 };
    safe_e2e_ctx_t sensor_rx = { .data_id = 1, .counter = 0 };

    safe_queue_t cmd_queue;
    safe_queue_init(&cmd_queue);

    printf("  Phase 1: Infrastructure initialized.\n");

    /* ── Phase 2: Normal operation (10 cycles) ──────────────────────── */
    printf("  Phase 2: Running 10 normal cycles...\n");
    for (int cycle = 0; cycle < 10; cycle++) {
        /* sensor task check-in */
        safe_wdg_checkpoint(&wdg, 0);

        /* produce E2E-protected sensor reading */
        uint8_t reading[8];
        snprintf((char *)reading, sizeof(reading), "S%03d", cycle);
        uint8_t frame[SAFE_E2E_MAX_DATA_LEN + SAFE_E2E_HEADER_SIZE];
        size_t  f_len = safe_e2e_protect(&sensor_tx, reading,
                                         strlen((char *)reading) + 1, frame);

        /* controller task: receive + queue command */
        safe_wdg_checkpoint(&wdg, 1);
        uint8_t rx_data[SAFE_E2E_MAX_DATA_LEN];
        size_t  rx_len;
        safe_status_t e2e_st = safe_e2e_check(&sensor_rx, frame, f_len,
                                              rx_data, &rx_len);
        if (e2e_st == SAFE_OK) {
            /* generate command and enqueue */
            uint8_t cmd[SAFE_QUEUE_DATA_SIZE];
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-truncation"
            snprintf((char *)cmd, sizeof(cmd), "CMD_%s", rx_data);
#pragma GCC diagnostic pop
            safe_queue_add(&cmd_queue, cmd, strlen((char *)cmd) + 1);
        }

        /* actuator task: dequeue and execute */
        safe_wdg_checkpoint(&wdg, 2);
        uint8_t exec_cmd[SAFE_QUEUE_DATA_SIZE];
        size_t  cmd_len;
        safe_queue_get(&cmd_queue, exec_cmd, &cmd_len);

        usleep(25000);
        safe_wdg_service(&wdg);
    }
    printf("  Phase 2 complete. Queue entries remaining: %u\n",
           safe_queue_count(&cmd_queue));

    /* ── Phase 3: Inject faults and verify detection ──────────────── */
    printf("  Phase 3: Injecting faults...\n");

    /* Fault A: Corrupt E2E payload mid-flight */
    uint8_t bad_msg[] = { 0xFF, 0xFF, 0xFF, 0xFF };
    uint8_t bad_frame[SAFE_E2E_MAX_DATA_LEN + SAFE_E2E_HEADER_SIZE];
    safe_e2e_protect(&sensor_tx, bad_msg, sizeof(bad_msg), bad_frame);
    fault_bit_flip(bad_frame, sizeof(bad_msg) + SAFE_E2E_HEADER_SIZE, 10);
    uint8_t tmp[SAFE_E2E_MAX_DATA_LEN];
    size_t  tmp_len;
    safe_status_t st_a = safe_e2e_check(&sensor_rx, bad_frame,
                                         sizeof(bad_msg) + SAFE_E2E_HEADER_SIZE,
                                         tmp, &tmp_len);
    printf("    Fault A (bit-flip in E2E frame): %s\n", status_str(st_a));

    /* Fault B: Corrupt queue entry */
    uint8_t qdata[] = "Q_FAULT";
    safe_queue_add(&cmd_queue, qdata, strlen((char *)qdata) + 1);
    fault_queue_crc_corrupt(&cmd_queue,
                            (cmd_queue.head + safe_queue_count(&cmd_queue) - 1)
                            % SAFE_QUEUE_MAX_ENTRIES);
    safe_status_t st_b = safe_queue_get(&cmd_queue, tmp, &tmp_len);
    printf("    Fault B (CRC corruption in queue): %s\n", status_str(st_b));

    /* Fault C: Starve watchdog slot */
    printf("    Fault C (starving actuator checkpoint for 300ms)...\n");
    for (int i = 0; i < 10; i++) {
        safe_wdg_checkpoint(&wdg, 0);
        safe_wdg_checkpoint(&wdg, 1);
        /* slot 2 (actuator) intentionally NOT checkpointed */
        usleep(35000);
        safe_wdg_service(&wdg);
    }
    printf("    Fault C result: watchdog fired=%d\n", wdg.fired);

    safe_wdg_stop(&wdg);

    printf("\n  Integration summary:\n");
    printf("    E2E bit-flip detected:  %s\n",
           st_a == SAFE_CRC_MISMATCH ? "YES" : "NO");
    printf("    Queue CRC corrupt detected: %s\n",
           st_b == SAFE_CRC_MISMATCH ? "YES" : "NO");
    printf("    Watchdog timeout detected:  %s\n",
           wdg.fired ? "YES" : "NO");

    if (st_a == SAFE_CRC_MISMATCH && st_b == SAFE_CRC_MISMATCH && wdg.fired) {
        PASS();
    } else {
        FAIL();
    }
}
