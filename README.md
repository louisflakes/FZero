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

The diff view draws a **bit matrix** — one row per capture, one column per bit
(filled cell = 1, empty cell = 0). Fixed bits show up as clean vertical stripes;
changing bits look noisy, so the structure is legible at a glance. Below it, a
classification strip marks each bit fixed (thin line) or changing (tall bar).

Controls in the diff view:

- **Left / Right** — move the bit cursor; the footer shows that bit's value in
  each file (e.g. `b11 chg 1 0 1`) and whether it's `fix`ed or `chg`ing.
- **OK** — toggle **bits ↔ hex**: the hex view lists each capture's key as a hex
  value with the differing digits boxed, so you can read the actual bytes.
- **Up** — jump to **Craft & Call** seeded with the most recent capture.
- **Back** — return to the menu.

## Auto-Analysis (one button)

With 2+ captures, **Auto-Analysis** runs a predictor and routes you:

- **Pattern found** — the changing field is a monotonic constant-step counter
  (or the code is fixed). It shows the step and the predicted next value (e.g.
  `Counter +1 → next C39A80`) and offers **Load burst**, which opens Craft & Call
  pre-filled with the predicted next payload — just hold OK to send.
- **No pattern** — the captures don't form a clean counter (irregular, or a
  high-entropy encrypted rolling code). Routes to **Manual** analysis.

The predictor works on the full key as an integer, so carries propagate
correctly (`7F → 80`). It assumes captures were taken as **consecutive** presses;
inconsistent steps report "no pattern" rather than guess.

## Craft & Call (transmit)

Craft & Call edits a payload and transmits it. Fixed bits are locked; the
changing field is seeded from the most recent capture (or the predicted next).

- **Up / Down** — step the value ±1 (with carry). Hold to scroll.
- **Left / Right** — move the bit cursor.
- **OK (tap)** — flip the bit under the cursor.
- **OK (hold)** — transmit while held; the Flipper **vibrates** while sending.
  Release to stop. Back to leave.

Transmit reuses the source `.sub` as a template: only the `Key` is swapped, so
frequency/preset/TE and any protocol-specific fields are preserved. TX is
**region-limited** — a disallowed frequency fails cleanly. Fixed-code and
plaintext-counter systems are sent exactly; **encrypted rolling codes** (KeeLoq,
etc.) transmit the crafted bits but the target won't accept them (the valid next
code is ciphertext you can't compute without the device key) — that case shows
as "no pattern" in Auto-Analysis. Only transmit to devices you own or are
authorized to test, and obey local RF regulations.

## Usage

1. **Capture live** — pick from the menu, then press the remote repeatedly
   (default 433.92 MHz, OOK). Each recognised press is added to the set (and
   saved as a temp `.sub` so it can be transmitted). Press OK to **Analyze**.
2. **Load .sub file** — add saved captures of the same button from `SD:/subghz`.
3. **Auto-Analysis** — predict + optionally load the next burst to send.
4. **Analyze (N)** — open the bit-matrix diff of the current set.
5. **Craft & Call** — hand-edit a payload and transmit it.
6. **Clear captures** — start over.

Only captures with a **matching protocol and bit length** can be combined — a
bitwise diff across different protocols is meaningless, so mismatches are
rejected.

## Bench FSK (interop test rig)

**Bench FSK** is a separate, fixed-configuration mode (menu item "Bench FSK
(RAK3401)") for interop testing against the RAK3401/RAK13302 bench tester in
[`rak_fsk_benchtop/`](rak_fsk_benchtop/) — not part of the remote-signal
analysis flow above. It listens at 915.000 MHz on the stock **FM238** preset
(2-FSK, ±2.380371 kHz deviation, ~4.8 kbps — CC1101's hardware sync/preamble
detection is off in this preset, so RollCall does its own bit-sync and framing
in software, in `subghz/bench_fsk_codec.{h,c}`) for a small framed packet this
project defines itself (magic/version/type/seq/length/payload/CRC-16 — see
that header for the exact wire format, which must match
`rak_fsk_benchtop/src/main.cpp`'s `make_packet`/`parse_packet` exactly, since
the two are independently-implemented sides of one protocol). Press **Send
PING** to transmit one burst and watch for the RAK's **PONG** reply; RX/TX
counts and the last decoded packet are shown live.

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
rollcall.c               app entry, ViewDispatcher + SceneManager wiring
rollcall_i.h             shared app struct, view/scene/event ids
model/capture.{h,c}      Capture model, .sub loader, decoder->capture
model/analysis.{h,c}     bit classification + verdict + predictor (unit-tested)
views/diff_view.{h,c}    bit-matrix heatmap view
views/craft_view.{h,c}   payload editor (hex/bit, Up/Down value, OK-hold send)
subghz/rollcall_rx.{h,c} live receiver (devices + worker + receiver chain)
subghz/rollcall_tx.{h,c} transmit engine (key-swap template + hold-repeat)
subghz/bench_fsk_codec.{h,c}     Bench FSK wire format + bit-sync/framing (unit-tested, no Flipper API deps)
subghz/rollcall_bench_fsk.{h,c}  Bench FSK device glue (raw worker callback, one-shot TX)
scenes/                  start / capture / auto / diff / craft / bench_fsk / about
rak_fsk_benchtop/        standalone PlatformIO/Arduino firmware for the RAK3401/RAK13302 bench tester
```

## Status

- Builds clean and passes `APPCHK` against firmware API 87.1.
- Analysis + predictor are unit-tested on the host (fixed / counter / random /
  carry `7F→80` / fixed-LSB step / irregular / mismatch cases).
- Bench FSK's codec (`bench_fsk_codec.c`) is unit-tested on the host: clean
  round-trip, ±50µs jitter, inverted-polarity fallback, noise/gap recovery,
  and corrupted-CRC rejection. The RAK3401 tester firmware
  (`rak_fsk_benchtop/`) builds clean under PlatformIO.
- Live RX, transmit, and the on-device GUI still need validation on real
  hardware — this very much includes Bench FSK's actual over-the-air link:
  the CC1101/SX1262 timing match is verified against real register tables and
  datasheets, not against an actual RF capture between the two radios.

## Roadmap

- **RAW / unknown-protocol mode** — demodulate raw `.sub` timings (threshold,
  sync detection, bit alignment) so unrecognised protocols can be diffed too.
- **LFSR / linear recovery** — detect and predict "looks-random-but-linear"
  rolling codes (Berlekamp–Massey).
- **Encryption-info panel** — protocol→cipher facts + avalanche/linearity stats
  for the high-entropy case.
- Export an analysis summary back to a file.
