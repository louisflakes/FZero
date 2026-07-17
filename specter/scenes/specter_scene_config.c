#include "../specter_i.h"
#include "specter_scene.h"

// Config screen: band + persistence, picked here so the scan view (limited to
// short OK / Left / Right / Up / Down) doesn't need to spend buttons on them.
// Room to add more options here later without touching scan-view controls.

#define SPECTER_CONFIG_ITEM_BAND 0
#define SPECTER_CONFIG_ITEM_DECAY 1
#define SPECTER_CONFIG_ITEM_START 2

static void specter_scene_config_band_changed(VariableItem* item) {
    Specter* app = variable_item_get_context(item);
    uint8_t index = variable_item_get_current_value_index(item);
    app->band_select = (SpecterBandSelect)index;
    variable_item_set_current_value_text(item, specter_band_label(app->band_select));
}

static void specter_scene_config_decay_changed(VariableItem* item) {
    Specter* app = variable_item_get_context(item);
    uint8_t index = variable_item_get_current_value_index(item);
    app->decay = (SpecterDecay)index;
    variable_item_set_current_value_text(item, specter_decay_label(app->decay));
}

static void specter_scene_config_enter_callback(void* context, uint32_t index) {
    Specter* app = context;
    if(index == SPECTER_CONFIG_ITEM_START) {
        view_dispatcher_send_custom_event(app->view_dispatcher, SPECTER_CONFIG_ITEM_START);
    }
}

void specter_scene_config_on_enter(void* context) {
    Specter* app = context;
    VariableItemList* list = app->variable_item_list;
    variable_item_list_reset(list);

    VariableItem* band_item = variable_item_list_add(
        list, "Band", 4, specter_scene_config_band_changed, app);
    variable_item_set_current_value_index(band_item, app->band_select);
    variable_item_set_current_value_text(band_item, specter_band_label(app->band_select));

    VariableItem* decay_item = variable_item_list_add(
        list, "Persistence", 4, specter_scene_config_decay_changed, app);
    variable_item_set_current_value_index(decay_item, app->decay);
    variable_item_set_current_value_text(decay_item, specter_decay_label(app->decay));

    variable_item_list_add(list, "Start Scan >", 1, NULL, app);

    variable_item_list_set_enter_callback(list, specter_scene_config_enter_callback, app);
    view_dispatcher_switch_to_view(app->view_dispatcher, SpecterViewVariableItemList);
}

bool specter_scene_config_on_event(void* context, SceneManagerEvent event) {
    Specter* app = context;
    bool consumed = false;
    if(event.type == SceneManagerEventTypeCustom && event.event == SPECTER_CONFIG_ITEM_START) {
        consumed = true;
        scene_manager_next_scene(app->scene_manager, SpecterSceneScan);
    }
    return consumed;
}

void specter_scene_config_on_exit(void* context) {
    Specter* app = context;
    variable_item_list_reset(app->variable_item_list);
}
