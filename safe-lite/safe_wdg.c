#include "safe_wdg.h"
#include <sys/timerfd.h>
#include <time.h>
#include <unistd.h>
#include <errno.h>

/* ─── safe_wdg_init ───────────────────────────────────────────────────────
 * Allocate a CLOCK_MONOTONIC timerfd.  The timer is NOT started yet — call
 * safe_wdg_start() to arm it.  The callback is invoked if any slot exceeds
 * its deadline + tolerance for max_misses consecutive service cycles.
 *
 * PRODUCTION: This would configure a real hardware watchdog (WDG peripheral)
 * with windowed mode, early-warning interrupt, and reset escalation.
 * ───────────────────────────────────────────────────────────────────────── */
safe_status_t safe_wdg_init(safe_wdg_t *wdg, safe_wdg_timeout_cb_t cb)
{
    if (wdg == NULL) {
        return SAFE_ERROR;
    }
    memset(wdg, 0, sizeof(*wdg));
    wdg->timer_fd = timerfd_create(CLOCK_MONOTONIC, TFD_NONBLOCK);
    if (wdg->timer_fd < 0) {
        perror("timerfd_create");
        return SAFE_ERROR;
    }
    wdg->on_timeout = cb;
    return SAFE_OK;
}

/* ─── safe_wdg_register_slot ──────────────────────────────────────────────
 * Register a supervised checkpoint.  id must be 0..SAFE_WDG_MAX_SLOTS-1.
 * deadline_ms  — maximum allowed interval between checkpoints
 * tolerance_ms — extra grace period added to deadline
 * max_misses   — consecutive misses allowed before declaring timeout
 *
 * Example: deadline=100ms, tolerance=20ms, max_misses=3
 *   → timeout fires after 3×(100+20)=360ms of silence from that slot.
 * ───────────────────────────────────────────────────────────────────────── */
safe_status_t safe_wdg_register_slot(safe_wdg_t *wdg, int id,
                                     const char *name,
                                     uint32_t deadline_ms,
                                     uint32_t tolerance_ms,
                                     uint32_t max_misses)
{
    if (wdg == NULL || id < 0 || id >= SAFE_WDG_MAX_SLOTS || name == NULL) {
        return SAFE_ERROR;
    }
    safe_wdg_slot_t *s = &wdg->slots[id];
    s->name          = name;
    s->deadline_ms   = deadline_ms;
    s->tolerance_ms  = tolerance_ms;
    s->max_misses    = max_misses;
    s->miss_count    = 0;
    s->last_seen_us  = 0;
    wdg->slot_count++;
    return SAFE_OK;
}

/* ─── safe_wdg_start ──────────────────────────────────────────────────────
 * Arm the periodic timer.  interval_ms is the service polling period.
 * A short interval (10-50ms) gives responsive detection.
 * ───────────────────────────────────────────────────────────────────────── */
safe_status_t safe_wdg_start(safe_wdg_t *wdg)
{
    if (wdg == NULL || wdg->slot_count == 0) {
        return SAFE_ERROR;
    }

    struct itimerspec its;
    its.it_interval.tv_sec  = 0;
    its.it_interval.tv_nsec = 20 * 1000 * 1000;  /* 20 ms period */
    its.it_value            = its.it_interval;

    if (timerfd_settime(wdg->timer_fd, 0, &its, NULL) < 0) {
        perror("timerfd_settime");
        return SAFE_ERROR;
    }
    wdg->enabled = true;
    return SAFE_OK;
}

/* ─── safe_wdg_checkpoint ─────────────────────────────────────────────────
 * Mark slot id as "alive" at the current monotonic time.  Resets the
 * consecutive miss counter for that slot.
 * ───────────────────────────────────────────────────────────────────────── */
safe_status_t safe_wdg_checkpoint(safe_wdg_t *wdg, int id)
{
    if (wdg == NULL || id < 0 || id >= SAFE_WDG_MAX_SLOTS) {
        return SAFE_ERROR;
    }
    struct timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);
    wdg->slots[id].last_seen_us = (uint64_t)now.tv_sec * 1000000ULL
                                + (uint64_t)now.tv_nsec / 1000ULL;
    wdg->slots[id].miss_count = 0;
    return SAFE_OK;
}

/* ─── safe_wdg_service ────────────────────────────────────────────────────
 * Must be called periodically (e.g., in main loop).  Reads the timerfd to
 * drain expirations, then checks every registered slot against its deadline.
 *
 * Returns SAFE_TIMEOUT if any slot exceeded its (deadline + tolerance) for
 * max_misses consecutive service calls.
 *
 * PRODUCTION: The service function often runs in a high-priority cyclic task
 * or directly in the timer ISR.  Here we simulate it in polled mode.
 * ───────────────────────────────────────────────────────────────────────── */
safe_status_t safe_wdg_service(safe_wdg_t *wdg)
{
    if (wdg == NULL || !wdg->enabled) {
        return SAFE_NOT_INITIALIZED;
    }
    if (wdg->fired) {
        return SAFE_TIMEOUT;
    }

    uint64_t expirations;
    ssize_t  n = read(wdg->timer_fd, &expirations, sizeof(expirations));
    if (n < 0 && errno != EAGAIN) {
        perror("read(timer_fd)");
        return SAFE_ERROR;
    }

    struct timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);
    uint64_t now_us = (uint64_t)now.tv_sec * 1000000ULL
                    + (uint64_t)now.tv_nsec / 1000ULL;

    for (uint32_t i = 0; i < SAFE_WDG_MAX_SLOTS; i++) {
        safe_wdg_slot_t *s = &wdg->slots[i];
        if (s->name == NULL) {
            continue;  /* unregistered slot — skip */
        }
        if (s->last_seen_us == 0) {
            continue;  /* not yet checked in — skip first cycle */
        }

        uint64_t elapsed_ms = (now_us - s->last_seen_us) / 1000ULL;
        uint64_t limit_ms   = (uint64_t)s->deadline_ms + (uint64_t)s->tolerance_ms;

        if (elapsed_ms > limit_ms) {
            s->miss_count++;
            if (s->miss_count >= s->max_misses) {
                wdg->fired = true;
                if (wdg->on_timeout != NULL) {
                    wdg->on_timeout((int)i, s->name);
                }
                return SAFE_TIMEOUT;
            }
        } else {
            s->miss_count = 0;
        }
    }
    return SAFE_OK;
}

/* ─── safe_wdg_stop ─────────────────────────────────────────────────────── */
safe_status_t safe_wdg_stop(safe_wdg_t *wdg)
{
    if (wdg == NULL) {
        return SAFE_ERROR;
    }
    struct itimerspec its;
    memset(&its, 0, sizeof(its));
    timerfd_settime(wdg->timer_fd, 0, &its, NULL);
    close(wdg->timer_fd);
    wdg->enabled = false;
    return SAFE_OK;
}
