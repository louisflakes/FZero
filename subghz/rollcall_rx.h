// Live sub-GHz receiver: listens on a frequency, decodes known protocols, and
// hands each decoded signal back as a Capture. Decoding reuses the firmware's
// protocol registry, so anything the stock Read screen recognises works here.
//
// The callback fires on the worker thread — keep it short and thread-safe
// (the capture scene just enqueues + pokes the ViewDispatcher).
#pragma once

#include <furi.h>
#include "../model/capture.h"

typedef struct RollCallRx RollCallRx;

typedef void (*RollCallRxCallback)(void* context, const Capture* capture);

RollCallRx* rollcall_rx_alloc(void);
void rollcall_rx_free(RollCallRx* rx);

void rollcall_rx_set_callback(RollCallRx* rx, RollCallRxCallback callback, void* context);

// Begin receiving on `frequency` (Hz), OOK 650kHz preset (covers most remotes).
// Returns false if the frequency is invalid for the region/hardware.
bool rollcall_rx_start(RollCallRx* rx, uint32_t frequency);
void rollcall_rx_stop(RollCallRx* rx);
