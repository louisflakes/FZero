// Capture model: a single decoded sub-GHz signal reduced to the fields that
// matter for cross-capture comparison (protocol, bit length, key bytes).
//
// Both capture sources converge on this representation:
//   * live RX  -> decoder serialized to a FlipperFormat, Key/Bit/Protocol read back
//   * .sub file -> the same Key/Bit/Protocol fields read straight from disk
#pragma once

#include <furi.h>
#include <flipper_format/flipper_format.h>

// Sub-GHz key files store the key as 8 hex bytes (up to 64 significant bits).
#define ROLLCALL_KEY_BYTES  8
#define ROLLCALL_MAX_BITS   (ROLLCALL_KEY_BYTES * 8)
#define ROLLCALL_MAX_CAPTURES 16
#define ROLLCALL_PROTO_LEN  32
#define ROLLCALL_PATH_LEN   192

typedef struct {
    char protocol[ROLLCALL_PROTO_LEN];
    uint16_t bit_count; // number of significant bits (1..64)
    uint64_t key; // big-endian value; significant bits are the low `bit_count`
    // Path to the .sub file this capture came from (loaded file, or a temp file
    // written for a live capture). Used as the template when transmitting: the
    // Key is swapped in and everything else (Frequency/Preset/TE/...) is reused.
    char source_path[ROLLCALL_PATH_LEN];
} Capture;

typedef struct {
    Capture items[ROLLCALL_MAX_CAPTURES];
    size_t count;
} CaptureSet;

// Extract bit at display position `pos` (0 = left / MSB of the field).
static inline uint8_t capture_bit(const Capture* c, uint16_t pos) {
    // pos 0            -> most significant significant-bit
    // pos bit_count-1  -> least significant bit
    return (uint8_t)((c->key >> (c->bit_count - 1u - pos)) & 1u);
}

void capture_set_reset(CaptureSet* set);

// Add a capture. Returns false if the set is full, or if the capture's
// protocol/bit_count does not match captures already in the set (mixing
// protocols makes a bitwise diff meaningless). On mismatch, `*reason` (if
// non-NULL) is set to a short human-readable explanation.
bool capture_set_add(CaptureSet* set, const Capture* c, const char** reason);

// Parse a saved .sub key file into `out`. Returns false for RAW captures or
// files without a decodable Protocol/Bit/Key triple.
bool capture_load_from_sub(Storage* storage, const char* path, Capture* out);

// Read Protocol/Bit/Key out of an already-populated FlipperFormat (as produced
// by serializing a decoder). The format is left rewound on return.
bool capture_from_flipper_format(FlipperFormat* ff, Capture* out);
