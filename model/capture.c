#include "capture.h"
#include <storage/storage.h>
#include <string.h>

void capture_set_reset(CaptureSet* set) {
    furi_assert(set);
    memset(set, 0, sizeof(CaptureSet));
}

bool capture_set_add(CaptureSet* set, const Capture* c, const char** reason) {
    furi_assert(set);
    furi_assert(c);

    if(set->count >= ROLLCALL_MAX_CAPTURES) {
        if(reason) *reason = "Capture list full";
        return false;
    }
    if(c->bit_count == 0 || c->bit_count > ROLLCALL_MAX_BITS) {
        if(reason) *reason = "Unsupported bit length";
        return false;
    }
    if(set->count > 0) {
        const Capture* first = &set->items[0];
        if(first->bit_count != c->bit_count ||
           strncmp(first->protocol, c->protocol, ROLLCALL_PROTO_LEN) != 0) {
            if(reason) *reason = "Protocol/length mismatch";
            return false;
        }
    }
    set->items[set->count++] = *c;
    return true;
}

bool capture_from_flipper_format(FlipperFormat* ff, Capture* out) {
    furi_assert(ff);
    furi_assert(out);

    bool ok = false;
    FuriString* tmp = furi_string_alloc();
    memset(out, 0, sizeof(Capture));

    do {
        flipper_format_rewind(ff);
        if(!flipper_format_read_string(ff, "Protocol", tmp)) break;
        // A RAW capture has no fixed key to diff.
        if(furi_string_equal(tmp, "RAW")) break;
        strncpy(out->protocol, furi_string_get_cstr(tmp), ROLLCALL_PROTO_LEN - 1);

        uint32_t bit = 0;
        flipper_format_rewind(ff);
        if(!flipper_format_read_uint32(ff, "Bit", &bit, 1)) break;
        if(bit == 0 || bit > ROLLCALL_MAX_BITS) break;
        out->bit_count = (uint16_t)bit;

        uint8_t key_bytes[ROLLCALL_KEY_BYTES] = {0};
        flipper_format_rewind(ff);
        if(!flipper_format_read_hex(ff, "Key", key_bytes, ROLLCALL_KEY_BYTES)) break;

        uint64_t key = 0;
        for(size_t i = 0; i < ROLLCALL_KEY_BYTES; i++) {
            key = (key << 8) | key_bytes[i];
        }
        out->key = key;
        ok = true;
    } while(false);

    flipper_format_rewind(ff);
    furi_string_free(tmp);
    return ok;
}

bool capture_load_from_sub(Storage* storage, const char* path, Capture* out) {
    furi_assert(storage);
    furi_assert(path);
    furi_assert(out);

    FlipperFormat* ff = flipper_format_file_alloc(storage);
    bool ok = false;

    do {
        if(!flipper_format_file_open_existing(ff, path)) break;
        // .sub files declare their own header; we only care about the payload keys.
        ok = capture_from_flipper_format(ff, out);
    } while(false);

    if(ok) {
        // Remember where it came from so we can transmit it later.
        strncpy(out->source_path, path, ROLLCALL_PATH_LEN - 1);
    }

    flipper_format_free(ff);
    return ok;
}
