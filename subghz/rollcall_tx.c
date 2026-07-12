#include "rollcall_tx.h"

#include <lib/subghz/transmitter.h>
#include <lib/subghz/environment.h>
#include <lib/subghz/subghz_protocol_registry.h>
#include <lib/subghz/devices/devices.h>
#include <lib/subghz/devices/cc1101_int/cc1101_int_interconnect.h>
#include <lib/flipper_format/flipper_format.h>
#include <storage/storage.h>
#include <string.h>

// Where the outgoing (key-swapped) copy of the template is written.
#define ROLLCALL_TX_FILE EXT_PATH("subghz/.rollcall/rc_tx.sub")
#define TX_REPEAT_PERIOD_MS 20

struct RollCallTx {
    SubGhzEnvironment* environment;
    const SubGhzDevice* device;
    Storage* storage;
    FuriTimer* timer;

    FlipperFormat* ff; // open outgoing file, kept for re-deserialize on repeat
    SubGhzTransmitter* transmitter;
    RollCallTxStatus status;
    bool running;
};

// Map a saved Preset name to the device preset enum (standard presets only).
static FuriHalSubGhzPreset preset_from_name(const char* name) {
    if(strcmp(name, "FuriHalSubGhzPresetOok270Async") == 0) return FuriHalSubGhzPresetOok270Async;
    if(strcmp(name, "FuriHalSubGhzPreset2FSKDev238Async") == 0)
        return FuriHalSubGhzPreset2FSKDev238Async;
    if(strcmp(name, "FuriHalSubGhzPreset2FSKDev476Async") == 0)
        return FuriHalSubGhzPreset2FSKDev476Async;
    return FuriHalSubGhzPresetOok650Async; // default / most remotes
}

// Re-arm the upload once the current burst finishes -> continuous send.
static void rollcall_tx_timer(void* context) {
    RollCallTx* tx = context;
    if(!tx->running) return;
    if(!subghz_devices_is_async_complete_tx(tx->device)) return;

    subghz_devices_stop_async_tx(tx->device);
    flipper_format_rewind(tx->ff);
    subghz_transmitter_deserialize(tx->transmitter, tx->ff);
    subghz_devices_start_async_tx(
        tx->device, subghz_transmitter_yield, tx->transmitter);
}

RollCallTx* rollcall_tx_alloc(void) {
    RollCallTx* tx = malloc(sizeof(RollCallTx));

    tx->environment = subghz_environment_alloc();
    subghz_environment_set_protocol_registry(tx->environment, (void*)&subghz_protocol_registry);
    subghz_environment_load_keystore(tx->environment, SUBGHZ_KEYSTORE_DIR_NAME);
    subghz_environment_load_keystore(tx->environment, SUBGHZ_KEYSTORE_DIR_USER_NAME);
    subghz_environment_set_came_atomo_rainbow_table_file_name(
        tx->environment, SUBGHZ_CAME_ATOMO_DIR_NAME);
    subghz_environment_set_alutech_at_4n_rainbow_table_file_name(
        tx->environment, SUBGHZ_ALUTECH_AT_4N_DIR_NAME);
    subghz_environment_set_nice_flor_s_rainbow_table_file_name(
        tx->environment, SUBGHZ_NICE_FLOR_S_DIR_NAME);

    // Device init/deinit is owned by the app (rollcall.c); just look it up here.
    tx->device = subghz_devices_get_by_name(SUBGHZ_DEVICE_CC1101_INT_NAME);
    tx->storage = furi_record_open(RECORD_STORAGE);
    tx->timer = furi_timer_alloc(rollcall_tx_timer, FuriTimerTypePeriodic, tx);

    tx->ff = NULL;
    tx->transmitter = NULL;
    tx->status = RollCallTxIdle;
    tx->running = false;
    return tx;
}

void rollcall_tx_free(RollCallTx* tx) {
    furi_assert(tx);
    rollcall_tx_stop(tx);
    furi_timer_free(tx->timer);
    subghz_environment_free(tx->environment);
    furi_record_close(RECORD_STORAGE);
    free(tx);
}

bool rollcall_tx_start(
    RollCallTx* tx,
    const char* template_path,
    uint64_t key,
    uint16_t bit_count) {
    furi_assert(tx);
    if(tx->running) return true;

    tx->status = RollCallTxErrParse;

    // Work on a copy so the original .sub is never modified.
    storage_common_remove(tx->storage, ROLLCALL_TX_FILE);
    if(storage_common_copy(tx->storage, template_path, ROLLCALL_TX_FILE) != FSE_OK) return false;

    tx->ff = flipper_format_file_alloc(tx->storage);
    FuriString* protocol = furi_string_alloc();
    FuriString* preset_name = furi_string_alloc();
    bool ok = false;

    do {
        if(!flipper_format_file_open_existing(tx->ff, ROLLCALL_TX_FILE)) break;

        // Swap in the (possibly edited) key.
        uint8_t key_bytes[8];
        for(int i = 0; i < 8; i++) key_bytes[i] = (key >> (8 * (7 - i))) & 0xFF;
        if(!flipper_format_update_hex(tx->ff, "Key", key_bytes, 8)) break;
        (void)bit_count; // Bit is unchanged; the key width is fixed by the template

        uint32_t frequency = 0;
        flipper_format_rewind(tx->ff);
        if(!flipper_format_read_uint32(tx->ff, "Frequency", &frequency, 1)) break;
        flipper_format_rewind(tx->ff);
        if(!flipper_format_read_string(tx->ff, "Preset", preset_name)) break;
        flipper_format_rewind(tx->ff);
        if(!flipper_format_read_string(tx->ff, "Protocol", protocol)) break;

        tx->transmitter =
            subghz_transmitter_alloc_init(tx->environment, furi_string_get_cstr(protocol));
        if(!tx->transmitter) break;
        flipper_format_rewind(tx->ff);
        if(subghz_transmitter_deserialize(tx->transmitter, tx->ff) != SubGhzProtocolStatusOk)
            break;

        // Configure the radio and check the region allows TX here.
        subghz_devices_reset(tx->device);
        subghz_devices_idle(tx->device);
        subghz_devices_load_preset(
            tx->device, preset_from_name(furi_string_get_cstr(preset_name)), NULL);
        subghz_devices_set_frequency(tx->device, frequency);
        if(!subghz_devices_set_tx(tx->device)) {
            tx->status = RollCallTxErrRegion;
            break;
        }

        subghz_devices_start_async_tx(tx->device, subghz_transmitter_yield, tx->transmitter);
        tx->running = true;
        tx->status = RollCallTxSending;
        furi_timer_start(tx->timer, furi_ms_to_ticks(TX_REPEAT_PERIOD_MS));
        ok = true;
    } while(false);

    furi_string_free(protocol);
    furi_string_free(preset_name);

    if(!ok) {
        // Clean up partial state on failure.
        if(tx->transmitter) {
            subghz_transmitter_free(tx->transmitter);
            tx->transmitter = NULL;
        }
        if(tx->ff) {
            flipper_format_free(tx->ff);
            tx->ff = NULL;
        }
    }
    return ok;
}

void rollcall_tx_stop(RollCallTx* tx) {
    furi_assert(tx);
    if(!tx->running) return;
    tx->running = false;

    furi_timer_stop(tx->timer);
    subghz_devices_stop_async_tx(tx->device);
    subghz_devices_idle(tx->device);
    subghz_devices_sleep(tx->device);

    if(tx->transmitter) {
        subghz_transmitter_free(tx->transmitter);
        tx->transmitter = NULL;
    }
    if(tx->ff) {
        flipper_format_free(tx->ff);
        tx->ff = NULL;
    }
    tx->status = RollCallTxIdle;
}

RollCallTxStatus rollcall_tx_status(RollCallTx* tx) {
    furi_assert(tx);
    return tx->status;
}
