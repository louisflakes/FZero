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
        "times, then Analyze to see\n"
        "which bits are FIXED and which\n"
        "CHANGE.\n\n"
        "Solid stripes = fixed (serial,\n"
        "button id). Noisy columns =\n"
        "rolling data.\n\n"
        "Verdict: Fixed code / Counter /\n"
        "Rolling (random).\n\n"
        "In the diff: Left/Right move the\n"
        "bit cursor, OK toggles bits <-> hex.\n\n"
        "Understanding tool only - it\n"
        "does not predict or forge codes.");
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
