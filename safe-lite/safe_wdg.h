#ifndef SAFE_WDG_H
#define SAFE_WDG_H

#include "safe_common.h"

/* ─── Checkpoint slot descriptor ──────────────────────────────────────────
 * Each slot monitors a software component's aliveness.  The component must
 * call safe_wdg_checkpoint() before its deadline expires.  If it misses
 * max_misses consecutive deadlines, the watchdog fires.
 *
 * PRODUCTION: In a real ECU, the watchdog is a hardware peripheral with an
 * independent clock source.  Here we use Linux timerfd as a teaching proxy.
 * ───────────────────────────────────────────────────────────────────────── */
typedef struct {
    const char *name;
    uint32_t    deadline_ms;
    uint32_t    tolerance_ms;
    uint32_t    max_misses;
    uint32_t    miss_count;
    uint64_t    last_seen_us;
} safe_wdg_slot_t;

typedef void (*safe_wdg_timeout_cb_t)(int slot_id, const char *name);

typedef struct {
    int                   timer_fd;
    safe_wdg_slot_t       slots[SAFE_WDG_MAX_SLOTS];
    uint32_t              slot_count;
    safe_wdg_timeout_cb_t on_timeout;
    bool                  enabled;
    bool                  fired;
} safe_wdg_t;

/* ─── API ───────────────────────────────────────────────────────────────── */
safe_status_t safe_wdg_init(safe_wdg_t *wdg, safe_wdg_timeout_cb_t cb);
safe_status_t safe_wdg_register_slot(safe_wdg_t *wdg, int id,
                                     const char *name,
                                     uint32_t deadline_ms,
                                     uint32_t tolerance_ms,
                                     uint32_t max_misses);
safe_status_t safe_wdg_start(safe_wdg_t *wdg);
safe_status_t safe_wdg_checkpoint(safe_wdg_t *wdg, int id);
safe_status_t safe_wdg_service(safe_wdg_t *wdg);
safe_status_t safe_wdg_stop(safe_wdg_t *wdg);

#endif /* SAFE_WDG_H */
