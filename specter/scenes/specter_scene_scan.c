#include "../specter_i.h"
#include "specter_scene.h"
#include "../views/scope_view.h"
#include "../subghz/specter_scan.h"

// Scan scene: wires the scope view (display + input) to the scan engine
// (radio worker thread). A redraw timer pulls the latest completed sweep from
// the engine and pushes it into the view (which applies persistence decay),
// then triggers a redraw -- decoupling the ~10Hz acquisition rate from the
// smoother display refresh so the persistence afterglow reads as real-time.

#define SPECTER_REDRAW_PERIOD_MS 25 // ~40 fps display refresh

// Fired by the scope view when the user pans/zooms; retarget the engine.
static void specter_scene_scan_window_cb(void* ctx, uint32_t start_hz, uint32_t span_hz) {
    Specter* app = ctx;
    specter_scan_set_window(app->scan, start_hz, span_hz);
}

static void specter_scene_scan_timer_cb(void* ctx) {
    Specter* app = ctx;
    SpecterScanResult result;
    if(specter_scan_snapshot(app->scan, &result)) {
        scope_view_push_data(app->scope_view, &result);
    }
}

void specter_scene_scan_on_enter(void* context) {
    Specter* app = context;
    const SpecterBand* band = specter_band_get(app->band_select);

    scope_view_configure(app->scope_view, band, app->decay);
    scope_view_set_window_callback(app->scope_view, specter_scene_scan_window_cb, app);

    // Seed the engine with the view's initial window, then start scanning.
    uint32_t start_hz, span_hz;
    scope_view_get_window(app->scope_view, &start_hz, &span_hz);
    specter_scan_set_window(app->scan, start_hz, span_hz);
    specter_scan_start(app->scan);

    // Redraw timer pumps engine -> view on the GUI thread.
    app->scan_timer =
        furi_timer_alloc(specter_scene_scan_timer_cb, FuriTimerTypePeriodic, app);
    furi_timer_start(app->scan_timer, furi_ms_to_ticks(SPECTER_REDRAW_PERIOD_MS));

    view_dispatcher_switch_to_view(app->view_dispatcher, SpecterViewScope);
}

bool specter_scene_scan_on_event(void* context, SceneManagerEvent event) {
    UNUSED(context);
    UNUSED(event);
    return false;
}

void specter_scene_scan_on_exit(void* context) {
    Specter* app = context;
    if(app->scan_timer) {
        furi_timer_stop(app->scan_timer);
        furi_timer_free(app->scan_timer);
        app->scan_timer = NULL;
    }
    specter_scan_stop(app->scan);
}
