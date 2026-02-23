# BMW I-Bus / K-Bus — reference

This folder holds reference material for the BMW E39 Assistant (Nocturne OS).

## Protocol summary

- **I-Bus / K-Bus**: single-wire serial, 9600 baud, **8E1** (8 data bits, even parity, 1 stop bit).
- **Levels**: Not 3.3 V logic; use a transceiver (MCP2004A, TH3122, or optocoupler). See I-K_Bus schematics.
- **E39**: Supports both I-Bus and K-Bus; same protocol. Typical tap: CD changer connector (trunk) or behind radio.
- **Packet format**: `[source] [length] [dest] [cmd] [data...] [checksum]`. Checksum = XOR of all bytes including length. Length = number of bytes from destination through data (excl. source and checksum); typically 3–36.

## Sources

- [I-K_Bus](https://github.com/muki01/I-K_Bus) — Arduino library, E46/E39 codes, installation.
- [Hack The IBus](http://web.archive.org/web/20110320205413/http://ibus.stuge.se/Main_Page) — device IDs, message list.
- **BMW Communication Codes** — see `BMW Communication Codes/buttons.txt` for steering wheel / phone button hex codes (if copied here).

## This repo layout

- **library/** — IbusSerial + RingBuffer (ESP32-compatible).
- **codes/** — IbusCodes.h/cpp (locks, lights, etc.) for E39.
- **reference/** — IbusDefines.h (device and command IDs).
- **docs/** — this file and any extra notes.
