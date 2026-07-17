// Specter scope view: renders a windowed RSSI spectrum with peak-hold
// persistence, and owns the pan/zoom/dBm-scale input.
//
// Controls:
//   OK (short)    cycle tier: Coarse(25M) -> Mid(10M) -> Fine(2.5M) -> ...
//                 (keeps the window centered on the same frequency)
//   Left / Right  slide the window across the band (clamped to band edges)
//   Up / Down     pan the visible dBm range up / down
//   Back          falls through to the ViewDispatcher (leaves the scene)
//
// The view is the source of truth for the window; when the user pans/zooms it
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

// Push a fresh sweep (from the scene's redraw timer). Ignored if the result's
// window doesn't match the current view window (stale after a pan/zoom).
// Applies persistence decay.
void scope_view_push_data(SpecterScopeView* scope, const SpecterScanResult* result);
