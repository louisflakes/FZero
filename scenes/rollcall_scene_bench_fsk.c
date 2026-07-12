#include "../rollcall_i.h"
#include "rollcall_scene.h"
#include <stdio.h>
#include <string.h>

// One 915.000 MHz / FM238 request-reply link to the RAK3401/RAK13302 bench
// tester (rak_fsk_benchtop/). Deliberately separate from the capture/diff/
// craft flow: this decodes our own framed packet, not a registered protocol.

typedef struct {
    BenchFskPacket packet;
    float rssi;
} BenchFskRxEvent;

static const char* bench_fsk_type_str(uint8_t type) {
    switch(type) {
    case BENCH_FSK_TYPE_PING: return "PING";
    case BENCH_FSK_TYPE_PONG: return "PONG";
    case BENCH_FSK_TYPE_HELLO: return "HELLO";
    default: return "?";
    }
}

// Fires on the worker thread: hand off to the GUI thread via the queue.
static void rollcall_scene_bench_fsk_rx_callback(
    void* context,
    const BenchFskPacket* packet,
    float rssi) {
    RollCall* app = context;
    BenchFskRxEvent event = {.packet = *packet, .rssi = rssi};
    furi_message_queue_put(app->bench_fsk_queue, &event, 0);
    view_dispatcher_send_custom_event(app->view_dispatcher, RollCallCustomEventBenchFskRx);
}

static void rollcall_scene_bench_fsk_button_callback(
    GuiButtonType result,
    InputType type,
    void* context) {
    RollCall* app = context;
    if(result == GuiButtonTypeCenter && type == InputTypeShort) {
        view_dispatcher_send_custom_event(app->view_dispatcher, RollCallCustomEventBenchFskPing);
    }
}

static void rollcall_scene_bench_fsk_redraw(RollCall* app, bool started, const char* status_note) {
    Widget* widget = app->widget;
    widget_reset(widget);

    widget_add_string_element(
        widget, 64, 2, AlignCenter, AlignTop, FontPrimary, "Bench FSK 915.000 FM238");

    if(!started) {
        widget_add_string_multiline_element(
            widget, 64, 30, AlignCenter, AlignCenter, FontSecondary,
            "Radio busy or\nfrequency invalid");
        return;
    }

    char body[160];
    uint32_t rx = rollcall_bench_fsk_rx_count(app->bench_fsk);
    uint32_t tx = rollcall_bench_fsk_tx_count(app->bench_fsk);
    if(rx == 0) {
        snprintf(
            body,
            sizeof(body),
            "RX 0  TX %lu\nWaiting for a packet...\n%s",
            (unsigned long)tx,
            status_note ? status_note : "");
    } else {
        const BenchFskPacket* last = &app->bench_fsk_last;
        snprintf(
            body,
            sizeof(body),
            "RX %lu  TX %lu\n%s seq=%u: %s\nRSSI %d dBm%s%s",
            (unsigned long)rx,
            (unsigned long)tx,
            bench_fsk_type_str(last->type),
            last->seq,
            last->text,
            (int)app->bench_fsk_last_rssi,
            status_note ? "\n" : "",
            status_note ? status_note : "");
    }
    widget_add_string_multiline_element(
        widget, 64, 32, AlignCenter, AlignCenter, FontSecondary, body);

    widget_add_button_element(
        widget, GuiButtonTypeCenter, "Send PING", rollcall_scene_bench_fsk_button_callback, app);
}

void rollcall_scene_bench_fsk_on_enter(void* context) {
    RollCall* app = context;

    app->bench_fsk_queue = furi_message_queue_alloc(8, sizeof(BenchFskRxEvent));
    app->bench_fsk = rollcall_bench_fsk_alloc();
    rollcall_bench_fsk_set_callback(app->bench_fsk, rollcall_scene_bench_fsk_rx_callback, app);
    memset(&app->bench_fsk_last, 0, sizeof(app->bench_fsk_last));
    app->bench_fsk_last_rssi = 0.0f;

    bool started = rollcall_bench_fsk_start(app->bench_fsk);
    rollcall_scene_bench_fsk_redraw(app, started, NULL);
    view_dispatcher_switch_to_view(app->view_dispatcher, RollCallViewWidget);
}

bool rollcall_scene_bench_fsk_on_event(void* context, SceneManagerEvent event) {
    RollCall* app = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        if(event.event == RollCallCustomEventBenchFskRx) {
            consumed = true;
            BenchFskRxEvent rx_event;
            bool got_one = false;
            while(furi_message_queue_get(app->bench_fsk_queue, &rx_event, 0) == FuriStatusOk) {
                app->bench_fsk_last = rx_event.packet;
                app->bench_fsk_last_rssi = rx_event.rssi;
                got_one = true;
            }
            if(got_one) notification_message(app->notifications, &sequence_blink_green_100);
            rollcall_scene_bench_fsk_redraw(app, true, NULL);
        } else if(event.event == RollCallCustomEventBenchFskPing) {
            consumed = true;
            bool sent = rollcall_bench_fsk_send_ping(app->bench_fsk);
            notification_message(
                app->notifications, sent ? &sequence_blink_blue_100 : &sequence_error);
            rollcall_scene_bench_fsk_redraw(
                app, true, sent ? "Sent PING" : "TX not allowed here");
        }
    }
    return consumed;
}

void rollcall_scene_bench_fsk_on_exit(void* context) {
    RollCall* app = context;
    if(app->bench_fsk) {
        rollcall_bench_fsk_stop(app->bench_fsk);
        rollcall_bench_fsk_free(app->bench_fsk);
        app->bench_fsk = NULL;
    }
    if(app->bench_fsk_queue) {
        furi_message_queue_free(app->bench_fsk_queue);
        app->bench_fsk_queue = NULL;
    }
    widget_reset(app->widget);
}
