// Transmit a (possibly edited) capture. Given a template .sub path and a key,
// it copies the template, swaps in the key, and transmits it continuously while
// active (start -> hold -> stop). Region rules are enforced by the HAL, so a
// disallowed frequency fails cleanly.
#pragma once

#include <furi.h>
#include "../model/capture.h"

typedef struct RollCallTx RollCallTx;

typedef enum {
    RollCallTxIdle,
    RollCallTxSending,
    RollCallTxErrParse, // template unreadable / protocol not encodable
    RollCallTxErrRegion, // TX not allowed on this frequency here
} RollCallTxStatus;

RollCallTx* rollcall_tx_alloc(void);
void rollcall_tx_free(RollCallTx* tx);

// Begin transmitting `key` (bit_count significant bits) using `template_path`
// (a .sub file) for frequency/preset/protocol and all other fields. Repeats
// until rollcall_tx_stop(). Returns false on parse/region failure (see status).
bool rollcall_tx_start(RollCallTx* tx, const char* template_path, uint64_t key, uint16_t bit_count);
void rollcall_tx_stop(RollCallTx* tx);

RollCallTxStatus rollcall_tx_status(RollCallTx* tx);
