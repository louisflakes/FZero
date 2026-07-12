# RollCall — Sub-GHz signal field analyzer

RollCall captures the **same button** from a sub-GHz remote several times and
diffs the decoded payloads **bit-by-bit**, so you can see the *internal
structure* of a signal you already captured:

- Which bits are **fixed** (serial number, button id, framing)
- Which bits **change** between presses (the interesting part)
- Whether the changing block behaves like a **counter** (a rolling-code
  sequence number that increments) or looks **random** (an encrypted rolling
  block you can observe but not predict)

The stock Sub-GHz app and popular community apps (ProtoView, Spectrum Analyzer,
Weather Station, TPMS) all work on a *single* capture in real time. RollCall
fills a different gap: **comparing multiple captures** to reveal field layout.

> **Analysis tool, not an attack tool.** RollCall never predicts, replays, or
> forges codes. It shows you how a signal is built. A garage remote whose bits
> never change is a fixed-code device; one with a large random block is using a
> rolling code — RollCall lets you *see* that, nothing more.

## How it works

Both capture sources converge on the same representation — a `Protocol` name, a
`Bit` count, and a `Key` value:

- **Live RX** listens on a frequency, decodes with the firmware's protocol
  registry (same decoders as the stock *Read* screen), and serializes each
  decoded packet to pull out `Protocol`/`Bit`/`Key`.
- **Load .sub** reads those same three fields straight out of saved `.sub` key
  files.

The analysis engine (`model/analysis.c`) then counts, per bit position, how
often that bit was `1` across all captures, classifies each bit as fixed or
changing, and renders a field-level verdict for the contiguous changing block
(`Fixed code` / `Counter` / `Rolling (random)` / `Mixed`).

The diff view draws a **bit matrix** — one row per capture, one column per bit.
Fixed bits show up as clean vertical stripes; changing bits look noisy, so the
structure is legible at a glance. Left/Right move a cursor to inspect any bit
(`ones/n` and its class).

## Usage

1. **Capture live** — pick from the menu, then press the remote repeatedly
   (default 433.92 MHz, OOK). Each recognised press is added to the set. Press
   the center button to **Analyze**.
2. **Load .sub file** — add saved captures of the same button from
   `SD:/subghz`.
3. **Analyze (N)** — open the bit-matrix diff of the current set.
4. **Clear captures** — start over.

Only captures with a **matching protocol and bit length** can be combined — a
bitwise diff across different protocols is meaningless, so mismatches are
rejected.

## Building

Built and API-checked against the official firmware SDK (`ufbt`, API 87.1):

```sh
pip install ufbt
ufbt            # build dist/rollcall.fap
ufbt launch     # build + install + run on a connected Flipper
```

RollCall only uses **core** sub-GHz APIs (`subghz_devices`, the receiver/worker
chain, `flipper_format`), so it also builds on **Unleashed** and **Momentum**,
which ship *more* protocol decoders — meaning more signals RollCall can analyze.
To build against a custom firmware's SDK, point `ufbt` at it:

```sh
ufbt update --index-url https://up.momentum-fw.dev/firmware/directory.json   # Momentum
# or the Unleashed SDK index, then: ufbt
```

## Layout

```
rollcall.c              app entry, ViewDispatcher + SceneManager wiring
rollcall_i.h            shared app struct, view/scene/event ids
model/capture.{h,c}     Capture model, .sub loader, decoder->capture
model/analysis.{h,c}    bit classification + field verdict (pure, unit-tested)
views/diff_view.{h,c}   bit-matrix heatmap view
subghz/rollcall_rx.{h,c} live receiver (devices + worker + receiver chain)
scenes/                 start / capture / diff / about scenes
```

## Status

- Builds clean and passes `APPCHK` against firmware API 87.1.
- The analysis engine is unit-tested on the host (fixed / counter / random /
  two-button / mismatch cases).
- Live RX and the on-device GUI still need validation on real hardware.

## Roadmap

- **RAW / unknown-protocol mode** — demodulate raw `.sub` timings (threshold,
  sync detection, bit alignment) so unrecognised protocols can be diffed too.
- Field-value readout for a detected counter (show the sequence).
- Export an analysis summary back to a file.
