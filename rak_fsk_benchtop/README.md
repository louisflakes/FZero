# RAK3401/RAK13302 915 MHz 2-FSK bench tester

This is standalone test firmware for the WisMesh 1 W Booster Starter Kit:
RAK3401 core plus RAK13302 (SX1262 and SKY66122 PA). It is not Meshtastic
firmware and does not interoperate with Meshtastic packets.

## Purpose

It transmits and receives a small, documented packet format which the Flipper
bench app will use. Its only intended use is controlled, authorized benchtop
testing.

RF parameters are deliberately aligned with Flipper's `FM238` preset:

| Parameter | Value |
| --- | --- |
| Frequency | 915.000 MHz |
| Modulation | 2-FSK, NRZ, no Gaussian shaping |
| Bit rate | 4.8 kbps |
| Frequency deviation | +/- 2.380371 kHz |
| Preamble | 16 bits |
| Sync word | `D3 91` |
| SX1262 configured output | -9 dBm |

The RAK13302 contains a 1 W power amplifier. The configured SX1262 output is
kept at its minimum, but that does **not** establish the actual radiated power
after the external PA. Do not transmit until the setup is shielded or otherwise
made safe and lawful. Never place the RAK and Flipper antennas close together.

> **Sync word note:** the original bench-test plan for this project used sync
> word `12 AD`; the value actually implemented (and used consistently through
> this firmware and its docs) is `D3 91`. If you need it to match a specific
> prior capture, change `SyncWord[]` in `src/main.cpp:15` -- just keep the
> Flipper-side decoder in sync with whatever you pick.

## Flipper-side companion

The Flipper side of this bench test is RollCall's **Bench FSK** scene
(`../scenes/rollcall_scene_bench_fsk.c`, `../subghz/bench_fsk_codec.{h,c}`,
`../subghz/rollcall_bench_fsk.{h,c}`) -- see the top-level `../README.md`'s
"Bench FSK" section. The RF parameters above were checked against Flipper's
actual CC1101 register table for the stock `FM238` preset (not just its name):
`MDMCFG3 = 0x83` / `MDMCFG4 = 0x67` set **4.79794 kBaud**, `DEVIATN = 0x04` sets
**2.380371 kHz** deviation -- both match this project's RadioLib config, and
`MDMCFG2 = 0x04` confirms CC1101's hardware sync/preamble detection is off in
this preset, which is why the Flipper side does its own software bit-sync and
framing rather than relying on any hardware packet engine.

## Packet format

The SX1262 uses variable packet length mode. Its PHY length byte is outside the
application packet below.

```
A5 | 01 | type | sequence | payload_length | ASCII payload | CRC-16/CCITT-FALSE
```

CRC covers all bytes through the payload. Packet types are `01` PING, `02`
PONG, and `03` HELLO. A valid PING receives a PONG automatically.

## Build

```powershell
python -m platformio run
```

The UF2 will be under `.pio/build/rak3401_1watt/`. Do not flash it yet unless
you have a safe RF setup and have preserved the vendor/Meshtastic recovery UF2.

### Board configuration

This project targets `board = wiscore_rak4631` (not a generic Feather board):
the RAK3401 shares the RAK4631's bootloader/flash layout, and that's what
Meshtastic's own build uses for this hardware. `wiscore_rak4631` isn't one of
`platform-nordicnrf52`'s stock boards, so its board JSON is vendored at
`boards/wiscore_rak4631.json` (PlatformIO auto-discovers custom boards placed
under a project's `boards/` directory). The RAK13302 pin map is supplied
separately via `board_build.variant = rak3401_1watt`, pointing at
`variants/rak3401_1watt/`.

Both the board JSON and the variant files are vendored verbatim from
[`meshtastic/firmware`](https://github.com/meshtastic/firmware)
(`variants/nrf52840/rak3401_1watt/` and `boards/wiscore_rak4631.json`), used
here only to get correct hardware definitions for this standalone project --
no Meshtastic application code is included or built. The variant files carry
their original Arduino/Adafruit LGPL-2.1 header; the board JSON is sourced from
a GPL-3.0 repository.

## Serial interface

Open the USB serial port at 115200 baud.

| Key | Action |
| --- | --- |
| `t` | transmit `HELLO` |
| `p` | transmit `PING` |
| `h` | print help |

The receiver is active between commands. A received valid PING is printed and
answered with PONG.

## Restore Meshtastic

Double-tap reset to enter the nRF52 bootloader and copy the existing local
`firmware-rak3401-1watt-2.7.15.567b8ea.uf2` back to the device. Do not substitute
the regular `rak4631` firmware: the RAK13302 1 W board has a distinct target.

## Verifying against real firmware/library source

`src/main.cpp` was checked against RadioLib 7.3.0's actual pinned source
(`SX126x.h`/`.cpp`, `SX1262.h`) rather than assumed from memory:
`SX1262::beginFSK(freq, br, freqDev, rxBw, power, preambleLength, tcxoVoltage,
useRegulatorLDO)` is a real 8-argument override (the base `SX126x::beginFSK`
has fewer params and no frequency/power -- the `SX1262` object resolves to the
subclass override), and `receive(data, len, timeoutMs)` is a real 3-argument
overload in this version that polls the wired DIO1 IRQ pin for up to
`timeoutMs`, then calls `readData()`, which reads the true on-air packet length
via `getPacketLength()` -- so trusting `packet[4]` (this project's own declared
length byte) after a successful `receive()` call is correct, not undefined
behavior.
