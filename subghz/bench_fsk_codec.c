#include "bench_fsk_codec.h"
#include <string.h>

uint16_t bench_fsk_crc16(const uint8_t* data, size_t length) {
    uint16_t crc = 0xFFFF;
    for(size_t i = 0; i < length; i++) {
        crc ^= (uint16_t)data[i] << 8;
        for(uint8_t bit = 0; bit < 8; bit++) {
            crc = (crc & 0x8000) ? (uint16_t)((crc << 1) ^ 0x1021) : (uint16_t)(crc << 1);
        }
    }
    return crc;
}

size_t bench_fsk_build_frame(
    uint8_t type,
    uint8_t seq,
    const char* text,
    uint8_t* out,
    size_t out_size) {
    size_t payload_len = strlen(text);
    if(payload_len > BENCH_FSK_MAX_PAYLOAD) payload_len = BENCH_FSK_MAX_PAYLOAD;
    size_t total = BENCH_FSK_HEADER_LEN + payload_len + BENCH_FSK_CRC_LEN;
    if(out_size < total) return 0;

    out[0] = BENCH_FSK_MAGIC;
    out[1] = BENCH_FSK_VERSION;
    out[2] = type;
    out[3] = seq;
    out[4] = (uint8_t)payload_len;
    memcpy(&out[5], text, payload_len);

    uint16_t crc = bench_fsk_crc16(out, BENCH_FSK_HEADER_LEN + payload_len);
    out[BENCH_FSK_HEADER_LEN + payload_len] = (uint8_t)(crc >> 8);
    out[BENCH_FSK_HEADER_LEN + payload_len + 1] = (uint8_t)(crc & 0xFF);
    return total;
}

bool bench_fsk_parse_frame(const uint8_t* data, size_t length, BenchFskPacket* out) {
    if(length < BENCH_FSK_HEADER_LEN + BENCH_FSK_CRC_LEN) return false;
    if(data[0] != BENCH_FSK_MAGIC || data[1] != BENCH_FSK_VERSION) return false;

    size_t payload_len = data[4];
    if(payload_len > BENCH_FSK_MAX_PAYLOAD) return false;
    if(length != payload_len + BENCH_FSK_HEADER_LEN + BENCH_FSK_CRC_LEN) return false;

    uint16_t received_crc = ((uint16_t)data[length - 2] << 8) | data[length - 1];
    if(bench_fsk_crc16(data, length - 2) != received_crc) return false;

    out->type = data[2];
    out->seq = data[3];
    out->payload_len = (uint8_t)payload_len;
    memcpy(out->text, &data[5], payload_len);
    out->text[payload_len] = '\0';
    return true;
}

size_t bench_fsk_waveform_len(size_t frame_len) {
    return BENCH_FSK_PREAMBLE_BITS + BENCH_FSK_SYNC_BITS + 8 + frame_len * 8;
}

size_t bench_fsk_build_waveform(
    const uint8_t* frame,
    size_t frame_len,
    bool* out_levels,
    uint32_t* out_durations,
    size_t out_capacity) {
    size_t needed = bench_fsk_waveform_len(frame_len);
    if(out_capacity < needed) return 0;

    size_t idx = 0;
    bool level = true;
    for(size_t i = 0; i < BENCH_FSK_PREAMBLE_BITS; i++) {
        out_levels[idx] = level;
        out_durations[idx] = BENCH_FSK_TE_US;
        idx++;
        level = !level;
    }
    for(int b = BENCH_FSK_SYNC_BITS - 1; b >= 0; b--) {
        out_levels[idx] = ((BENCH_FSK_SYNC_WORD >> b) & 1u) != 0;
        out_durations[idx] = BENCH_FSK_TE_US;
        idx++;
    }
    // Hardware length byte: the RAK's SX1262 packet engine auto-inserts one
    // here in variablePacketLengthMode(); mimic it so the RAK's own receive()
    // (which expects the same framing symmetrically) can parse our waveform.
    for(int b = 7; b >= 0; b--) {
        out_levels[idx] = (((uint8_t)frame_len >> b) & 1u) != 0;
        out_durations[idx] = BENCH_FSK_TE_US;
        idx++;
    }
    for(size_t i = 0; i < frame_len; i++) {
        for(int b = 7; b >= 0; b--) {
            out_levels[idx] = ((frame[i] >> b) & 1u) != 0;
            out_durations[idx] = BENCH_FSK_TE_US;
            idx++;
        }
    }
    return idx;
}

void bench_fsk_decoder_reset(BenchFskDecoder* dec) {
    memset(dec, 0, sizeof(*dec));
}

static void bench_fsk_feed_synced_bit(BenchFskDecoder* dec, bool bit) {
    if(dec->inverted) bit = !bit;
    dec->cur_byte = (uint8_t)((dec->cur_byte << 1) | (bit ? 1 : 0));
    dec->cur_bit_in_byte++;
    if(dec->cur_bit_in_byte < 8) return;

    dec->bytes[dec->byte_count++] = dec->cur_byte;
    dec->cur_byte = 0;
    dec->cur_bit_in_byte = 0;

    if(dec->byte_count == BENCH_FSK_HEADER_LEN) {
        uint8_t payload_len = dec->bytes[4];
        if(dec->bytes[0] != BENCH_FSK_MAGIC || dec->bytes[1] != BENCH_FSK_VERSION ||
           payload_len > BENCH_FSK_MAX_PAYLOAD) {
            bench_fsk_decoder_reset(dec);
            return;
        }
        dec->total_len = (uint8_t)(payload_len + BENCH_FSK_HEADER_LEN + BENCH_FSK_CRC_LEN);
    }
}

bool bench_fsk_decoder_feed(
    BenchFskDecoder* dec,
    bool level,
    uint32_t duration_us,
    BenchFskPacket* out) {
    int n = (int)((duration_us + BENCH_FSK_TE_US / 2) / BENCH_FSK_TE_US);
    if(n < 1) n = 1;
    int32_t err = (int32_t)duration_us - n * BENCH_FSK_TE_US;
    if(err < 0) err = -err;
    if(n > BENCH_FSK_MAX_RUN_BITS || err > BENCH_FSK_TE_TOLERANCE_US) {
        // Noise or an inter-burst gap: drop any partial frame and keep scanning.
        bench_fsk_decoder_reset(dec);
        return false;
    }

    bool found = false;
    for(int i = 0; i < n; i++) {
        if(!dec->synced) {
            dec->shift = ((dec->shift << 1) | (level ? 1u : 0u)) & 0xFFFFu;
            if(dec->shift == BENCH_FSK_SYNC_WORD) {
                dec->synced = true;
                dec->inverted = false;
                dec->skip_bits = 8;
            } else if(dec->shift == (~BENCH_FSK_SYNC_WORD & 0xFFFFu)) {
                dec->synced = true;
                dec->inverted = true;
                dec->skip_bits = 8;
            }
        } else if(dec->skip_bits > 0) {
            // Discard the SX1262 packet engine's auto-inserted length byte;
            // it isn't part of our own frame (see bench_fsk_build_waveform).
            dec->skip_bits--;
        } else {
            bench_fsk_feed_synced_bit(dec, level);
            if(!found && dec->total_len != 0 && dec->byte_count == dec->total_len) {
                bool ok = bench_fsk_parse_frame(dec->bytes, dec->byte_count, out);
                bench_fsk_decoder_reset(dec);
                if(ok) found = true;
            }
        }
    }
    return found;
}
