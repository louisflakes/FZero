// RollCall - Sub-GHz signal field analyzer
//
// Capture the same button several times (live or from .sub files) and diff the
// decoded payloads bit-by-bit to reveal which fields are fixed and which change
// (counter vs. rolling/random). An analysis tool, not an attack tool.
#include "rollcall_i.h"
#include "scenes/rollcall_scene.h"
#include <lib/subghz/devices/devices.h>

#define ROLLCALL_DEFAULT_FREQUENCY 433920000UL

static bool rollcall_custom_event_callback(void* context, uint32_t event) {
    furi_assert(context);
    RollCall* app = context;
    return scene_manager_handle_custom_event(app->scene_manager, event);
}

static bool rollcall_back_event_callback(void* context) {
    furi_assert(context);
    RollCall* app = context;
    return scene_manager_handle_back_event(app->scene_manager);
}

static RollCall* rollcall_alloc(void) {
    RollCall* app = malloc(sizeof(RollCall));

    // Owns the sub-GHz device registry for the whole app lifetime so the RX and
    // TX modules can be (de)allocated independently.
    subghz_devices_init();

    app->frequency = ROLLCALL_DEFAULT_FREQUENCY;
    capture_set_reset(&app->captures);
    app->rx = NULL;
    app->rx_queue = NULL;
    app->bench_fsk = NULL;
    app->bench_fsk_queue = NULL;

    app->gui = furi_record_open(RECORD_GUI);
    app->notifications = furi_record_open(RECORD_NOTIFICATION);
    app->dialogs = furi_record_open(RECORD_DIALOGS);

    app->view_dispatcher = view_dispatcher_alloc();
    app->scene_manager = scene_manager_alloc(&rollcall_scene_handlers, app);

    view_dispatcher_set_event_callback_context(app->view_dispatcher, app);
    view_dispatcher_set_custom_event_callback(
        app->view_dispatcher, rollcall_custom_event_callback);
    view_dispatcher_set_navigation_event_callback(
        app->view_dispatcher, rollcall_back_event_callback);

    app->tx = rollcall_tx_alloc();
    app->craft_seed = 0;

    app->submenu = submenu_alloc();
    app->widget = widget_alloc();
    app->popup = popup_alloc();
    app->diff_view = diff_view_alloc();
    app->craft_view = craft_view_alloc();

    view_dispatcher_add_view(
        app->view_dispatcher, RollCallViewSubmenu, submenu_get_view(app->submenu));
    view_dispatcher_add_view(
        app->view_dispatcher, RollCallViewWidget, widget_get_view(app->widget));
    view_dispatcher_add_view(
        app->view_dispatcher, RollCallViewPopup, popup_get_view(app->popup));
    view_dispatcher_add_view(
        app->view_dispatcher, RollCallViewDiff, diff_view_get_view(app->diff_view));
    view_dispatcher_add_view(
        app->view_dispatcher, RollCallViewCraft, craft_view_get_view(app->craft_view));

    view_dispatcher_attach_to_gui(
        app->view_dispatcher, app->gui, ViewDispatcherTypeFullscreen);

    return app;
}

static void rollcall_free(RollCall* app) {
    furi_assert(app);

    view_dispatcher_remove_view(app->view_dispatcher, RollCallViewSubmenu);
    view_dispatcher_remove_view(app->view_dispatcher, RollCallViewWidget);
    view_dispatcher_remove_view(app->view_dispatcher, RollCallViewPopup);
    view_dispatcher_remove_view(app->view_dispatcher, RollCallViewDiff);
    view_dispatcher_remove_view(app->view_dispatcher, RollCallViewCraft);

    submenu_free(app->submenu);
    widget_free(app->widget);
    popup_free(app->popup);
    diff_view_free(app->diff_view);
    craft_view_free(app->craft_view);
    rollcall_tx_free(app->tx);

    scene_manager_free(app->scene_manager);
    view_dispatcher_free(app->view_dispatcher);

    furi_record_close(RECORD_DIALOGS);
    furi_record_close(RECORD_NOTIFICATION);
    furi_record_close(RECORD_GUI);

    subghz_devices_deinit();
    free(app);
}

int32_t rollcall_app(void* p) {
    UNUSED(p);
    RollCall* app = rollcall_alloc();

    scene_manager_next_scene(app->scene_manager, RollCallSceneStart);
    view_dispatcher_run(app->view_dispatcher);

    rollcall_free(app);
    return 0;
}
