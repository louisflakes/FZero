#include "../rollcall_i.h"
#include "rollcall_scene.h"

void rollcall_scene_about_on_enter(void* context) {
    RollCall* app = context;
    Widget* widget = app->widget;
    widget_reset(widget);

    widget_add_string_element(
        widget, 64, 2, AlignCenter, AlignTop, FontPrimary, "RollCall");
    widget_add_text_scroll_element(
        widget,
        0,
        14,
        128,
        50,
        "Capture the same button a few\n"
        "times (3+), then:\n\n"
        "AUTO-ANALYSIS finds a counter\n"
        "and offers the next predicted\n"
        "burst, ready to send.\n\n"
        "ANALYZE (manual) shows which\n"
        "bits are FIXED vs CHANGING.\n"
        "Left/Right move the cursor, OK\n"
        "toggles bits <-> hex, Up jumps\n"
        "to Craft.\n\n"
        "CRAFT & CALL: fixed bits are\n"
        "locked; Up/Down step the value;\n"
        "OK-tap flips a bit; HOLD OK to\n"
        "transmit (buzzes while sending).\n\n"
        "TX is region-limited. It sends\n"
        "fixed/counter codes exactly;\n"
        "encrypted rolling codes are not\n"
        "predictable from captures.");
    view_dispatcher_switch_to_view(app->view_dispatcher, RollCallViewWidget);
}

bool rollcall_scene_about_on_event(void* context, SceneManagerEvent event) {
    UNUSED(context);
    UNUSED(event);
    return false;
}

void rollcall_scene_about_on_exit(void* context) {
    RollCall* app = context;
    widget_reset(app->widget);
}
