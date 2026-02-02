# Heltec PC Monitor — JSON protocol

One JSON object per line over TCP. Device sends `screen:N\n` to report current screen (0–10). Server sends `{"ct":..., "gt":..., ...}\n`.

## JSON keys → main.cpp

| Key                                | main.cpp field                             | Screen / use             |
| ---------------------------------- | ------------------------------------------ | ------------------------ |
| **Hardware (basic, any payload)**  |                                            |                          |
| ct                                 | hw.cpuTemp                                 | Main, CPU Details        |
| gt                                 | hw.gpuTemp                                 | Main, GPU Details        |
| gth                                | hw.gpuHotSpot                              | GPU Details              |
| cpu_load                           | hw.cpuLoad                                 | Main, CPU Details, Cores |
| cpu_pwr                            | hw.cpuPwr                                  | CPU Details              |
| cpu_mhz                            | hw.cpuMhzFallback                          | CPU Details, Cores       |
| gpu_load                           | hw.gpuLoad                                 | Main, GPU Details        |
| gpu_pwr                            | hw.gpuPwr                                  | GPU Details              |
| p, r, c, gf                        | hw.fanPump, fanRad, fanCase, fanGpu        | Cooling                  |
| gck                                | hw.gpuClk                                  | GPU Details              |
| nvme2_t, chipset_t                 | hw.nvme2Temp, hw.chipsetTemp               | Cooling                  |
| play                               | media.isPlaying                            | Media, Equalizer         |
| **Full payload only**              |                                            |                          |
| c_arr                              | hw.cores[6]                                | Cores                    |
| cl_arr                             | hw.coreLoad[6]                             | Cores                    |
| ram_u, ram_a, ram_pct              | hw.ramUsed, ramTotal=used+avail, hw.ramPct | Main, Memory             |
| vram_u, vram_t                     | hw.vramUsed, hw.vramTotal                  | Memory                   |
| d1_t..d4_t, d1_u..d4_u, d1_f..d4_f | hw.diskTemp/Used/Total[4]                  | Storage                  |
| net_up, net_down                   | hw.netUp, hw.netDown                       | Main                     |
| disk_r, disk_w                     | hw.diskRead, hw.diskWrite                  | —                        |
| wt, wd, wi, w_dh, w_dl, w_dc       | weather.\*                                 | Weather (3 subpages)     |
| w_wh, w_wl, w_wc                   | weather.weekHigh/Low/Code[7]               | Weather Week             |
| tp                                 | procs.cpuNames/Percent[3]                  | Top CPU                  |
| tp_ram                             | procs.ramNames/ramMb[2]                    | Memory                   |
| art, trk, cover_b64                | media.artist, track, coverBitmap           | Media                    |

## Screens (0–10)

0=Main, 1=Cores, 2=CPU Details, 3=GPU Details, 4=Memory, 5=Storage, 6=Media, 7=Cooling, 8=Weather, 9=Top CPU, 10=Equalizer.

When changing payload keys, update both `monitor.py` `build_payload()` and `main.cpp` `parseHardwareBasic()` / `parseFullPayload()` / `parseMediaFromDoc()`.
