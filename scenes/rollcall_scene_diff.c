#include "../rollcall_i.h"
#include "rollcall_scene.h"

void rollcall_scene_diff_on_enter(void* context) {
    RollCall* app = context;
    diff_view_set_data(app->diff_view, &app->captures);
    view_dispatcher_switch_to_view(app->view_dispatcher, RollCallViewDiff);
}

bool rollcall_scene_diff_on_event(void* context, SceneManagerEvent event) {
    UNUSED(context);
    UNUSED(event);
    // Navigation (Back) is handled by the ViewDispatcher; the diff view only
    // consumes Left/Right for its bit cursor.
    return false;
}

void rollcall_scene_diff_on_exit(void* context) {
    UNUSED(context);
}
