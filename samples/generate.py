#!/usr/bin/env python3
"""Generate RollCall sample .sub captures. Synthetic sets are test vectors:
the keys are made up and operate no real device; they exist only to exercise
RollCall's fixed/counter/random classifier. Verdicts are verified against
model/analysis.c."""
import os

def write_sub(path, freq, preset, proto, bit, key_bytes, te=None):
    lines = [
        "Filetype: Flipper SubGhz Key File",
        "Version: 1",
        f"Frequency: {freq}",
        f"Preset: {preset}",
        f"Protocol: {proto}",
        f"Bit: {bit}",
        "Key: " + " ".join(f"{b:02X}" for b in key_bytes),
    ]
    if te is not None:
        lines.append(f"TE: {te}")
    with open(path, "w") as f:
        f.write("\n".join(lines) + "\n")

base = os.path.dirname(os.path.abspath(__file__))

# 1) Real fixed-code data (MegaCode), 4 different buttons of one remote.
#    Source: github.com/Zero-Sploit/FlipperZero-Subghz-DB (Gates/Lift_master).
#    Expected RollCall verdict: Mixed (fixed facility code + small button field).
for i, last in enumerate((0xDA, 0xE1, 0xDE, 0xDD), start=1):
    write_sub(
        f"{base}/megacode_liftmaster/Liftmaster_Parking_Gate_{i}.sub",
        318000000, "FuriHalSubGhzPresetOok270Async", "MegaCode", 24,
        [0x00, 0x00, 0x00, 0x00, 0x00, 0xCE, 0x10, last])

# 2) Synthetic rolling code (KeeLoq): fixed 32-bit serial+button, random 32-bit
#    hop per press. Expected RollCall verdict: Rolling (random).
serial = [0x1A, 0x2B, 0x3C, 0x4D]
hops = [
    [0x9F, 0x3C, 0x71, 0xE2],
    [0x14, 0xA8, 0x0D, 0x5B],
    [0xC3, 0x62, 0xF9, 0x07],
    [0x7E, 0xD1, 0x46, 0xBC],
    [0x2B, 0x08, 0xEE, 0x93],
]
for i, hop in enumerate(hops, start=1):
    write_sub(
        f"{base}/keeloq_rolling/keeloq_button_A_press{i}.sub",
        433920000, "FuriHalSubGhzPresetOok650Async", "KeeLoq", 64, hop + serial)

# 3) Synthetic clear counter: fixed 16-bit serial + 8-bit counter that
#    increments each press. Expected RollCall verdict: Counter.
for i in range(6):
    write_sub(
        f"{base}/counter_demo/counter_press{i+1}.sub",
        433920000, "FuriHalSubGhzPresetOok650Async", "TestCounter", 24,
        [0x00, 0x00, 0x00, 0x00, 0x00, 0xC3, 0x9A, 0x7D + i])

print("generated samples under", base)
