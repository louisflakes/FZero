#include "../rollcall_i.h"
#include "rollcall_scene.h"

// Diff view "Up" -> Craft & Call (runs on the GUI thread; hand off via event).
static void rollcall_scene_diff_craft_cb(void* ctx) {
    RollCall* app = ctx;
    view_dispatcher_send_custom_event(app->view_dispatcher, RollCallCustomEventDiffCraft);
}

void rollcall_scene_diff_on_enter(void* context) {
    RollCall* app = context;
    diff_view_set_data(app->diff_view, &app->captures);
    diff_view_set_craft_callback(app->diff_view, rollcall_scene_diff_craft_cb, app);
    view_dispatcher_switch_to_view(app->view_dispatcher, RollCallViewDiff);
}

bool rollcall_scene_diff_on_event(void* context, SceneManagerEvent event) {
    RollCall* app = context;
    if(event.type == SceneManagerEventTypeCustom &&
       event.event == RollCallCustomEventDiffCraft) {
        if(app->captures.count >= 1) {
            // Seed craft with the most recent capture (unedited starting point).
            app->craft_seed = app->captures.items[app->captures.count - 1].key;
            scene_manager_next_scene(app->scene_manager, RollCallSceneCraft);
        }
        return true;
    }
    return false;
}

void rollcall_scene_diff_on_exit(void* context) {
    UNUSED(context);
}
