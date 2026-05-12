# Forza UDP Capture

Capture telemetry from Forza to debug gear/transmission display.

## Usage

1. Run: `python forza_udp_capture.py`
2. In Forza: Settings → Data Out → IP = **your PC IP** (e.g. 192.168.1.5), Port = **5300**
3. Start driving — the script prints packet size and gear bytes

## Output

- `len` — packet size (232=Sled, 311=FM Dash, 323=FH Dash)
- `G@307` — byte at FM7/FM8 gear offset
- `G@319` — byte at FH4/FH5 gear offset
- Pos, A, B, C, H — RacePosition, Accel, Brake, Clutch, HandBrake

Gear mapping: 0=R, 1–10=gears, 11=N.
