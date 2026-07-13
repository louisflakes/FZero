#include "rollcall_bench_fsk.h"

#include <lib/subghz/subghz_worker.h>
#include <lib/subghz/devices/devices.h>
#include <lib/subghz/devices/cc1101_int/cc1101_int_interconnect.h>
#include <lib/toolbox/level_duration.h>
#include <stdlib.h>
#include <string.h>

#define BENCH_FSK_FREQUENCY 915000000UL
#define BENCH_FSK_PRESET FuriHalSubGhzPreset2FSKDev238Async
// bench_fsk_waveform_len(BENCH_FSK_MAX_PACKET): preamble(16) + sync(16) +
// HW length byte(8) + BENCH_FSK_MAX_PACKET*8. Not computed via the function
// itself since this sizes a fixed struct member, not a runtime buffer.
#define BENCH_FSK_MAX_WAVEFORM (16 + 16 + 8 + BENCH_FSK_MAX_PACKET * 8)

typedef struct {
    bool levels[BENCH_FSK_MAX_WAVEFORM];
    uint32_t durations[BENCH_FSK_MAX_WAVEFORM];
    size_t count;
    size_t pos;
} BenchFskTxGen;

struct RollCallBenchFsk {
    const SubGhzDevice* device;
    SubGhzWorker* worker;
    BenchFskDecoder decoder;
    BenchFskTxGen tx_gen;

    RollCallBenchFskCallback callback;
    void* context;

    uint32_t rx_count;
    uint32_t tx_count;
    uint8_t tx_seq;
    bool running;
};

static void rollcall_bench_fsk_overrun_cb(void* context) {
    RollCallBenchFsk* bf = context;
    bench_fsk_decoder_reset(&bf->decoder);
}

// Fires on the worker thread for every raw (level, duration) pulse pair --
// same primitive every protocol decoder consumes, but framed by our own
// codec instead of the protocol registry.
static void rollcall_bench_fsk_pair_cb(void* context, bool level, uint32_t duration) {
    RollCallBenchFsk* bf = context;
    BenchFskPacket packet;
    if(bench_fsk_decoder_feed(&bf->decoder, level, duration, &packet)) {
        bf->rx_count++;
        if(bf->callback) {
            bf->callback(bf->context, &packet, subghz_devices_get_rssi(bf->device));
        }
    }
}

static LevelDuration bench_fsk_tx_yield(void* context) {
    BenchFskTxGen* gen = context;
    if(gen->pos >= gen->count) return level_duration_reset();
    LevelDuration ld = level_duration_make(gen->levels[gen->pos], gen->durations[gen->pos]);
    gen->pos++;
    return ld;
}

RollCallBenchFsk* rollcall_bench_fsk_alloc(void) {
    RollCallBenchFsk* bf = malloc(sizeof(RollCallBenchFsk));
    memset(bf, 0, sizeof(*bf));

    // Device init/deinit is owned by the app (rollcall.c); just look it up.
    bf->device = subghz_devices_get_by_name(SUBGHZ_DEVICE_CC1101_INT_NAME);

    bf->worker = subghz_worker_alloc();
    subghz_worker_set_overrun_callback(bf->worker, rollcall_bench_fsk_overrun_cb);
    subghz_worker_set_pair_callback(bf->worker, rollcall_bench_fsk_pair_cb);
    subghz_worker_set_context(bf->worker, bf);

    bench_fsk_decoder_reset(&bf->decoder);
    return bf;
}

void rollcall_bench_fsk_free(RollCallBenchFsk* bf) {
    furi_assert(bf);
    rollcall_bench_fsk_stop(bf);
    subghz_worker_free(bf->worker);
    free(bf);
}

void rollcall_bench_fsk_set_callback(
    RollCallBenchFsk* bf,
    RollCallBenchFskCallback callback,
    void* context) {
    furi_assert(bf);
    bf->callback = callback;
    bf->context = context;
}

static void rollcall_bench_fsk_enter_rx(RollCallBenchFsk* bf) {
    bench_fsk_decoder_reset(&bf->decoder);
    subghz_devices_reset(bf->device);
    subghz_devices_idle(bf->device);
    subghz_devices_load_preset(bf->device, BENCH_FSK_PRESET, NULL);
    subghz_devices_set_frequency(bf->device, BENCH_FSK_FREQUENCY);
    subghz_worker_start(bf->worker);
    subghz_devices_start_async_rx(bf->device, subghz_worker_rx_callback, bf->worker);
}

bool rollcall_bench_fsk_start(RollCallBenchFsk* bf) {
    furi_assert(bf);
    if(bf->running) return true;
    if(!bf->device) return false;
    if(!subghz_devices_is_frequency_valid(bf->device, BENCH_FSK_FREQUENCY)) return false;

    rollcall_bench_fsk_enter_rx(bf);
    bf->running = true;
    return true;
}

static void rollcall_bench_fsk_leave_rx(RollCallBenchFsk* bf) {
    subghz_devices_stop_async_rx(bf->device);
    subghz_worker_stop(bf->worker);
    subghz_devices_idle(bf->device);
}

void rollcall_bench_fsk_stop(RollCallBenchFsk* bf) {
    furi_assert(bf);
    if(!bf->running) return;
    rollcall_bench_fsk_leave_rx(bf);
    subghz_devices_sleep(bf->device);
    bf->running = false;
}

bool rollcall_bench_fsk_send_ping(RollCallBenchFsk* bf) {
    furi_assert(bf);
    if(!bf->running) return false;

    uint8_t frame[BENCH_FSK_MAX_PACKET];
    size_t frame_len =
        bench_fsk_build_frame(BENCH_FSK_TYPE_PING, bf->tx_seq, "PING", frame, sizeof(frame));
    if(frame_len == 0) return false;

    bf->tx_gen.count =
        bench_fsk_build_waveform(frame, frame_len, bf->tx_gen.levels, bf->tx_gen.durations, BENCH_FSK_MAX_WAVEFORM);
    bf->tx_gen.pos = 0;
    if(bf->tx_gen.count == 0) return false;

    // Half-duplex: leave RX, key up, send exactly one burst, return to RX.
    rollcall_bench_fsk_leave_rx(bf);

    subghz_devices_reset(bf->device);
    subghz_devices_idle(bf->device);
    subghz_devices_load_preset(bf->device, BENCH_FSK_PRESET, NULL);
    subghz_devices_set_frequency(bf->device, BENCH_FSK_FREQUENCY);

    bool sent = false;
    if(subghz_devices_set_tx(bf->device)) {
        subghz_devices_start_async_tx(bf->device, bench_fsk_tx_yield, &bf->tx_gen);
        // A full frame is ~480 bits * 208us =~ 100ms; poll for completion with
        // a generous cap so a stuck radio can't hang the UI thread forever.
        int waited_ms = 0;
        for(; waited_ms < 500; waited_ms += 5) {
            if(subghz_devices_is_async_complete_tx(bf->device)) break;
            furi_delay_ms(5);
        }
        subghz_devices_stop_async_tx(bf->device);
        FURI_LOG_I(
            "RollCallBenchFsk",
            "tx: streamed %u/%u entries in ~%dms",
            (unsigned)bf->tx_gen.pos,
            (unsigned)bf->tx_gen.count,
            waited_ms);
        bf->tx_seq++;
        bf->tx_count++;
        sent = true;
    }

    rollcall_bench_fsk_enter_rx(bf);
    return sent;
}

uint32_t rollcall_bench_fsk_rx_count(const RollCallBenchFsk* bf) {
    furi_assert(bf);
    return bf->rx_count;
}

uint32_t rollcall_bench_fsk_tx_count(const RollCallBenchFsk* bf) {
    furi_assert(bf);
    return bf->tx_count;
}
