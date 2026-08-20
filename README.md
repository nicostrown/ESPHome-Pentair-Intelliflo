# Pentair IntelliFlo VS → ESPHome → Home Assistant

For a LILYGO T-CAN485 (ESP32) wired to the pump's RS-485 pair (yellow = A/+, green = B/−).

## Install

Copy `pentair-intelliflo.yaml` into your Home Assistant ESPHome config directory.
The component itself is pulled from this repo at build time, so there are no files
to place by hand:

```yaml
external_components:
  - source:
      type: git
      url: https://github.com/gamer22026/ESPHome-Pentair-Intelliflo
      ref: main
    components: [pentair_intelliflo]
    refresh: 0s
```

The only secrets this config needs are `wifi_ssid` and `wifi_password` — the two
the ESPHome dashboard already sets up. Nothing else to add.

The API is unencrypted and OTA has no password, which keeps the secret list to
those two. To harden either one, add a key to your ESPHome secrets and uncomment
the relevant line in the YAML:

```yaml
api:
  encryption:
    key: !secret pool_pump_api_key      # openssl rand -base64 32

ota:
  - platform: esphome
    password: !secret pool_pump_ota_password
```

Verified against ESPHome 2026.7.2 — `esphome config` and `esphome compile` both
pass on `esp-idf` and on `arduino`.

If you would rather iterate on the C++ without a push/refresh cycle, clone the
repo next to the YAML and swap the source for a local path:

```yaml
external_components:
  - source:
      type: local
      path: components
    components: [pentair_intelliflo]
```

## Board pins (LILYGO T-CAN485)

| Signal | GPIO | Notes |
| --- | --- | --- |
| RS485 TX | 22 | |
| RS485 RX | 21 | |
| `PIN_5V_EN` | 16 | must be HIGH — powers the transceiver |
| `RS485_EN` | 17 | must be HIGH |
| `RS485_SE` | 19 | must be HIGH — enables automatic TX/RX turnaround |

Source: [`Xinyuan-LilyGO/T-CAN485` → `example/Arduino/RS485/config.h`](https://github.com/Xinyuan-LilyGO/T-CAN485).
The three enable pins are driven HIGH by `gpio` switches with
`restore_mode: ALWAYS_ON`. Because the board handles direction itself there is no
DE pin to toggle around writes. Do **not** add a `psram:` block — GPIO16/17 are
the PSRAM pins on WROVER modules and would collide.

## Entities

**Control**

| Entity | Behaviour |
| --- | --- |
| `select` **Preset** | Off / Low / Medium / High / Max. Picking a speed while stopped starts the pump. |
| `number` **Speed setpoint** | Any RPM in range. Changing it while running ramps the pump; while stopped it starts it. |
| `switch` **Pump** | Master on/off. |
| `select` **Control method** | `Direct RPM` or `External Program 1` — see below. |
| `button` **Release to local control** | Stops the pump and unlocks its keypad. |

**Status** — Power (W), Speed (RPM), Flow (GPM), Filter cycle used (%), Error code,
Time remaining, Running, Remote control, Program, Drive state, Error.

## How control works

Every command is a `0xA5` automation-bus frame addressed to the pump at `0x60`,
sent as `FF 00 FF A5 00 60 10 <cmd> <len> <payload> <ckHi> <ckLo>`:

| Purpose | Frame |
| --- | --- |
| Request status | `A5 00 60 10 07 00` |
| Take remote control | `A5 00 60 10 04 01 FF` |
| Release to local | `A5 00 60 10 04 01 00` |
| Start drive | `A5 00 60 10 06 01 0A` |
| Stop drive | `A5 00 60 10 06 01 04` |
| Set speed (direct) | `A5 00 60 10 01 04 02 C4 <rpmHi> <rpmLo>` |
| Store speed in program N | `A5 00 60 10 01 04 03 <0x26+N> <rpmHi> <rpmLo>` |
| Activate program N | `A5 00 60 10 01 04 03 21 00 <N*8>` |

A single `push_state` script reconciles the pump with whatever the HA entities
say, and a 15 s `interval` re-runs it. That keepalive matters: the pump falls back
to its own keypad and schedule once it stops hearing from the bus, which is also
the failsafe if the ESP32 dies.

Smooth transitions come for free — the pump ramps internally, so changing speed is
just a new `02 C4` write, and starting from stopped is a speed write followed by
`06 01 0A`.

### `Direct RPM` vs `External Program 1`

`Direct RPM` writes register `0x02C4`, which is volatile and is what
nodejs-poolController uses. It is the default. If your pump ignores it, switch the
**Control method** select to `External Program 1`, which instead stores the speed
in the pump's program 1 (register `0x0327`) and activates that program
(register `0x0321`). Two caveats for that mode, both handled in the YAML:

- `0x0327` is an EEPROM write, so it only happens when the setpoint actually
  changes, never on the keepalive.
- An externally activated program self-cancels after about a minute, so the
  keepalive re-sends `0x0321` every 15 s.

## What changed from the earlier revision

The component in this repo before this commit did not build or drive the pump:

1. `components/pentair_intelliflo/switch/` and `output/` are leftover **pipsolar**
   files that `#include "../pipsolar.h"`, which does not exist. Every `.cpp` under
   the component directory gets compiled, so the build fails. Dropped here.
2. `select.py` calls `set_operating_mode_select()`, which is not declared in
   `intelliflo.h`. Dropped; the YAML uses `select: platform: template` instead.
3. `commandRPM()` wrote register `0x0327` (program 1's *stored* speed) without
   ever activating program 1, so nothing changed the running speed.
   `commandFlow()` used command `0x09`, which is not a write command. Both fixed.
4. The `pressure` sensor read payload byte `[8]` and divided by 14.504. That byte
   is filter-cycle percent used, not PSI. Replaced with `filter_percent`.
5. The receiver required a literal `FF 00 FF A5` preamble and cleared the whole
   buffer on any mismatch, so a longer run of idle `FF`s dropped the frame that
   followed. Rewritten to sync on `00 FF A5` and discard one byte at a time.
6. The example YAML re-sent the RPM write every 10 s. Under the old code path that
   was an EEPROM write on every tick.
7. Board was `esp32-s3-devkitc-1` on GPIO17/18; the T-CAN485 is a plain ESP32 on
   GPIO22/21 and needs its three enable pins driven.

## Protocol references

- [Controlling an IntelliFlo pump from Home Assistant](https://www.yoctopuce.com/EN/article/controlling-an-intelliflo-pump-from-home-assistant) — the frames above match its published bytes exactly
- [nodejs-poolController wiki: Pumps](https://github.com/tagyoureit/nodejs-poolController/wiki/Pumps) — register map (`0x27`–`0x2A` program speeds, `0xC4` RPM, `0xE4` GPM)
