#include "specter_scan.h"

#include <lib/subghz/devices/devices.h>
#include <lib/subghz/devices/cc1101_int/cc1101_int_interconnect.h>
#include <furi_hal_random.h>
#include <string.h>

// Per-hop RX settle before reading RSSI (us). Matches the stock frequency
// analyzer's proven-safe value. An earlier, much more aggressive 200us
// setting hung the device solid (no crash, no log output) after ~30+ sweeps
// of sustained idle/retune/RX cycling on real hardware -- likely wedging the
// CC1101 into a state where the firmware's internal "wait for IDLE" poll
// spins forever (no hard timeout on that path). Reliability beats raw sweep
// speed; re-tune down from here only with real stability testing, not a
// blind guess.
#define SPECTER_SETTLE_US 2000

// Below this the reading is treated as "no data" rather than a real floor.
#define SPECTER_RSSI_FLOOR -127.0f

struct SpecterScan {
    const SubGhzDevice* device;
    FuriThread* thread;
    FuriMutex* mutex; // guards window_* and published
    volatile bool running;

    // Requested window (written by GUI, read by worker at each sweep start).
    uint32_t window_start_hz;
    uint32_t window_span_hz;

    // Most recently completed sweep (written by worker, read by GUI).
    SpecterScanResult published;
    bool has_published;

    uint32_t rng_state;
};

// Pick the widest-appropriate standard OOK preset for a given window span, so
// each bin's channel filter roughly matches its width. Narrower spans want
// narrower filters for resolution; the narrowest *standard* preset is 270kHz
// (a custom ~58kHz register set is a follow-up, matching the stock analyzer's
// fine stage).
static FuriHalSubGhzPreset specter_preset_for_span(uint32_t span_hz) {
    if(span_hz <= 3000000UL) return FuriHalSubGhzPresetOok270Async; // fine (2.5 MHz)
    return FuriHalSubGhzPresetOok650Async; // coarse / mid
}

static inline uint32_t specter_xorshift(uint32_t* s) {
    uint32_t x = *s;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    *s = x;
    return x;
}

// Center frequency of bin i within [start, start+span).
static inline uint32_t specter_bin_freq(uint32_t start_hz, uint32_t span_hz, uint32_t i) {
    // start + (i + 0.5) * span / BINS, integer math, 64-bit intermediate.
    uint64_t num = (uint64_t)span_hz * (2 * (uint64_t)i + 1);
    return start_hz + (uint32_t)(num / (2 * SPECTER_BINS));
}

static int32_t specter_scan_thread(void* context) {
    SpecterScan* scan = context;
    FURI_LOG_I(
        "SpecterScan",
        "scan thread start, device=%p name=%s",
        (void*)scan->device,
        scan->device ? subghz_devices_get_name(scan->device) : "NULL");

    subghz_devices_reset(scan->device);
    FURI_LOG_I("SpecterScan", "reset ok");
    subghz_devices_idle(scan->device);
    FURI_LOG_I("SpecterScan", "idle ok");

    uint8_t order[SPECTER_BINS];
    for(uint32_t i = 0; i < SPECTER_BINS; i++) order[i] = (uint8_t)i;

    uint32_t last_preset_span = 0;
    uint32_t sweep_num = 0;

    while(scan->running) {
        // Snapshot the requested window.
        furi_mutex_acquire(scan->mutex, FuriWaitForever);
        uint32_t start_hz = scan->window_start_hz;
        uint32_t span_hz = scan->window_span_hz;
        furi_mutex_release(scan->mutex);

        if(span_hz == 0) {
            furi_delay_ms(10);
            continue;
        }

        // (Re)load the preset only when the span band changes.
        if(span_hz != last_preset_span) {
            FURI_LOG_I("SpecterScan", "loading preset for span=%lu", (unsigned long)span_hz);
            subghz_devices_idle(scan->device);
            subghz_devices_load_preset(scan->device, specter_preset_for_span(span_hz), NULL);
            last_preset_span = span_hz;
            FURI_LOG_I("SpecterScan", "preset loaded");
        }

        // Fisher-Yates shuffle the visit order for this sweep.
        for(uint32_t i = SPECTER_BINS - 1; i > 0; i--) {
            uint32_t j = specter_xorshift(&scan->rng_state) % (i + 1);
            uint8_t t = order[i];
            order[i] = order[j];
            order[j] = t;
        }

        SpecterScanResult work;
        work.start_hz = start_hz;
        work.span_hz = span_hz;
        for(uint32_t i = 0; i < SPECTER_BINS; i++) {
            work.valid[i] = false;
            work.rssi[i] = SPECTER_RSSI_FLOOR;
        }

        uint32_t t_start = furi_get_tick();
        FURI_LOG_I(
            "SpecterScan",
            "sweep %lu start: window=%lu/%lu",
            (unsigned long)sweep_num,
            (unsigned long)start_hz,
            (unsigned long)span_hz);

        for(uint32_t k = 0; k < SPECTER_BINS && scan->running; k++) {
            uint32_t bin = order[k];
            uint32_t freq = specter_bin_freq(start_hz, span_hz, bin);

            // Out-of-band / region-disallowed bins are skipped, not scanned
            // (set_frequency would otherwise crash on an invalid frequency).
            if(!subghz_devices_is_frequency_valid(scan->device, freq)) {
                FURI_LOG_I("SpecterScan", "hop %lu bin=%lu freq=%lu SKIP invalid",
                    (unsigned long)k, (unsigned long)bin, (unsigned long)freq);
                continue;
            }

            FURI_LOG_I(
                "SpecterScan",
                "hop %lu bin=%lu freq=%lu",
                (unsigned long)k,
                (unsigned long)bin,
                (unsigned long)freq);

            subghz_devices_idle(scan->device);
            FURI_LOG_D("SpecterScan", "  idle ok");
            subghz_devices_set_frequency(scan->device, freq);
            FURI_LOG_D("SpecterScan", "  set_frequency ok");
            subghz_devices_set_rx(scan->device);
            FURI_LOG_D("SpecterScan", "  set_rx ok");
            furi_delay_us(SPECTER_SETTLE_US);
            FURI_LOG_D("SpecterScan", "  settle ok");

            work.rssi[bin] = subghz_devices_get_rssi(scan->device);
            FURI_LOG_D("SpecterScan", "  get_rssi ok: %d", (int)work.rssi[bin]);
            work.valid[bin] = true;
        }

        subghz_devices_idle(scan->device);
        work.sweep_ms = furi_get_tick() - t_start;
        FURI_LOG_I(
            "SpecterScan",
            "sweep %lu done: %lums",
            (unsigned long)sweep_num,
            (unsigned long)work.sweep_ms);
        sweep_num++;

        // Publish the completed sweep.
        furi_mutex_acquire(scan->mutex, FuriWaitForever);
        work.sweep_count = scan->published.sweep_count + 1;
        scan->published = work;
        scan->has_published = true;
        furi_mutex_release(scan->mutex);

        // Yield to the scheduler between sweeps. The per-hop settle uses
        // furi_delay_us (a busy-wait), so without this the loop never sleeps
        // and starves the GUI/timer threads even though it's lower priority.
        furi_delay_ms(2);
    }

    subghz_devices_idle(scan->device);
    subghz_devices_sleep(scan->device);
    return 0;
}

SpecterScan* specter_scan_alloc(void) {
    SpecterScan* scan = malloc(sizeof(SpecterScan));

    // Device init/deinit is owned by the app (specter.c); just look it up here.
    scan->device = subghz_devices_get_by_name(SUBGHZ_DEVICE_CC1101_INT_NAME);
    scan->mutex = furi_mutex_alloc(FuriMutexTypeNormal);
    scan->thread = NULL;
    scan->running = false;
    scan->window_start_hz = 0;
    scan->window_span_hz = 0;
    memset(&scan->published, 0, sizeof(scan->published));
    scan->has_published = false;
    scan->rng_state = furi_hal_random_get();
    if(scan->rng_state == 0) scan->rng_state = 0xA5A5A5A5u; // xorshift can't seed 0
    return scan;
}

void specter_scan_free(SpecterScan* scan) {
    furi_assert(scan);
    specter_scan_stop(scan);
    furi_mutex_free(scan->mutex);
    free(scan);
}

void specter_scan_set_window(SpecterScan* scan, uint32_t start_hz, uint32_t span_hz) {
    furi_assert(scan);
    furi_mutex_acquire(scan->mutex, FuriWaitForever);
    scan->window_start_hz = start_hz;
    scan->window_span_hz = span_hz;
    furi_mutex_release(scan->mutex);
}

bool specter_scan_start(SpecterScan* scan) {
    furi_assert(scan);
    if(scan->running) return true;
    if(!scan->device) return false;

    scan->running = true;
    scan->thread = furi_thread_alloc_ex("SpecterScan", 4096, specter_scan_thread, scan);
    // Below the GUI (Normal=16) so input and the redraw timer always preempt
    // the acquisition loop -- otherwise the busy-wait settle starves the UI.
    furi_thread_set_priority(scan->thread, FuriThreadPriorityLow);
    furi_thread_start(scan->thread);
    return true;
}

void specter_scan_stop(SpecterScan* scan) {
    furi_assert(scan);
    if(!scan->running) return;
    scan->running = false;
    if(scan->thread) {
        furi_thread_join(scan->thread);
        furi_thread_free(scan->thread);
        scan->thread = NULL;
    }
}

bool specter_scan_snapshot(SpecterScan* scan, SpecterScanResult* out) {
    furi_assert(scan);
    furi_mutex_acquire(scan->mutex, FuriWaitForever);
    bool ok = scan->has_published;
    if(ok) *out = scan->published;
    furi_mutex_release(scan->mutex);
    return ok;
}
