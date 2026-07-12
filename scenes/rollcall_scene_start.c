#include "../rollcall_i.h"
#include "rollcall_scene.h"
#include <storage/storage.h>
#include <stdio.h>

typedef enum {
    StartItemCaptureLive,
    StartItemLoadSub,
    StartItemAutoAnalysis,
    StartItemAnalyze,
    StartItemCraft,
    StartItemClear,
    StartItemAbout,
} StartItem;

static void rollcall_scene_start_submenu_callback(void* context, uint32_t index) {
    RollCall* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, index);
}

static void rollcall_scene_start_build(RollCall* app) {
    Submenu* submenu = app->submenu;
    submenu_reset(submenu);
    submenu_set_header(submenu, "RollCall");

    submenu_add_item(
        submenu,
        "Capture live",
        StartItemCaptureLive,
        rollcall_scene_start_submenu_callback,
        app);
    submenu_add_item(
        submenu, "Load .sub file", StartItemLoadSub, rollcall_scene_start_submenu_callback, app);

    submenu_add_item(
        submenu,
        "Auto-Analysis",
        StartItemAutoAnalysis,
        rollcall_scene_start_submenu_callback,
        app);

    char analyze_label[24];
    snprintf(analyze_label, sizeof(analyze_label), "Analyze (%u)", (unsigned)app->captures.count);
    submenu_add_item(
        submenu, analyze_label, StartItemAnalyze, rollcall_scene_start_submenu_callback, app);

    submenu_add_item(
        submenu, "Craft & Call", StartItemCraft, rollcall_scene_start_submenu_callback, app);

    submenu_add_item(
        submenu, "Clear captures", StartItemClear, rollcall_scene_start_submenu_callback, app);
    submenu_add_item(
        submenu, "About", StartItemAbout, rollcall_scene_start_submenu_callback, app);
}

// Blocking file browser; loads a .sub key file into the capture set.
static void rollcall_scene_start_load_sub(RollCall* app) {
    DialogsFileBrowserOptions options;
    dialog_file_browser_set_basic_options(&options, ".sub", NULL);
    options.base_path = EXT_PATH("subghz");

    FuriString* path = furi_string_alloc_set(EXT_PATH("subghz"));
    bool picked = dialog_file_browser_show(app->dialogs, path, path, &options);

    if(picked) {
        Storage* storage = furi_record_open(RECORD_STORAGE);
        Capture capture;
        const char* reason = NULL;
        bool ok = capture_load_from_sub(storage, furi_string_get_cstr(path), &capture) &&
                  capture_set_add(&app->captures, &capture, &reason);
        furi_record_close(RECORD_STORAGE);

        notification_message(
            app->notifications, ok ? &sequence_success : &sequence_error);
        if(!ok && reason) {
            widget_reset(app->widget);
            widget_add_string_multiline_element(
                app->widget, 64, 28, AlignCenter, AlignCenter, FontSecondary, reason);
        }
    }

    furi_string_free(path);
}

void rollcall_scene_start_on_enter(void* context) {
    RollCall* app = context;
    rollcall_scene_start_build(app);
    submenu_set_selected_item(
        app->submenu, scene_manager_get_scene_state(app->scene_manager, RollCallSceneStart));
    view_dispatcher_switch_to_view(app->view_dispatcher, RollCallViewSubmenu);
}

bool rollcall_scene_start_on_event(void* context, SceneManagerEvent event) {
    RollCall* app = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        scene_manager_set_scene_state(app->scene_manager, RollCallSceneStart, event.event);
        consumed = true;
        switch(event.event) {
        case StartItemCaptureLive:
            scene_manager_next_scene(app->scene_manager, RollCallSceneCapture);
            break;
        case StartItemLoadSub:
            rollcall_scene_start_load_sub(app);
            rollcall_scene_start_build(app);
            break;
        case StartItemAutoAnalysis:
            if(app->captures.count >= 2) {
                scene_manager_next_scene(app->scene_manager, RollCallSceneAuto);
            } else {
                notification_message(app->notifications, &sequence_error);
            }
            break;
        case StartItemAnalyze:
            if(app->captures.count >= 1) {
                scene_manager_next_scene(app->scene_manager, RollCallSceneDiff);
            } else {
                notification_message(app->notifications, &sequence_error);
            }
            break;
        case StartItemCraft:
            if(app->captures.count >= 1) {
                // Seed the editor with the most recent capture (unedited).
                app->craft_seed = app->captures.items[app->captures.count - 1].key;
                scene_manager_next_scene(app->scene_manager, RollCallSceneCraft);
            } else {
                notification_message(app->notifications, &sequence_error);
            }
            break;
        case StartItemClear:
            capture_set_reset(&app->captures);
            rollcall_scene_start_build(app);
            notification_message(app->notifications, &sequence_blink_blue_100);
            break;
        case StartItemAbout:
            scene_manager_next_scene(app->scene_manager, RollCallSceneAbout);
            break;
        default:
            consumed = false;
            break;
        }
    }
    return consumed;
}

void rollcall_scene_start_on_exit(void* context) {
    RollCall* app = context;
    submenu_reset(app->submenu);
}
