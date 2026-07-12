#include "../rollcall_i.h"
#include "rollcall_scene.h"
#include <stdio.h>

static void rollcall_auto_hex(uint64_t key, uint16_t bit_count, char* out, size_t out_size) {
    int nibbles = (bit_count + 3) / 4;
    uint64_t mask = (bit_count >= 64) ? ~0ull : ((1ull << bit_count) - 1);
    uint64_t v = key & mask;
    int n = 0;
    for(int i = nibbles - 1; i >= 0 && (size_t)(n + 1) < out_size; i--) {
        uint8_t nib = (v >> (i * 4)) & 0xF;
        out[n++] = (nib < 10) ? ('0' + nib) : ('A' + nib - 10);
    }
    out[n] = '\0';
}

static void rollcall_auto_button_cb(GuiButtonType result, InputType type, void* ctx) {
    RollCall* app = ctx;
    if(type != InputTypeShort) return;
    view_dispatcher_send_custom_event(
        app->view_dispatcher,
        (result == GuiButtonTypeRight) ? RollCallCustomEventAutoLoadBurst :
                                         RollCallCustomEventAutoManual);
}

void rollcall_scene_auto_on_enter(void* context) {
    RollCall* app = context;
    Widget* widget = app->widget;
    widget_reset(widget);

    if(app->captures.count == 0) {
        scene_manager_previous_scene(app->scene_manager);
        return;
    }

    Analysis analysis;
    analysis_run(&app->captures, &analysis);
    PredictResult predict;
    analysis_predict(&app->captures, &analysis, &predict);
    // Craft seed = the predicted next burst (or the identical fixed code).
    app->craft_seed = predict.next_key;

    char hex[20];
    rollcall_auto_hex(predict.next_key, analysis.bit_count, hex, sizeof(hex));

    if(predict.kind == PredictNone) {
        widget_add_string_element(
            widget, 64, 4, AlignCenter, AlignTop, FontPrimary, "No pattern");
        widget_add_string_multiline_element(
            widget, 64, 22, AlignCenter, AlignTop, FontSecondary,
            "Captures don't form a\nclean counter.");
        widget_add_button_element(
            widget, GuiButtonTypeCenter, "Manual", rollcall_auto_button_cb, app);
    } else {
        widget_add_string_element(
            widget, 64, 4, AlignCenter, AlignTop, FontPrimary, "Pattern found");

        char line[48];
        if(predict.kind == PredictCounter) {
            snprintf(line, sizeof(line), "Counter %+lld", (long long)predict.step);
        } else {
            snprintf(line, sizeof(line), "Fixed code");
        }
        widget_add_string_element(
            widget, 64, 22, AlignCenter, AlignTop, FontSecondary, line);

        char nextline[40];
        snprintf(nextline, sizeof(nextline), "next: %s", hex);
        widget_add_string_element(
            widget, 64, 34, AlignCenter, AlignTop, FontSecondary, nextline);

        if(!predict.confident) {
            widget_add_string_element(
                widget, 64, 44, AlignCenter, AlignTop, FontSecondary, "(low confidence)");
        }

        widget_add_button_element(
            widget, GuiButtonTypeLeft, "Manual", rollcall_auto_button_cb, app);
        widget_add_button_element(
            widget, GuiButtonTypeRight, "Load burst", rollcall_auto_button_cb, app);
    }

    view_dispatcher_switch_to_view(app->view_dispatcher, RollCallViewWidget);
}

bool rollcall_scene_auto_on_event(void* context, SceneManagerEvent event) {
    RollCall* app = context;
    bool consumed = false;
    if(event.type == SceneManagerEventTypeCustom) {
        if(event.event == RollCallCustomEventAutoLoadBurst) {
            scene_manager_next_scene(app->scene_manager, RollCallSceneCraft);
            consumed = true;
        } else if(event.event == RollCallCustomEventAutoManual) {
            scene_manager_next_scene(app->scene_manager, RollCallSceneDiff);
            consumed = true;
        }
    }
    return consumed;
}

void rollcall_scene_auto_on_exit(void* context) {
    RollCall* app = context;
    widget_reset(app->widget);
}
