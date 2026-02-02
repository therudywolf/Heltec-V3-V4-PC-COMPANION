# Heltec PC Monitor — JSON protocol v3

One JSON object per line over TCP. Device sends `screen:N\n` (N = 0–5). Server sends `{"ct":..., "gt":..., ...}\n`. All keys are 2 characters except arrays and media.

## JSON keys (2-char) → main.cpp

| Key                       | main.cpp field               | Screen / use |
| ------------------------- | ---------------------------- | ------------ |
| ct                        | hw.ct (CPU temp)             | SYS.OP       |
| gt                        | hw.gt (GPU temp)             | SYS.OP       |
| cl                        | hw.cl (CPU load)             | SYS.OP       |
| gl                        | hw.gl (GPU load)             | SYS.OP       |
| ru                        | hw.ru (RAM used G)           | SYS.OP       |
| ra                        | hw.ra (RAM total G)          | SYS.OP       |
| nd                        | hw.nd (net down KB/s)        | NET.IO       |
| nu                        | hw.nu (net up KB/s)          | NET.IO       |
| cf,s1,s2,gf               | hw.cf,s1,s2,gf (fans RPM)    | THERMAL      |
| su                        | hw.su (NVMe % use)           | STORAGE      |
| du                        | hw.du (HDD % use)            | STORAGE      |
| vu,vt                     | hw.vu, hw.vt (VRAM)          | SYS.OP       |
| ch                        | hw.ch (chipset °C)           | THERMAL      |
| dr,dw                     | hw.dr, hw.dw (disk R/W KB/s) | —            |
| wt,wd,wi                  | weather.temp, desc, icon     | ATMOS        |
| w_wh, w_wl, w_wc          | weather week arrays          | —            |
| tp                        | procs (top CPU)              | —            |
| tp_ram                    | procs (top RAM)              | —            |
| art, trk, play, cover_b64 | media                        | MEDIA        |

Values: integers for loads/temps; 1-decimal float for RAM (ru, ra, vu, vt). Weather: after first successful fetch, server never sends 0/null for temp/desc (cached).

## Screens (0–5)

0 = [SYS.OP] Operations (2×2: CPU/GPU temp+load, RAM, VRAM or Uptime)  
1 = [NET.IO] Uplink (DOWN/UP, bars, IP)  
2 = [THERMAL] Cooling (fans + chipset)  
3 = [STORAGE] Drives (SYS C:, DATA D: bars + %)  
4 = [ATMOS] Weather (icon, temp, description, location)  
5 = [MEDIA] Deck (STANDBY or Artist / Track + progress)

When changing keys, update both `monitor.py` `build_payload()` and `main.cpp` `parsePayload()`.
