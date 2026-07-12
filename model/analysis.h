// Bit-level analysis across a set of captures of (ideally) the same button.
//
// For every bit position we count how often it was 1. From that we classify:
//   * Fixed  - constant across every capture (serial number, button id, framing)
//   * Change - differs between captures (the interesting part)
// and, for the contiguous block of changing bits, we render a field-level
// verdict describing *how* it changes:
//   * Counter - the changing block, read as an integer, increases in capture
//               order (a rolling-code sequence counter)
//   * Random  - the changing block looks high-entropy (an encrypted rolling
//               block; you can see it but not predict it)
//   * Mixed   - changes that fit neither pattern cleanly
//
// This is an *understanding* tool, not an attack tool: it never predicts or
// forges the next code, it just shows you the structure of what you captured.
#pragma once

#include "capture.h"

typedef enum {
    BitClassFixed0, // constant 0 across all captures
    BitClassFixed1, // constant 1 across all captures
    BitClassChange, // varies between captures
} BitClass;

typedef enum {
    FieldVerdictNone, // no changing bits (all captures identical -> fixed code)
    FieldVerdictCounter,
    FieldVerdictRandom,
    FieldVerdictMixed,
} FieldVerdict;

typedef struct {
    uint16_t bit_count;
    size_t n; // number of captures analyzed
    BitClass classes[ROLLCALL_MAX_BITS];
    uint8_t ones[ROLLCALL_MAX_BITS]; // per-bit count of captures where bit == 1

    uint16_t fixed_bits;
    uint16_t changing_bits;

    // Contiguous span of changing bits, [change_lo, change_hi] inclusive in
    // display-position order. Valid only when changing_bits > 0.
    int change_lo;
    int change_hi;

    FieldVerdict verdict;
} Analysis;

// Run the classifier over `set`. Safe to call with 0 or 1 captures (the result
// then simply reports everything as fixed / nothing changing).
void analysis_run(const CaptureSet* set, Analysis* out);

// Human-readable one-liner for the field verdict, e.g. "Rolling (random)".
const char* analysis_verdict_str(FieldVerdict v);
