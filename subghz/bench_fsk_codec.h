#pragma once

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

// Wire format + software bit-sync/framing for the RAK3401/RAK13302 FSK bench
// tester (see rak_fsk_benchtop/). CC1101's FM238 preset (2-FSK, deviation
// 2.380371 kHz, ~4.79794 kBaud) runs with MDMCFG2 hardware sync/preamble
// detection DISABLED (async, continuous, no whitening) -- so unlike a
// registered remote-control protocol, nothing downstream of the radio does
// framing for us. This module does it entirely in software from the same
// (level, duration) pulse pairs every other RollCall/stock decoder consumes.
//
// Frame on air (MSB-first per byte, NRZ):
//   [16 alternating bits, radio settling] [16-bit sync 0xD391]
//   [Magic 0xA5][Version 0x01][Type][Seq][PayloadLen][payload...][CRC16 hi][CRC16 lo]
// This must match rak_fsk_benchtop/src/main.cpp's make_packet/parse_packet
// exactly -- the two are independently-implemented sides of one wire format.

#define BENCH_FSK_MAGIC 0xA5
#define BENCH_FSK_VERSION 0x01
#define BENCH_FSK_TYPE_PING 0x01
#define BENCH_FSK_TYPE_PONG 0x02
#define BENCH_FSK_TYPE_HELLO 0x03

#define BENCH_FSK_MAX_PAYLOAD 48
#define BENCH_FSK_HEADER_LEN 5 // magic, version, type, seq, length
#define BENCH_FSK_CRC_LEN 2
#define BENCH_FSK_MAX_PACKET (BENCH_FSK_HEADER_LEN + BENCH_FSK_MAX_PAYLOAD + BENCH_FSK_CRC_LEN)

#define BENCH_FSK_SYNC_WORD 0xD391u // must match SyncWord[] in main.cpp
#define BENCH_FSK_SYNC_BITS 16
#define BENCH_FSK_PREAMBLE_BITS 16

// Nominal bit period for MDMCFG3=0x83 (~4.79794 kBaud) = 1e6/4797.94 us.
#define BENCH_FSK_TE_US 208
#define BENCH_FSK_TE_TOLERANCE_US 60
// Longest plausible same-level run we'll expand from one pulse; anything
// longer is treated as an inter-burst gap, not data.
#define BENCH_FSK_MAX_RUN_BITS 40

typedef struct {
    uint8_t type;
    uint8_t seq;
    uint8_t payload_len;
    char text[BENCH_FSK_MAX_PAYLOAD + 1]; // NUL-terminated for display/printf
} BenchFskPacket;

uint16_t bench_fsk_crc16(const uint8_t* data, size_t length);

// Encodes `text` (truncated to BENCH_FSK_MAX_PAYLOAD) into `out` using the
// wire format above. Returns the number of bytes written (header + payload +
// CRC), or 0 if `out_size` is too small.
size_t bench_fsk_build_frame(
    uint8_t type,
    uint8_t seq,
    const char* text,
    uint8_t* out,
    size_t out_size);

// Parses a raw frame (as produced by bench_fsk_build_frame) into `out`.
// Returns false on length/magic/version/CRC mismatch.
bool bench_fsk_parse_frame(const uint8_t* data, size_t length, BenchFskPacket* out);

// Expands a built frame into a preamble + sync + bit-level (level, duration)
// waveform, MSB-first per byte. `out` must hold at least
// bench_fsk_waveform_len(frame_len) entries. Returns the number of entries
// written.
size_t bench_fsk_waveform_len(size_t frame_len);
size_t bench_fsk_build_waveform(
    const uint8_t* frame,
    size_t frame_len,
    bool* out_levels,
    uint32_t* out_durations,
    size_t out_capacity);

// ---- Software bit-sync / framing decoder ----
// Feed raw (level, duration) pulse pairs one at a time (exactly what
// SubGhzWorker's pair callback delivers). Returns true and fills `out` once a
// CRC-valid frame completes; resets itself automatically to search for the
// next frame either way.
typedef struct {
    uint32_t shift; // rolling window of the most recently seen bits
    bool synced;
    bool inverted;
    uint8_t cur_byte;
    uint8_t cur_bit_in_byte;
    uint8_t bytes[BENCH_FSK_MAX_PACKET];
    uint8_t byte_count;
    uint8_t total_len; // 0 until the length byte (bytes[4]) is known
} BenchFskDecoder;

void bench_fsk_decoder_reset(BenchFskDecoder* dec);
bool bench_fsk_decoder_feed(
    BenchFskDecoder* dec,
    bool level,
    uint32_t duration_us,
    BenchFskPacket* out);
