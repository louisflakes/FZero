#include "../rollcall_i.h"
#include "rollcall_scene.h"
#include <stdio.h>

// Runs on the RX worker thread: hand the capture to the GUI thread via the
// queue, then poke the ViewDispatcher (both are thread-safe).
static void rollcall_scene_capture_rx_callback(void* context, const Capture* capture) {
    RollCall* app = context;
    furi_message_queue_put(app->rx_queue, capture, 0);
    view_dispatcher_send_custom_event(app->view_dispatcher, RollCallCustomEventRxCapture);
}

static void rollcall_scene_capture_button_callback(
    GuiButtonType result,
    InputType type,
    void* context) {
    RollCall* app = context;
    if(result == GuiButtonTypeCenter && type == InputTypeShort) {
        view_dispatcher_send_custom_event(
            app->view_dispatcher, RollCallCustomEventCaptureAnalyze);
    }
}

static void rollcall_scene_capture_redraw(RollCall* app, bool started) {
    Widget* widget = app->widget;
    widget_reset(widget);

    char header[48];
    snprintf(header, sizeof(header), "RX %lu.%02lu MHz",
        (unsigned long)(app->frequency / 1000000),
        (unsigned long)((app->frequency % 1000000) / 10000));
    widget_add_string_element(widget, 64, 2, AlignCenter, AlignTop, FontPrimary, header);

    if(!started) {
        widget_add_string_multiline_element(
            widget, 64, 30, AlignCenter, AlignCenter, FontSecondary,
            "Radio busy or\nfrequency invalid");
        return;
    }

    char body[96];
    if(app->captures.count == 0) {
        snprintf(body, sizeof(body), "Press the remote\nrepeatedly...");
    } else {
        const Capture* last = &app->captures.items[app->captures.count - 1];
        snprintf(
            body,
            sizeof(body),
            "Captured: %u\nLast: %s %ub",
            (unsigned)app->captures.count,
            last->protocol,
            (unsigned)last->bit_count);
    }
    widget_add_string_multiline_element(
        widget, 64, 30, AlignCenter, AlignCenter, FontSecondary, body);

    if(app->captures.count >= 1) {
        widget_add_button_element(
            widget,
            GuiButtonTypeCenter,
            "Analyze",
            rollcall_scene_capture_button_callback,
            app);
    }
}

void rollcall_scene_capture_on_enter(void* context) {
    RollCall* app = context;

    app->rx_queue = furi_message_queue_alloc(ROLLCALL_MAX_CAPTURES, sizeof(Capture));
    app->rx = rollcall_rx_alloc();
    rollcall_rx_set_callback(app->rx, rollcall_scene_capture_rx_callback, app);

    bool started = rollcall_rx_start(app->rx, app->frequency);
    rollcall_scene_capture_redraw(app, started);
    view_dispatcher_switch_to_view(app->view_dispatcher, RollCallViewWidget);
}

bool rollcall_scene_capture_on_event(void* context, SceneManagerEvent event) {
    RollCall* app = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        if(event.event == RollCallCustomEventRxCapture) {
            consumed = true;
            Capture capture;
            bool added = false;
            while(furi_message_queue_get(app->rx_queue, &capture, 0) == FuriStatusOk) {
                const char* reason = NULL;
                if(capture_set_add(&app->captures, &capture, &reason)) added = true;
            }
            if(added) notification_message(app->notifications, &sequence_blink_green_100);
            rollcall_scene_capture_redraw(app, true);
        } else if(event.event == RollCallCustomEventCaptureAnalyze) {
            consumed = true;
            scene_manager_next_scene(app->scene_manager, RollCallSceneDiff);
        }
    }
    return consumed;
}

void rollcall_scene_capture_on_exit(void* context) {
    RollCall* app = context;
    if(app->rx) {
        rollcall_rx_stop(app->rx);
        rollcall_rx_free(app->rx);
        app->rx = NULL;
    }
    if(app->rx_queue) {
        furi_message_queue_free(app->rx_queue);
        app->rx_queue = NULL;
    }
    widget_reset(app->widget);
}
