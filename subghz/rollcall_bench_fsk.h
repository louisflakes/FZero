// Bench FSK: a fixed-configuration (915.000 MHz, FM238 / 2-FSK 4.8kbps
// ±2.380371 kHz) request/reply link to the RAK3401/RAK13302 bench tester
// (see rak_fsk_benchtop/). This is deliberately separate from RollCall's
// remote-control analysis flow: it decodes our own framed packet (see
// bench_fsk_codec.h), not registered protocols, using a raw SubGhzWorker
// pair callback instead of SubGhzReceiver.
//
// Half-duplex like the physical radio: rollcall_bench_fsk_send_ping()
// briefly stops RX, transmits one PING burst, then resumes RX to listen for
// the RAK's PONG. The receive callback fires on the worker thread.
#pragma once

#include <furi.h>
#include "bench_fsk_codec.h"

typedef struct RollCallBenchFsk RollCallBenchFsk;

typedef void (*RollCallBenchFskCallback)(void* context, const BenchFskPacket* packet, float rssi);

RollCallBenchFsk* rollcall_bench_fsk_alloc(void);
void rollcall_bench_fsk_free(RollCallBenchFsk* bf);

void rollcall_bench_fsk_set_callback(
    RollCallBenchFsk* bf,
    RollCallBenchFskCallback callback,
    void* context);

// Starts listening at 915.000 MHz / FM238. Returns false if the frequency is
// invalid for the region/hardware.
bool rollcall_bench_fsk_start(RollCallBenchFsk* bf);
void rollcall_bench_fsk_stop(RollCallBenchFsk* bf);

// Sends one PING burst (blocks briefly -- a full frame is well under 200ms
// at 4.8 kbps), then resumes RX. Returns false if TX isn't region-allowed
// here, or if not currently started.
bool rollcall_bench_fsk_send_ping(RollCallBenchFsk* bf);

uint32_t rollcall_bench_fsk_rx_count(const RollCallBenchFsk* bf);
uint32_t rollcall_bench_fsk_tx_count(const RollCallBenchFsk* bf);
