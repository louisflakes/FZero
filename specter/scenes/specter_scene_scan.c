#include "../specter_i.h"
#include "specter_scene.h"
#include <stdio.h>

// Scan view: not yet implemented. This confirms config -> scan wiring works;
// the actual acquisition engine (random-order bin sampling, tiered pan/zoom,
// persistence decay, dBm scaling) lands in a follow-up pass.
//
// Reserved controls for that pass:
//   OK (short)    cycle tier: Coarse(25MHz) -> Mid(10MHz) -> Fine(2.5MHz) -> ...
//   Left / Right  slide the current tier's window across the selected band
//   Up / Down     scale the dBm y-axis
//   Back          return to config

void specter_scene_scan_on_enter(void* context) {
    Specter* app = context;
    Widget* widget = app->widget;
    widget_reset(widget);

    widget_add_string_element(
        widget, 64, 2, AlignCenter, AlignTop, FontPrimary, "Specter");

    char body[128];
    snprintf(
        body,
        sizeof(body),
        "Band: %s\nPersistence: %s\n\nScan engine not yet built.\nBack: config",
        specter_band_label(app->band_select),
        specter_decay_label(app->decay));
    widget_add_string_multiline_element(
        widget, 64, 24, AlignCenter, AlignTop, FontSecondary, body);

    view_dispatcher_switch_to_view(app->view_dispatcher, SpecterViewWidget);
}

bool specter_scene_scan_on_event(void* context, SceneManagerEvent event) {
    UNUSED(context);
    UNUSED(event);
    return false;
}

void specter_scene_scan_on_exit(void* context) {
    Specter* app = context;
    widget_reset(app->widget);
}
