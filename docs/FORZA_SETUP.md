# Forza Horizon 4/5 & Motorsport — Data Out Telemetry

The Wolf device acts as a real-time dashboard for Forza Horizon 4, Forza Horizon 5, and Forza Motorsport using the **Data Out** (UDP) feature.

## Enable Data Out in Forza

1. In Forza, go to **Settings** → **HUD and Gameplay** (or **Accessibility** in some versions).
2. Find **Data Out** or **UDP Telemetry**.
3. Enable it and set the following:
   - **IP Address:** The IP of your Wolf device (shown on the splash screen when you enter the mode).
   - **Port:** `5300`
   - **Data Format:** Sled or Dash, both supported. Speed is shown for both (Sled uses velocity-derived speed).

## Getting the Wolf Device IP

1. Open the menu: **Double-click** the button.
2. Navigate: **Games** → **RACING** (long-press to select).
3. A splash screen appears for 3 seconds showing:
   ```
   IP: 192.168.x.x
   PORT: 5300
   WAITING...
   ```
4. Note the IP address and enter it in Forza's Data Out settings.
5. Start or resume a race — the dashboard will begin receiving telemetry.

## Menu Path

**Main Menu** → **Games** → **RACING** → Forza dashboard

## Port

**5300** — Standard Forza Data Out UDP port.

## Requirements

- Wolf device connected to the **same Wi‑Fi network** as the PC/Xbox running Forza.
- Forza configured to send telemetry to the Wolf device's IP on port 5300.

## UDP Packet Format (port 5300)

Forza sends raw binary UDP packets. Two formats are supported:
- **Sled**: 232 bytes — speed from velocity (X,Y,Z)
- **Dash**: 311 bytes — speed at offset 244

Parsed offsets (little-endian):
| Offset | Type | Field |
|--------|------|-------|
| 0 | int32 | isRaceOn |
| 8 | float | EngineMaxRpm |
| 12 | float | EngineIdleRpm |
| 16 | float | CurrentEngineRpm |
| 32-40 | float | Velocity X,Y,Z (m/s) |
| 244 | float | Speed (m/s) — Dash only |
| 256-268 | float | Tire temps FL, FR, RL, RR (°C) |
| 280 | float | Fuel (0.0–1.0) |
| 296 | uint16 | LapNumber |
| 298 | uint8 | RacePosition |
| 307 | uint8 | Gear (0=R, 11=N, 1–10) |

**Debug**: In `config.h`, uncomment `#define FORZA_DEBUG_UDP` to log packet len, hex dump, and parsed values to Serial (~2 Hz).

## Displayed Data

- **Tachometer** (left) — NFS Underground style semicircular gauge
- **Gear** (right) — R, N, or 1–10
- **Speed** (right) — km/h (large digits)
- **Fuel** — Bar and % at bottom left
- **Tires** — FL, FR, RL, RR bars (temp → fill)
- **Position / Lap** — P3 L2 (top right)
- **Shift light** — Full-screen flash and LED blink when RPM ≥ 90% of max

## Exit

**Double-click** the button to return to the main menu.
