# RollCall — Sub-GHz Signal Analyzer & Craft Tool

**One-page technical overview** · Flipper Zero external app (`.fap`) · Repo: `FZero`, branch `claude/flipper-subghz-app-ideas-xj65un`

## What it is

RollCall analyzes **multiple captures of the same sub-GHz transmission** (same
remote button, pressed 2+ times) to determine, bit-by-bit, which parts of the
payload are static and which change — then optionally predicts the next
transmission and sends it. It fills a gap in the existing Flipper tooling: the
stock app and popular community tools (ProtoView, Spectrum Analyzer, Weather
Station) all analyze a *single* capture in isolation; RollCall's value is
entirely in the **cross-capture diff**.

## Hardware / RF path

- Radio: Flipper's internal **CC1101** sub-1GHz transceiver, driven through the
  firmware's `subghz_devices` HAL abstraction (not a raw register poke) — the
  same driver path the stock Sub-GHz app uses.
- Frequency: RX defaults to **433.92 MHz**; TX frequency is read per-signal from
  the source capture (so a 315/318/868 MHz capture transmits on its own
  frequency, not a fixed default).
- Modulation: RX runs a single fixed preset, **OOK, 650 kHz bandwidth**
  (`FuriHalSubGhzPresetOok650Async`) — covers the large majority of OOK remotes.
  TX reads the **Preset** field from the source capture and maps it to one of
  the four standard device presets (OOK 270 kHz, OOK 650 kHz, 2-FSK dev 2.38 kHz,
  2-FSK dev 47.6 kHz), defaulting to OOK 650 kHz if unrecognized. Custom/raw
  preset register blobs are not supported.
- Decoding: uses the firmware's existing **protocol registry** — the identical
  decoder set behind the stock *Read* screen (Princeton, CAME, KeeLoq, Nice
  FloR-S, MegaCode, etc., whatever the running firmware ships). RollCall adds no
  protocol decoders of its own.
- Region enforcement: TX calls `subghz_devices_set_tx()` before keying the
  radio; the HAL rejects frequencies not permitted by the firmware's region
  table, and RollCall surfaces that as a clean failure rather than transmitting
  anyway.

## Capabilities

1. **Capture** — live RX (decoded packets only; RAW/undecoded signals are
   skipped in this version) or load existing `.sub` key files from the SD card.
   Both converge on the same internal representation: `Protocol` name, `Bit`
   count, `Key` value. Live captures are also saved to a temp `.sub` so they can
   later be transmitted.
2. **Bitwise diff** — for a set of 2+ captures (must share protocol + bit
   length), classifies every bit position as fixed or changing across the set,
   and renders a bit-matrix heatmap plus a hex view with differing digits
   highlighted.
3. **Field verdict** — reports the changing block as `Fixed code` (nothing
   moves), `Counter` (values increase monotonically), `Rolling (random)`
   (high-entropy / avalanche pattern typical of an encrypted hop), or `Mixed`.
4. **Auto-predict** — with 3+ *consecutive* captures, tests whether the full key
   (as an integer) advances by a single constant step; if so, predicts the next
   key (carry-correct across byte boundaries, e.g. `0x7F → 0x80`). Two captures
   still produce a prediction but are flagged low-confidence. Anything
   non-monotonic or high-entropy reports **no pattern** rather than guessing.
5. **Craft & Call** — a payload editor seeded from a capture (or a prediction):
   fixed bits are locked, the changing field can be stepped ±1 (with carry) or
   hand-edited bit-by-bit, then transmitted by holding a button (continuous
   resend while held, released to stop).
6. **Transmit mechanism** — never rebuilds a signal from scratch. It copies the
   source `.sub`, swaps only the `Key` field, and re-serializes/deserializes
   through the firmware's own `SubGhzTransmitter`, so `Frequency`, `Preset`,
   `TE`, and any protocol-specific fields are reused unmodified.

## Intended use cases

- Determine whether an unfamiliar remote is a fixed-code or rolling-code system
  without a datasheet, by capturing a few presses and reading the verdict.
- Locate and size the rolling/counter field within a payload (bit position and
  width), useful when characterizing or documenting a protocol.
- Reproduce and test the next code of a **plaintext, incrementing-counter**
  system (a genuinely weak but real class of remote) to validate the counter
  window / anti-replay behavior of a receiver you own.
- Teaching/demo aid: makes "fixed vs. rolling vs. encrypted" a visible,
  bit-level distinction instead of an abstract description.

## Explicit limits (read before assuming capability)

- **Does not break encryption.** KeeLoq, Security+, Nice FloR-S-style encrypted
  hopping codes are correctly identified as high-entropy / "no pattern" —
  RollCall does not attempt key recovery, cryptanalysis, or ciphertext
  prediction. There is no code path that computes or guesses an encrypted next
  value; it declines by design.
- **Prediction assumes consecutive presses.** Skipped presses or out-of-order
  captures will most likely break the constant-step check and correctly yield
  "no pattern" rather than a wrong prediction.
- **RAW (undecoded) signals are out of scope for this version.** Only captures
  the firmware's protocol registry can decode to `Protocol`/`Bit`/`Key`
  participate; RAW-timing demodulation is a documented roadmap item, not
  implemented.
- **Editing `Key` on multi-field protocols is best-effort.** For protocols whose
  encoder derives the on-air signal from more than the stored key bytes, a
  crafted key may not round-trip exactly. Fixed-code and simple-counter
  protocols (the primary target) are exact.
- **Legal/RF responsibility is on the operator.** The app enforces the
  firmware's region table but does not know local regulations beyond that
  table, and provides no authorization gating of its own — transmit only to
  devices you own or are authorized to test.

## Status

Builds clean and passes `APPCHK` against firmware API 87.1 (official channel;
also builds against Unleashed/Momentum SDKs, which ship more protocol
decoders). The classification and prediction logic is unit-tested on a host
build (fixed / counter / high-entropy / carry-boundary / mismatched-protocol
cases). Live RX, live TX, and on-device UI timing have not yet been validated
against real hardware/receivers.
