// Specter scope view: renders a windowed RSSI spectrum with peak-hold
// persistence, and owns the pan/zoom/dBm-scale input.
//
// Controls (TEMPORARY test build -- hold-to-scan, see specter_scan.h):
//   OK (hold)     scan while held; releasing parks the radio idle and
//                 freezes whatever's on screen. Testing whether bounding
//                 each active burst (vs. continuous background scanning)
//                 avoids the SPI-acquire hang in furi_hal_subghz_rx().
//   Left / Right  slide the window across the band (clamped to band edges)
//   Up / Down     pan the visible dBm range up / down
//   Back          falls through to the ViewDispatcher (leaves the scene)
//
// Tier cycling (previously OK-short) is temporarily dropped since OK is now
// dedicated to hold-to-scan -- to be restored on a different input once the
// hang fix direction is confirmed.
//
// The view is the source of truth for the window; when the user pans it
// fires a callback so the scene can push the new window to the scan engine.
// The scene periodically pushes fresh sweep data in via scope_view_push_data.
#pragma once

#include <gui/view.h>
#include "../subghz/specter_scan.h"
#include "../specter_i.h"

typedef struct SpecterScopeView SpecterScopeView;

// Fired when the user changes the window (pan or tier). The scene should push
// the new window to the scan engine.
typedef void (*SpecterScopeWindowCallback)(void* ctx, uint32_t start_hz, uint32_t span_hz);

// Fired on OK press (active=true) / release (active=false). The scene should
// call specter_scan_set_active() accordingly.
typedef void (*SpecterScopeActiveCallback)(void* ctx, bool active);

SpecterScopeView* scope_view_alloc(void);
void scope_view_free(SpecterScopeView* scope);
View* scope_view_get_view(SpecterScopeView* scope);

// Configure for a scan session: band limits + persistence. Resets the window
// to the start of the band at the coarse tier.
void scope_view_configure(
    SpecterScopeView* scope,
    const SpecterBand* band,
    SpecterDecay decay);

// Read back the current window (so the scene can seed the scan engine).
void scope_view_get_window(SpecterScopeView* scope, uint32_t* start_hz, uint32_t* span_hz);

void scope_view_set_window_callback(
    SpecterScopeView* scope,
    SpecterScopeWindowCallback cb,
    void* ctx);

void scope_view_set_active_callback(
    SpecterScopeView* scope,
    SpecterScopeActiveCallback cb,
    void* ctx);

// Push a fresh sweep (from the scene's redraw timer). Ignored if the result's
// window doesn't match the current view window (stale after a pan/zoom).
// Applies persistence decay.
void scope_view_push_data(SpecterScopeView* scope, const SpecterScanResult* result);

// Diagnostic overlay (shown while no data has rendered yet). Lets us see, on
// screen, whether the worker started, whether snapshots are arriving, the
// sweep count, and the window the worker actually scanned vs. the view's.
void scope_view_set_started(SpecterScopeView* scope, bool started);
void scope_view_set_debug(
    SpecterScopeView* scope,
    bool snapshot_ok,
    uint32_t sweep_count,
    uint32_t res_start_hz,
    uint32_t res_span_hz);
