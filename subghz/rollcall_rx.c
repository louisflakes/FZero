#include "rollcall_rx.h"

#include <lib/subghz/subghz_worker.h>
#include <lib/subghz/receiver.h>
#include <lib/subghz/environment.h>
#include <lib/subghz/subghz_protocol_registry.h>
#include <lib/subghz/devices/devices.h>
#include <lib/subghz/devices/cc1101_int/cc1101_int_interconnect.h>
#include <lib/flipper_format/flipper_format.h>

#define PRESET_NAME "FuriHalSubGhzPresetOok650Async"

struct RollCallRx {
    SubGhzEnvironment* environment;
    SubGhzReceiver* receiver;
    SubGhzWorker* worker;
    const SubGhzDevice* device;

    RollCallRxCallback callback;
    void* context;
    uint32_t frequency;
    bool running;
};

// Fires on the worker thread once a protocol has decoded a full packet.
static void rollcall_rx_decoded(
    SubGhzReceiver* receiver,
    SubGhzProtocolDecoderBase* decoder_base,
    void* context) {
    RollCallRx* rx = context;

    FlipperFormat* ff = flipper_format_string_alloc();
    SubGhzRadioPreset preset = {
        .name = furi_string_alloc_set(PRESET_NAME),
        .frequency = rx->frequency,
        .data = NULL,
        .data_size = 0,
    };

    Capture capture;
    if(subghz_protocol_decoder_base_serialize(decoder_base, ff, &preset) ==
       SubGhzProtocolStatusOk) {
        if(capture_from_flipper_format(ff, &capture) && rx->callback) {
            rx->callback(rx->context, &capture);
        }
    }

    furi_string_free(preset.name);
    flipper_format_free(ff);
    subghz_receiver_reset(receiver);
}

RollCallRx* rollcall_rx_alloc(void) {
    RollCallRx* rx = malloc(sizeof(RollCallRx));

    rx->environment = subghz_environment_alloc();
    subghz_environment_set_protocol_registry(
        rx->environment, (void*)&subghz_protocol_registry);
    // Point rolling-code decoders at their standard asset tables; harmless if
    // the files are absent (those protocols just won't fully decode).
    subghz_environment_load_keystore(rx->environment, SUBGHZ_KEYSTORE_DIR_NAME);
    subghz_environment_load_keystore(rx->environment, SUBGHZ_KEYSTORE_DIR_USER_NAME);
    subghz_environment_set_came_atomo_rainbow_table_file_name(
        rx->environment, SUBGHZ_CAME_ATOMO_DIR_NAME);
    subghz_environment_set_alutech_at_4n_rainbow_table_file_name(
        rx->environment, SUBGHZ_ALUTECH_AT_4N_DIR_NAME);
    subghz_environment_set_nice_flor_s_rainbow_table_file_name(
        rx->environment, SUBGHZ_NICE_FLOR_S_DIR_NAME);

    rx->receiver = subghz_receiver_alloc_init(rx->environment);
    subghz_receiver_set_filter(rx->receiver, SubGhzProtocolFlag_Decodable);
    subghz_receiver_set_rx_callback(rx->receiver, rollcall_rx_decoded, rx);

    rx->worker = subghz_worker_alloc();
    subghz_worker_set_overrun_callback(
        rx->worker, (SubGhzWorkerOverrunCallback)subghz_receiver_reset);
    subghz_worker_set_pair_callback(
        rx->worker, (SubGhzWorkerPairCallback)subghz_receiver_decode);
    subghz_worker_set_context(rx->worker, rx->receiver);

    subghz_devices_init();
    rx->device = subghz_devices_get_by_name(SUBGHZ_DEVICE_CC1101_INT_NAME);

    rx->callback = NULL;
    rx->context = NULL;
    rx->frequency = 0;
    rx->running = false;
    return rx;
}

void rollcall_rx_free(RollCallRx* rx) {
    furi_assert(rx);
    rollcall_rx_stop(rx);
    subghz_devices_deinit();
    subghz_worker_free(rx->worker);
    subghz_receiver_free(rx->receiver);
    subghz_environment_free(rx->environment);
    free(rx);
}

void rollcall_rx_set_callback(RollCallRx* rx, RollCallRxCallback callback, void* context) {
    furi_assert(rx);
    rx->callback = callback;
    rx->context = context;
}

bool rollcall_rx_start(RollCallRx* rx, uint32_t frequency) {
    furi_assert(rx);
    if(rx->running) return true;
    if(!rx->device) return false;
    if(!subghz_devices_is_frequency_valid(rx->device, frequency)) return false;

    rx->frequency = frequency;
    subghz_devices_reset(rx->device);
    subghz_devices_idle(rx->device);
    subghz_devices_load_preset(rx->device, FuriHalSubGhzPresetOok650Async, NULL);
    subghz_devices_set_frequency(rx->device, frequency);

    subghz_worker_start(rx->worker);
    subghz_devices_start_async_rx(rx->device, subghz_worker_rx_callback, rx->worker);
    rx->running = true;
    return true;
}

void rollcall_rx_stop(RollCallRx* rx) {
    furi_assert(rx);
    if(!rx->running) return;
    subghz_devices_stop_async_rx(rx->device);
    subghz_worker_stop(rx->worker);
    subghz_devices_idle(rx->device);
    subghz_devices_sleep(rx->device);
    rx->running = false;
}
