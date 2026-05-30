# 🐺 Nocturne BMW

> Standalone BMW E39 companion firmware for Heltec WiFi LoRa 32 V4 (ESP32-S3).

One of the three Nocturne OS products. Built from this monorepo on top of the
shared [`lib/nocturne-core`](../../lib/nocturne-core) library; this app compiles
**only** the BMW path — no WiFi, monitoring, or hacker code.

## What it does

- **BLE proximity key** — phone connects → unlock; disconnects → lock.
- **I-Bus assistant** — lights, locks, windows, wipers, interior light, cluster
  text, MFL steering-wheel buttons, PDC parking sensors.
- **Demo mode** — exercise the UI and the Android app without a car or I-Bus.
- **OBD-II (optional, off by default)** — ELM327 stub for RPM / coolant / oil.

---

# 🔌 Connection guide — board → transceiver → car

This is the full, self-contained wiring guide. Follow it top to bottom. Protocol
details and message codes are in [docs/bmw](../../docs/bmw); cross-referenced
against the I-Bus references under `BMW datasheet/` at the repo root.

> ⚠️ **You MUST use an I-Bus transceiver.** The BMW I/K-Bus is a single-wire
> bus that does **not** run at 3.3 V logic levels. Wiring the bus straight to a
> GPIO will not work and can damage the board. Use an MCP2025/MCP2003/MCP2004A
> LIN transceiver, a TH3122, or an opto-isolated circuit.

## 1. What you need

| Item | Notes |
|------|-------|
| Heltec WiFi LoRa 32 **V4** (ESP32-S3) | The board this firmware targets |
| I-Bus transceiver | MCP2004A / TH3122 / opto — TTL ↔ single-wire bus |
| BMW with I/K-Bus | E39 (primary), also E46 / E38 / E53 and similar |
| Dupont wires + crimps | Board ↔ transceiver |
| Tap into the I-Bus | T-tap, IBUS Y-splitter, or solder at a known point |
| USB-C cable | Flashing + serial monitor |
| (optional) ELM327 UART | Only if you want RPM/coolant/oil over OBD-II |

## 2. Block diagram

```
 ┌──────────────────────┐        ┌─────────────────────┐        ┌──────────────┐
 │   Heltec V4 (3.3 V)  │        │  I-Bus transceiver  │        │   BMW car    │
 │                      │        │                     │        │              │
 │  5V / Ve ───────────────────► VCC                   │        │              │
 │  GND ───────────────────────► GND ───────────────────────────► GND (body)   │
 │  GPIO 39 (TX) ──────────────► TXD                   │        │              │
 │  GPIO 38 (RX) ◄───────────── RXD            BUS ◄──────────────► I-Bus wire  │
 └──────────────────────┘        └─────────────────────┘        └──────────────┘
```

The transceiver does the level translation between the board's 3.3 V TTL UART
and the car's single-wire bus. The board never touches the bus directly.

## 3. Pin map — Heltec V4 → transceiver

| Heltec V4 pin | → | Transceiver | Purpose |
|---------------|---|-------------|---------|
| `5V` (or `3V3` if your module is 3.3 V) | → | `VCC` | Power the transceiver |
| `GND` | → | `GND` | Common ground (required) |
| **`GPIO 39` (TX)** | → | `TXD` | Board transmits onto the bus |
| **`GPIO 38` (RX)** | ← | `RXD` | Board receives from the bus |

> ❗ **Do not swap TX/RX.** GPIO 39 (TX) → transceiver **TXD**; GPIO 38 (RX) →
> transceiver **RXD**. Swapped pins is the #1 reason the bus never syncs.

These defaults come from `lib/nocturne-core/src/nocturne/config.h`
(`NOCT_IBUS_TX_PIN 39`, `NOCT_IBUS_RX_PIN 38`). Bus format is **9600 8E1**.

## 4. Transceiver → car

| Transceiver | → | Car | Notes |
|-------------|---|-----|-------|
| `BUS` | → | I-Bus wire | The single signal wire (see §5) |
| `GND` | → | Body ground | Solid chassis ground — share it with the board |

## 5. Where to tap the I-Bus in the car

The I-Bus is one signal wire plus ground. Common access points on an E39:

- **CD changer connector** in the trunk (right side) — easiest if present.
- **Behind the radio / head unit** — pins on the harness.
- **Around the fuse box / kick panels.**

Wire colours are a guide, not a guarantee — **verify with a multimeter / scope
before connecting:**

- I-Bus signal: often **white with a red/yellow stripe** (varies by year/market).
- Ground: **brown** (BMW standard) — or just use solid chassis ground.

> Tip: with the firmware's debug logging on (§8), a correct tap shows live hex
> frames within a second of the car waking. No frames = wrong wire or TX/RX
> swap.

## 6. OBD-II (optional)

Only if you want engine RPM / coolant / oil temperature. The firmware has an
ELM327 UART stub, **off by default**.

| Heltec V4 | → | ELM327 (UART) |
|-----------|---|---------------|
| `GPIO 9` (TX) | → | `RX` |
| `GPIO 10` (RX) | ← | `TX` |
| `GND` | → | `GND` |

ELM327 baud **38400 8N1**; power it from the car's OBD-II port. Enable in
`config.h`: set `NOCT_OBD_ENABLED 1` (pins `NOCT_OBD_TX_PIN` / `NOCT_OBD_RX_PIN`).

## 7. Pre-power checklist

- [ ] Board, transceiver, and car share a **common GND**.
- [ ] Transceiver `VCC` within spec (3.3 V or 5 V for your module).
- [ ] **TX/RX not swapped**: GPIO 39 → TXD, GPIO 38 → RXD.
- [ ] `BUS` connected to the real I-Bus wire; transceiver `GND` to chassis.
- [ ] Firmware built for the BMW product (`pio run -d apps/bmw`).

## 8. First run & verifying the bus

1. Flash and open the serial monitor (§ Build & flash below).
2. On the board: double-tap into the menu → **BMW** → long-press **BMW Assistant**.
3. With the car awake and the bus wired correctly, the header shows **`IBUS OK`**.
   If it shows **`No IBus`**, the bus isn't syncing — recheck §3/§5.
4. **Debug logging:** in `config.h` set `NOCT_BMW_DEBUG 1` (logs sent frames as
   `[IBus TX] …`) and/or `NOCT_IBUS_MONITOR_VERBOSE 1` (hex-dumps every received
   frame). Serial is **115200 baud**.

## 9. Using it

- **On the board:** short-press = next action in the list; long-press (~2 s) =
  send the selected I-Bus command. Double-tap = back to the menu.
- **From the phone:** the board advertises BLE as **"BMW E39 Key"**; the Android
  companion app (`companion-app/`) connects and sends commands (lights, locks,
  light-show, now-playing). See [companion-app/README.md](companion-app/README.md).
- **No car?** Use **BMW Demo** (menu → BMW → BMW Demo) to drive the UI/app with
  simulated telemetry — nothing is sent to a real bus.

## 10. Troubleshooting

| Symptom | Likely cause |
|---------|--------------|
| Header stuck on `No IBus` | TX/RX swapped, wrong bus wire, or no common GND |
| Garbage / no frames in verbose log | Bus wire wrong, or transceiver not level-shifting (check VCC) |
| Commands sent but car does nothing | Module asleep (wake the car), or wrong target module for that command |
| Board resets when car cranks | Power the board from a clean 5 V source, not a noisy accessory line |
| BLE key not visible | You're not in BMW Assistant mode, or BLE busy — re-enter the mode |

---

## Build & flash

```bash
# from the repo root
pio run -d apps/bmw                 # build
pio run -d apps/bmw -t upload       # flash over USB-C
pio device monitor -b 115200        # serial log
```

## Reference

Full I-Bus protocol reference lives at the repo root under `BMW datasheet/`
(wilhelm-docs, E46 codes, AVR-IBus). Firmware message codes are cross-checked
against it. Deeper protocol notes: [docs/bmw](../../docs/bmw).

## License

GNU AGPL-3.0-only — see [LICENSE](../../LICENSE).
