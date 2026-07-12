#include "../rollcall_i.h"
#include "rollcall_scene.h"

// Continuous vibration while transmitting (turned on at send, off on release).
static const NotificationSequence sequence_craft_vibro_on = {
    &message_vibro_on,
    NULL,
};
static const NotificationSequence sequence_craft_vibro_off = {
    &message_vibro_off,
    NULL,
};

// OK-hold pressed: start transmitting + buzz.
static void rollcall_craft_send_start(
    void* ctx,
    const char* path,
    uint64_t key,
    uint16_t bit_count) {
    RollCall* app = ctx;
    if(rollcall_tx_start(app->tx, path, key, bit_count)) {
        craft_view_set_sending(app->craft_view, true);
        notification_message(app->notifications, &sequence_craft_vibro_on);
    } else {
        // Parse failure or region-blocked frequency.
        notification_message(app->notifications, &sequence_error);
    }
}

// OK released: stop transmitting + stop buzz.
static void rollcall_craft_send_stop(void* ctx) {
    RollCall* app = ctx;
    rollcall_tx_stop(app->tx);
    craft_view_set_sending(app->craft_view, false);
    notification_message(app->notifications, &sequence_craft_vibro_off);
}

void rollcall_scene_craft_on_enter(void* context) {
    RollCall* app = context;
    if(app->captures.count == 0) {
        scene_manager_previous_scene(app->scene_manager);
        return;
    }

    Analysis analysis;
    analysis_run(&app->captures, &analysis);
    const Capture* seed = &app->captures.items[app->captures.count - 1];

    craft_view_set_target(app->craft_view, seed, &analysis, app->craft_seed);
    craft_view_set_tx_callbacks(
        app->craft_view, rollcall_craft_send_start, rollcall_craft_send_stop, app);
    view_dispatcher_switch_to_view(app->view_dispatcher, RollCallViewCraft);
}

bool rollcall_scene_craft_on_event(void* context, SceneManagerEvent event) {
    UNUSED(context);
    UNUSED(event);
    return false;
}

void rollcall_scene_craft_on_exit(void* context) {
    RollCall* app = context;
    // Always stop TX + vibration when leaving.
    rollcall_tx_stop(app->tx);
    craft_view_set_sending(app->craft_view, false);
    notification_message(app->notifications, &sequence_craft_vibro_off);
}
