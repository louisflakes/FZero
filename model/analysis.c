#include "analysis.h"
#include <string.h>

const char* analysis_verdict_str(FieldVerdict v) {
    switch(v) {
    case FieldVerdictCounter:
        return "Counter";
    case FieldVerdictRandom:
        return "Rolling (random)";
    case FieldVerdictMixed:
        return "Mixed / unclear";
    case FieldVerdictNone:
    default:
        return "Fixed code";
    }
}

// Read the changing block [lo,hi] of capture `c` as an integer, MSB-first.
static uint64_t field_value(const Capture* c, int lo, int hi) {
    uint64_t v = 0;
    for(int p = lo; p <= hi; p++) {
        v = (v << 1) | capture_bit(c, (uint16_t)p);
    }
    return v;
}

// Does the changing block increase strictly in capture order? Rolling-code
// counters increment by one per press; we allow any positive step (presses may
// be missed) but require a strict, non-wrapping increase across the set.
static bool block_is_counter(const CaptureSet* set, int lo, int hi) {
    if(set->count < 3) return false; // too few points to trust a trend
    uint64_t prev = field_value(&set->items[0], lo, hi);
    for(size_t i = 1; i < set->count; i++) {
        uint64_t cur = field_value(&set->items[i], lo, hi);
        if(cur <= prev) return false;
        prev = cur;
    }
    return true;
}

void analysis_run(const CaptureSet* set, Analysis* out) {
    memset(out, 0, sizeof(Analysis));
    out->change_lo = -1;
    out->change_hi = -1;

    if(!set || set->count == 0) {
        out->verdict = FieldVerdictNone;
        return;
    }

    out->n = set->count;
    out->bit_count = set->items[0].bit_count;

    for(uint16_t p = 0; p < out->bit_count; p++) {
        uint8_t ones = 0;
        for(size_t i = 0; i < set->count; i++) {
            ones += capture_bit(&set->items[i], p);
        }
        out->ones[p] = ones;

        if(ones == 0) {
            out->classes[p] = BitClassFixed0;
            out->fixed_bits++;
        } else if(ones == set->count) {
            out->classes[p] = BitClassFixed1;
            out->fixed_bits++;
        } else {
            out->classes[p] = BitClassChange;
            out->changing_bits++;
            if(out->change_lo < 0) out->change_lo = p;
            out->change_hi = p;
        }
    }

    if(out->changing_bits == 0) {
        out->verdict = FieldVerdictNone;
        return;
    }

    int lo = out->change_lo;
    int hi = out->change_hi;
    int span = hi - lo + 1;

    if(block_is_counter(set, lo, hi)) {
        out->verdict = FieldVerdictCounter;
        return;
    }

    // Entropy heuristic: in a high-entropy (encrypted) block roughly half of
    // the captures set each bit. If most changing bits sit near that midpoint
    // and the block is wide, call it random; otherwise it's mixed structure.
    int near_half = 0;
    for(int p = lo; p <= hi; p++) {
        if(out->classes[p] != BitClassChange) continue;
        // ones/n close to 0.5 -> |2*ones - n| small relative to n
        int deviation = (int)(2 * out->ones[p]) - (int)out->n;
        if(deviation < 0) deviation = -deviation;
        if(deviation * 2 <= (int)out->n) near_half++; // within 25%..75%
    }

    if(span >= 8 && near_half * 2 >= out->changing_bits) {
        out->verdict = FieldVerdictRandom;
    } else {
        out->verdict = FieldVerdictMixed;
    }
}

// Significant-bits mask for a key of `bit_count` bits.
static uint64_t key_mask(uint16_t bit_count) {
    return (bit_count >= 64) ? ~0ull : ((1ull << bit_count) - 1);
}

void analysis_predict(const CaptureSet* set, const Analysis* a, PredictResult* out) {
    memset(out, 0, sizeof(PredictResult));
    out->kind = PredictNone;
    out->field_lo = -1;
    out->field_hi = -1;

    if(!set || set->count == 0) return;

    const Capture* last = &set->items[set->count - 1];
    uint64_t mask = key_mask(a->bit_count);
    out->next_key = last->key & mask;
    out->confident = set->count >= 3;

    // Fixed code: nothing changes -> the "next" burst is identical.
    if(a->changing_bits == 0) {
        out->kind = PredictFixed;
        return;
    }

    // Need at least two samples to measure a step.
    if(set->count < 2) return;
    out->field_lo = a->change_lo;
    out->field_hi = a->change_hi;

    // Treat the whole significant key as an integer: a counter advances it by a
    // single consistent, non-zero step. Working on the full value (not just the
    // observed-changing bits) makes carries propagate correctly (0x7F->0x80) and
    // gives the true step even when a low bit stays fixed (0x10,0x12,0x14 -> +2).
    int64_t step = (int64_t)((set->items[1].key & mask) - (set->items[0].key & mask));
    if(step == 0) return;
    for(size_t i = 2; i < set->count; i++) {
        int64_t d = (int64_t)((set->items[i].key & mask) - (set->items[i - 1].key & mask));
        if(d != step) return; // irregular -> not a clean counter
    }

    out->kind = PredictCounter;
    out->step = step;
    out->next_key = (last->key + (uint64_t)step) & mask;
}
