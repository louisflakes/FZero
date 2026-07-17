// Specter acquisition engine.
//
// The CC1101 has no wideband/parallel capture -- only serial per-frequency
// RSSI sampling. One "hop" is: idle -> set_frequency (which, through the
// firmware's subghz_devices HAL, always runs a full ~720us PLL calibration)
// -> set_rx -> settle -> read RSSI. This engine walks a windowed set of bins
// in *shuffled* order each sweep (equal dwell per bin, but randomized visit
// order to avoid stroboscopic aliasing against another periodically-hopping
// emitter), on a dedicated worker thread, and publishes a completed sweep for
// the GUI to render with persistence/decay.
//
// Speed notes: bin count is bounded to the display width (not the hundreds of
// discrete frequencies the stock analyzer walks), and the per-hop settle is
// far below the stock analyzer's fixed 2ms. The per-hop full calibration is
// baked into the subghz_devices HAL; escaping it needs raw FSCAL-register
// caching (datasheet 28.2 fast-hop), a documented follow-up optimization.
#pragma once

#include <furi.h>

#define SPECTER_BINS 64

// A published sweep: one RSSI reading per bin, tagged with the window it was
// scanned at (so the GUI can ignore stale data after a pan/zoom).
typedef struct {
    float rssi[SPECTER_BINS]; // dBm; only meaningful where valid[i]
    bool valid[SPECTER_BINS]; // false = bin skipped (out of band / region)
    uint32_t start_hz; // window this sweep covers
    uint32_t span_hz;
    uint32_t sweep_count; // increments each completed sweep
    uint32_t sweep_ms; // wall time of the last full sweep (perf/tuning)
} SpecterScanResult;

typedef struct SpecterScan SpecterScan;

SpecterScan* specter_scan_alloc(void);
void specter_scan_free(SpecterScan* scan);

// Set the window to scan (thread-safe; picked up on the next sweep).
void specter_scan_set_window(SpecterScan* scan, uint32_t start_hz, uint32_t span_hz);

// Start / stop the worker thread. start() returns false if the device is
// unavailable.
bool specter_scan_start(SpecterScan* scan);
void specter_scan_stop(SpecterScan* scan);

// Copy the most recently published sweep. Thread-safe. Returns false if no
// sweep has completed yet.
bool specter_scan_snapshot(SpecterScan* scan, SpecterScanResult* out);
