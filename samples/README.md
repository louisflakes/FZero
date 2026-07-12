# RollCall sample captures

Test `.sub` files for exercising RollCall's analysis. Copy a folder's files to
your Flipper under `SD:/subghz/` (e.g. `SD:/subghz/rollcall_samples/`), then in
RollCall: **Load .sub file** for each file in a set, then **Analyze**. Use
**Clear captures** between sets — RollCall only combines captures with a
matching protocol and bit length.

| Set | Files | Protocol | What it is | Expected verdict |
|-----|-------|----------|------------|------------------|
| `megacode_liftmaster/` | 4 | MegaCode (24-bit) | **Real data**, 4 buttons of one Linear MegaCode gate remote | **Mixed** — fixed facility code + small changing button field |
| `keeloq_rolling/` | 5 | KeeLoq (64-bit) | **Synthetic** rolling code: fixed 32-bit serial+button, random 32-bit hop | **Rolling (random)** |
| `counter_demo/` | 6 | TestCounter (24-bit) | **Synthetic** clear-text counter: fixed serial + incrementing byte | **Counter** |

## Notes

- **`megacode_liftmaster/`** comes from
  [Zero-Sploit/FlipperZero-Subghz-DB](https://github.com/Zero-Sploit/FlipperZero-Subghz-DB)
  (`subghz/Gates/Lift_master`). MegaCode is a *fixed-code* protocol, so this is
  not a rolling code — it's a good demonstration that RollCall reports a fixed
  ID plus a button-select field rather than falsely calling it "rolling".
- **The synthetic sets are test vectors.** Their keys are fabricated and control
  no real device; they exist only to reproduce the bit structures RollCall
  classifies. `TestCounter` is not a real Flipper protocol — it loads in
  RollCall (which only reads Protocol/Bit/Key) but won't decode in the stock
  Sub-GHz app. All three verdicts are verified against `model/analysis.c`.
- Real rolling-code captures in signal databases are usually stored as
  `Protocol: RAW` (undecoded timing data). RollCall v1 skips RAW files — that's
  the RAW-demodulation roadmap item. Until then, use decoded (Protocol/Bit/Key)
  captures like these.

Regenerate with `python3 samples/generate.py`.
