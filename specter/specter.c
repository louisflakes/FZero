// Specter - real-time sub-GHz spectrum scope
//
// A CC1101-based spectrum tool built around the hardware's real constraints:
// there's no wideband/parallel acquisition, only serial per-frequency RSSI
// sampling, so the design leans on random-order bin visits (avoiding
// stroboscopic aliasing against other hopping signals), a persistence/decay
// display (since any one bin only gets a fresh sample once per sweep), and a
// tiered pan/zoom model (25/10/2.5 MHz windows) that keeps sweep time roughly
// constant across zoom levels. See specter_i.h for the fixed v1 constants.
#include "specter_i.h"
#include "scenes/specter_scene.h"
#include "views/scope_view.h"
#include "subghz/specter_scan.h"
#include <lib/subghz/devices/devices.h>

static bool specter_custom_event_callback(void* context, uint32_t event) {
    furi_assert(context);
    Specter* app = context;
    return scene_manager_handle_custom_event(app->scene_manager, event);
}

static bool specter_back_event_callback(void* context) {
    furi_assert(context);
    Specter* app = context;
    return scene_manager_handle_back_event(app->scene_manager);
}

static Specter* specter_alloc(void) {
    Specter* app = malloc(sizeof(Specter));

    // Own the sub-GHz device registry for the app lifetime; the scan engine
    // just looks the device up.
    subghz_devices_init();

    app->band_select = SpecterBandSelect779_928;
    app->decay = SpecterDecayMedium;

    app->gui = furi_record_open(RECORD_GUI);

    app->view_dispatcher = view_dispatcher_alloc();
    app->scene_manager = scene_manager_alloc(&specter_scene_handlers, app);

    view_dispatcher_set_event_callback_context(app->view_dispatcher, app);
    view_dispatcher_set_custom_event_callback(app->view_dispatcher, specter_custom_event_callback);
    view_dispatcher_set_navigation_event_callback(
        app->view_dispatcher, specter_back_event_callback);

    app->widget = widget_alloc();
    app->variable_item_list = variable_item_list_alloc();
    app->scope_view = scope_view_alloc();
    app->scan = specter_scan_alloc();
    app->scan_timer = NULL;

    view_dispatcher_add_view(
        app->view_dispatcher, SpecterViewWidget, widget_get_view(app->widget));
    view_dispatcher_add_view(
        app->view_dispatcher,
        SpecterViewVariableItemList,
        variable_item_list_get_view(app->variable_item_list));
    view_dispatcher_add_view(
        app->view_dispatcher, SpecterViewScope, scope_view_get_view(app->scope_view));

    view_dispatcher_attach_to_gui(app->view_dispatcher, app->gui, ViewDispatcherTypeFullscreen);

    return app;
}

static void specter_free(Specter* app) {
    furi_assert(app);

    view_dispatcher_remove_view(app->view_dispatcher, SpecterViewWidget);
    view_dispatcher_remove_view(app->view_dispatcher, SpecterViewVariableItemList);
    view_dispatcher_remove_view(app->view_dispatcher, SpecterViewScope);

    widget_free(app->widget);
    variable_item_list_free(app->variable_item_list);
    scope_view_free(app->scope_view);
    specter_scan_free(app->scan);

    scene_manager_free(app->scene_manager);
    view_dispatcher_free(app->view_dispatcher);

    furi_record_close(RECORD_GUI);
    subghz_devices_deinit();
    free(app);
}

int32_t specter_app(void* p) {
    UNUSED(p);
    Specter* app = specter_alloc();

    scene_manager_next_scene(app->scene_manager, SpecterSceneConfig);
    view_dispatcher_run(app->view_dispatcher);

    specter_free(app);
    return 0;
}
