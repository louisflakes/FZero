// Craft view: edit a payload seeded from a capture (or a predicted next value),
// then transmit it. Fixed bits are shown locked; Up/Down step the value with
// carry; OK-tap flips a bit; OK-hold transmits while held.
#pragma once

#include <gui/view.h>
#include "../model/capture.h"
#include "../model/analysis.h"

typedef struct CraftView CraftView;

// Called from the view on OK-hold press / release. The scene wires these to the
// TX engine. `path` is the transmit template; `key`/`bit_count` the payload.
typedef void (*CraftSendStart)(void* ctx, const char* path, uint64_t key, uint16_t bit_count);
typedef void (*CraftSendStop)(void* ctx);

CraftView* craft_view_alloc(void);
void craft_view_free(CraftView* cv);
View* craft_view_get_view(CraftView* cv);

// Seed the editor from `seed` (protocol/path/bit_count) and `a` (fixed/changing
// map), with the working key set to `start_key` (predicted next, or seed->key).
void craft_view_set_target(
    CraftView* cv,
    const Capture* seed,
    const Analysis* a,
    uint64_t start_key);

void craft_view_set_tx_callbacks(
    CraftView* cv,
    CraftSendStart on_start,
    CraftSendStop on_stop,
    void* ctx);

// Reflect TX activity in the UI (scene updates this from the TX status).
void craft_view_set_sending(CraftView* cv, bool sending);
