// RollCall - Sub-GHz signal field analyzer
// Shared internal definitions: app struct, view/scene ids, config constants.
#pragma once

#include <furi.h>
#include <gui/gui.h>
#include <gui/view_dispatcher.h>
#include <gui/scene_manager.h>
#include <gui/modules/submenu.h>
#include <gui/modules/widget.h>
#include <gui/modules/popup.h>
#include <dialogs/dialogs.h>
#include <notification/notification_messages.h>

#include "model/capture.h"
#include "model/analysis.h"
#include "views/diff_view.h"
#include "views/craft_view.h"
#include "subghz/rollcall_rx.h"
#include "subghz/rollcall_tx.h"

#define ROLLCALL_TAG "RollCall"

typedef enum {
    RollCallViewSubmenu,
    RollCallViewWidget,
    RollCallViewPopup,
    RollCallViewDiff,
    RollCallViewCraft,
} RollCallView;

// Custom events routed through the ViewDispatcher / SceneManager.
typedef enum {
    // Emitted from the RX worker thread when a new signal has been decoded.
    RollCallCustomEventRxCapture = 100,
    // "Analyze" button pressed on the live capture scene.
    RollCallCustomEventCaptureAnalyze,
    // Auto-Analysis result buttons.
    RollCallCustomEventAutoLoadBurst,
    RollCallCustomEventAutoManual,
    // "Up" pressed in the diff view -> jump to Craft & Call.
    RollCallCustomEventDiffCraft,
} RollCallCustomEvent;

typedef struct {
    Gui* gui;
    NotificationApp* notifications;
    DialogsApp* dialogs;
    ViewDispatcher* view_dispatcher;
    SceneManager* scene_manager;

    // Shared GUI modules.
    Submenu* submenu;
    Widget* widget;
    Popup* popup;
    DiffView* diff_view;
    CraftView* craft_view;

    // Data model, shared across scenes.
    CaptureSet captures;

    // Live receiver + worker->GUI hand-off queue (active during capture scene).
    RollCallRx* rx;
    FuriMessageQueue* rx_queue;

    // Transmitter (app-lifetime; used by the craft scene).
    RollCallTx* tx;

    // Seed key handed to the Craft scene (predicted next burst, or last capture).
    uint64_t craft_seed;

    // Frequency used by the live capture scene (Hz).
    uint32_t frequency;
} RollCall;
