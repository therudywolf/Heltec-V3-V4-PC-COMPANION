# Nocturne OS — Optimization Notes (audit backlog)

Read-only audit of the firmware + PC server for **Heltec WiFi LoRa 32 V4**
(ESP32-S3R2, 240 MHz, 16 MB flash, **2 MB PSRAM**, OLED SSD1315 128x64 over I2C @ 800 kHz).

Each finding: `file:line` · what's suboptimal · concrete fix · **impact** (high/med/low) · **risk**.
Grouped by area, severity-ordered within each group. Items needing hardware verification
(display timing, I2C, ADC, BMW bus) are flagged **[HW-VERIFY]**.

Already-fixed (out of scope, noted for context): contrast caching (`applyContrast`,
`main.cpp:617`), LED `pinMode` hoisted out of loop, `WiFi.setSleep(false)`.

> Note on scale: most heap/CPU wins below are individually small on a 512 KB-SRAM /
> 240 MHz S3. They matter mainly (a) for the BMW build's RAM headroom alongside NimBLE,
> and (b) because they're cheap, low-risk, and compound. The display-loop and server
> items are the ones with user-visible payoff.

---

## 1. Memory / heap

### 1.1 `lineBuffer_[NOCT_TCP_LINE_MAX]` = 4 KB static in NetManager, plus a redundant 4 KB stack `JsonDocument` path — **HIGH**
- **Where:** `lib/nocturne-core/src/nocturne/config.h:122` (`NOCT_TCP_LINE_MAX 4096`); buffer at `src/modules/network/NetManager.h:56`; the double-parse at `src/main.cpp:653-658`.
- **Problem A (double parse):** In `loop()` the JSON line is parsed **twice**. `main.cpp:653` builds a `JsonDocument doc; deserializeJson(doc, buf, bufLen)` purely to test validity, then `NetManager::parsePayload` (`NetManager.cpp:224`) constructs a **second** `JsonDocument` and deserializes the same buffer again. ArduinoJson v7 `JsonDocument` is heap-backed and elastic; for this ~1–2 KB payload that's two full allocate→parse→free cycles per received frame (~2/s), doubling parse CPU and heap-churn/fragmentation for zero benefit.
  - **Fix:** Delete the first parse block in `main.cpp:653-656`. Call `parsePayload(buf, bufLen, &state)` directly and treat its `false` return as "invalid/ignore"; move the `markDataReceived` / graph pushes to run only when it returns `true`. One parse per frame.
- **Problem B (buffer size):** `NOCT_TCP_LINE_MAX = 4096` sizes a permanent 4 KB member in `NetManager` (lives for the whole program). The server caps payloads at `MAX_PAYLOAD_BYTES = 4096` (`monitor.py:230`) and falls back to a tiny minimal frame above that, but a typical frame is well under 2 KB.
  - **Fix:** Drop to `2048` (still > observed payloads; matches the realistic worst case). Saves ~2 KB RAM. If you want headroom, keep 4096 but at least apply Problem A.
- **Impact:** A: high (halves per-frame JSON CPU + heap churn, the main steady-state cost in the PC build). B: med (2 KB SRAM).
- **Risk:** A: low (pure dedup; `parsePayload` already returns a bool). B: low — verify the largest real payload (all fans + 4 disks + media + claude + events) stays < 2048 bytes before lowering.

### 1.2 ArduinoJson `JsonDocument` allocated per-frame on the stack — consider a reusable member — **MED**
- **Where:** `src/modules/network/NetManager.cpp:224` (and the duplicate at `main.cpp:653`).
- **Problem:** A fresh `JsonDocument` per call repeatedly grabs/returns its internal pool from the heap, the classic fragmentation source on ESP32 long-uptime devices.
- **Fix:** After applying 1.1-A, make a single `JsonDocument doc_;` member of `NetManager`, call `doc_.clear()` then `deserializeJson(doc_, …)` each frame. The pool is reused; steady-state heap allocations for parsing drop to ~zero. (ArduinoJson v7 reuses capacity across `clear()`.)
- **Impact:** med (long-uptime heap stability).
- **Risk:** low. Confirm peak doc capacity is acceptable as a resident member (~a few KB once warmed).

### 1.3 `String` churn in the hot JSON parse path — **MED**
- **Where:** `NetManager.cpp:289-329` — `weather.desc`, `process.cpuNames[0..2]`, `process.ramNames[0..1]`, `media.artist`, `media.track`, `media.mediaStatus`, `claude.plan` are all assigned `String(...)` **every frame** (~2/s). Types in `Types.h:38-77`.
- **Problem:** ~10 `String` heap (re)allocations per payload. Several are tiny but `mediaStatus` is rebuilt from a `strcmp` each time, and the names reallocate whenever length changes. This is the single largest ongoing heap-churn contributor after the JsonDocument.
- **Fix:** Convert the fixed-size, short fields to `char[]` like the code already does for `EventsData.top`/`severity` (`Types.h:60-64`, parsed with `strncpy` at `NetManager.cpp:355-359`). Good candidates: `mediaStatus` (→ a bool `isPlaying` already exists; drop the string entirely and derive at draw time), `claude.plan` (`char[12]`), `weather.desc` (`char[24]`). Process/media names can stay `String` if you prefer, but `char[24]` bounded buffers remove all allocation. `SceneManager` already `strncpy`s them into local buffers anyway (`drawPlayer` `artistBuf`/`trackBuf`, `SceneManager.cpp:972-979`).
- **Impact:** med.
- **Risk:** low-med (touches `Types.h` + every reader; mechanical but broad). Do `mediaStatus` and `claude.plan` first — highest churn, smallest blast radius.

### 1.4 FreeRTOS task stacks are generously sized; trim after measuring — **LOW**
- **Where:** `src/modules/car/ibus/IbusDriver.cpp:111-112` (`ibus_rx`/`ibus_tx`, 2048 words each = **8 KB each** — `xTaskCreate` stack arg is in **words**, 4 bytes on ESP32); `src/modules/car/DemoManager.cpp:52-59` (`DemoTask`, 2048 words = 8 KB).
- **Problem:** Three tasks at 2048 words = ~24 KB of stack reserved. The IBus read/write loops (`IbusDriver.cpp:61-85`) and the demo loop (`DemoManager.cpp:21-38`) do trivial work (queue ops, a few `random()` calls, small `memcpy`) — they almost certainly use a fraction of that.
- **Fix:** Add `uxTaskGetStackHighWaterMark` logging behind `NOCT_BMW_DEBUG`, run the BMW build, read the high-water marks, then size to peak + ~512 B margin (likely 1024–1536 words each). Reclaims several KB SRAM for the BMW build (which runs NimBLE concurrently and is the most RAM-pressured).
- **Impact:** low (SRAM headroom; only matters if BMW build approaches limits).
- **Risk:** **[HW-VERIFY]** med — under-sizing a stack is a hard crash. Must measure on real hardware (with BMW traffic + BLE connected) before committing numbers. Do not guess.

### 1.5 PSRAM is completely unused — wire it up for the few genuinely large buffers — **LOW (info)**
- **Where:** board has 2 MB PSRAM; no `ps_malloc` / `MALLOC_CAP_SPIRAM` / `-DBOARD_HAS_PSRAM` anywhere, and `platformio.ini` build flags (`platformio.ini:34-46`, per-app inis) don't enable PSRAM.
- **Problem:** All allocations land in internal SRAM (~512 KB), the scarce resource; 2 MB of PSRAM sits idle.
- **Reality check:** The firmware has *no* large heap buffers that belong in PSRAM — the OLED is 1 KB, JSON ~2 KB, graphs tiny. PSRAM has higher latency, so the per-frame display buffer and JSON parse should **stay** in SRAM. PSRAM would only help if you add something big later (frame history, packet capture ring for the hacker build, image/cover-art buffer).
- **Fix:** Low priority. If/when a large buffer appears, add `-DBOARD_HAS_PSRAM` and allocate that one buffer with `heap_caps_malloc(…, MALLOC_CAP_SPIRAM)`. Until then, document "PSRAM intentionally unused" so it isn't mistaken for an oversight.
- **Impact:** low today.
- **Risk:** **[HW-VERIFY]** — enabling PSRAM changes the memory map and adds octal/quad-SPI bring-up; only do it with a real allocation to test against.

### 1.6 `drawXBMArtFromBase64` carries a 1 KB stack buffer + mbedtls dep for dead code — **LOW**
- **Where:** `lib/nocturne-core/src/DisplayEngine.cpp:927-943` (`uint8_t decoded[1024]` on stack; `#include <mbedtls/base64.h>` at line 9). Header comment says "Legacy ... unused; media uses cassette animation".
- **Problem:** Dead function pulls in mbedtls/base64 and, if ever called, puts 1 KB on the caller's stack. Pure flash/dependency bloat.
- **Fix:** Delete `drawXBMArtFromBase64` and the mbedtls include. Confirm no callers (grep shows none in app code).
- **Impact:** low (flash + one dependency).
- **Risk:** low.

---

## 2. Display / render loop

### 2.1 Full-frame redraw + `sendBuffer` runs at up to ~60 FPS regardless of whether anything changed — **HIGH** (battery/I2C) **[HW-VERIFY]**
- **Where:** main render gate `src/main.cpp:997-1002` (`guiTimer` = `NOCT_REDRAW_INTERVAL_MS = 17`, config.h:139); `sendBuffer` at `main.cpp:1160`; `DisplayEngine::sendBuffer` → `u8g2_.sendBuffer()` (`DisplayEngine.cpp:112`).
- **Problem:** The loop redraws whenever `guiTimer.check(now)` is true — i.e. every 17 ms (~58 FPS) — even when no data, input, animation, or alert changed. Each `sendBuffer()` pushes the **entire** 1024-byte framebuffer over I2C. At 800 kHz that's ~1024×9 bits ≈ 11.5 ms of bus activity, ~58×/s → the I2C bus and CPU are busy a large fraction of every second drawing identical frames. On battery this is a real drain; it also starves other work.
  - Telemetry arrives ~2/s (`HEARTBEAT_INTERVAL = 0.5`, `monitor.py:234`). The only things that legitimately need ~60 FPS are: active scene **transition** (`inTransition`), the media **scroll** (`drawPlayer` uses `millis()/90`), fan spin, glitch, and blink. Static scenes do not.
- **Fix:** Gate the high frame rate on "is something actually animating?" Keep the 17 ms tick *only* while `inTransition || settings.glitchEnabled || (mediaScene && textOverflows) || state.alertActive || quickMenuOpen || carousel-due`. Otherwise fall back to a much slower refresh (e.g. redraw on `needRedraw` + a 200–500 ms idle keepalive). `needRedraw` already exists and is set on every data update (`main.cpp:660`) and input — the machinery is there; this just stops the *unconditional* timer redraw.
- **Impact:** high — potentially cuts steady-state I2C/CPU/display power by an order of magnitude on static screens.
- **Risk:** **[HW-VERIFY]** med — must confirm scroll/fan/transition still look smooth (they keep the fast path) and that there's no visible "first frame after idle" lag. Test each scene on hardware.

### 2.2 Per-frame `snprintf` + `getUTF8Width` for values that rarely change — **MED**
- **Where:** every scene draw, e.g. `SceneManager.cpp:310,329,346` (drawMain), `410-419` (drawCpu), `446-454` (drawGpu), `683-690` (drawMotherboard), `599-607` (drawDisks), `649-653` (drawFans). Each builds strings and measures widths every frame.
- **Problem:** Combined with 2.1 (≤60 FPS), each frame runs ~8–12 `snprintf`s and as many `getUTF8Width` glyph-walks for values that update at most 2/s. Wasted CPU.
- **Fix:** Primary fix is 2.1 (don't draw unchanged frames at all) — that alone removes ~96% of these calls. Secondary: if a scene still needs the fast path (transition), it's drawing fresh data anyway, so leave it. No need for value-cache complexity if 2.1 lands.
- **Impact:** med (largely subsumed by 2.1).
- **Risk:** low.

### 2.3 `applyGlitch()` / `drawGlitch()` do full-buffer reads + RNG every frame when glitch enabled — **MED**
- **Where:** `DisplayEngine::applyGlitch` (`DisplayEngine.cpp:1186-1224`) called at `main.cpp:1155` and several hacker/idle paths; `drawGlitch` (`DisplayEngine.cpp:124-209`) with an **8×128 = 1 KB `ghostBuf` on the stack** (line 132) and nested per-pixel buffer shuffles.
- **Problem:** When `settings.glitchEnabled`, `applyGlitch` runs every rendered frame: multiple `random()` calls + conditional full-width box draws + (in `drawGlitch`) a 1 KB stack buffer and O(pixels) copy loops. It's an opt-in cosmetic effect but it's pure overhead while on.
- **Fix:** (a) Time-gate the effect: only mutate the buffer every N ms (e.g. 60–100 ms) instead of every frame — the eye can't see per-17ms glitch changes anyway, and it cuts the work ~4–6×. (b) `drawGlitch`'s `ghostBuf[1024]` only needs `pageRows*128` bytes; size it to the actual band (≤6 px → ≤2 pages → 256 B) to shrink stack use. Note `drawGlitch` appears unused by app code (only `applyGlitch` is wired in) — confirm and consider deleting it.
- **Impact:** med (only when glitch on; default is off — `Settings.glitchEnabled=false`, `Types.h:86`).
- **Risk:** low.

### 2.4 `drawDecryptedText` / `drawHexStream` / `drawHexDecoration` reseed from `millis()/100` and re-randomize every frame — **LOW**
- **Where:** `DisplayEngine.cpp:557-587`, `705-723`, `608-634`.
- **Problem:** These regenerate random hex every frame; combined with 2.1's high FPS they spin RNG needlessly. Already throttle their *visible* change to 100 ms via the seed, but still recompute each frame.
- **Fix:** Subsumed by 2.1 (frames won't render when idle). If a screen using these is on the fast path, leave as-is.
- **Impact:** low.
- **Risk:** low.

### 2.5 `now % 5000 < 100` RSSI sampling is a 100 ms-wide window, can fire multiple times or miss — **LOW**
- **Where:** `src/modules/network/NetManager.cpp:128` (`if (now % 5000 < 100) rssi_ = WiFi.RSSI();`).
- **Problem:** Modulo-window timing fires on *every* loop iteration whose `millis()` lands in the 0–99 ms slot of each 5 s period — so `WiFi.RSSI()` (a real driver call) runs many times in that window (loop is sub-ms), and if a loop ever skips the window it's missed entirely. Minor wasted RSSI reads.
- **Fix:** Use an explicit `lastRssiMs_` timestamp like the rest of the codebase (`if (now - lastRssiMs_ >= 5000) { rssi_ = WiFi.RSSI(); lastRssiMs_ = now; }`).
- **Impact:** low.
- **Risk:** low.

---

## 3. CPU / timing

### 3.1 Loop `yield()` cadence and "always-redraw" interact to peg a core — **MED**
- **Where:** `src/main.cpp:987-1001, 1162-1163` — the loop `yield()`s only every >10 ms, and otherwise returns after a full draw.
- **Problem:** With 2.1 unfixed, the loop is effectively a 60 FPS render spinner: it never truly idles, so the Arduino loop task holds its core busy continuously (no `vTaskDelay`, only `yield()` which doesn't sleep). This blocks lower-priority background work and keeps the CPU at full power draw.
- **Fix:** After 2.1, when nothing needs redrawing add a short `vTaskDelay(pdMS_TO_TICKS(2..5))` on the idle path instead of `yield()`. That lets the scheduler sleep/idle the core (lower power) and still wakes well within input/telemetry latency budgets.
- **Impact:** med (battery + thermals + frees a core for WiFi/BLE).
- **Risk:** low-med — keep the delay small so button latency (`input.update()`) stays crisp; verify double-tap/long-press still feel responsive.

### 3.2 Boot sequence spends ~3.5–4 s in blocking `vTaskDelay`s before the loop — **LOW**
- **Where:** `src/main.cpp:255-322` — Vext settle 100 ms, RST pulse 50+50 ms, post-`begin` 100 ms, Vext recheck 50 ms, LED blink 200 ms, ADC warmup 100+50 ms; plus `drawBootSequence` and the **demo-hold poll** (`main.cpp:281-302`) which can wait up to `NOCT_DEMO_BOOT_HOLD_MS = 2500 ms`.
- **Problem:** Several delays are conservative. The demo-hold loop always costs up to 2.5 s of polling only to detect a held button. Slower cold-start than necessary.
- **Fix:** (a) The demo-hold can be shortened or made event-driven; 2.5 s held-button is a long UX ask anyway. (b) Some OLED reset delays (`50 ms` pulses) are likely over-spec for SSD1315 — but this is **[HW-VERIFY]**; do not shorten display reset timing without scope-checking init reliability across units.
- **Impact:** low (one-time, boot only).
- **Risk:** **[HW-VERIFY]** med for the display-reset delays; low for the demo-hold logic.

### 3.3 `predatorMode` path clears + sends a full empty buffer every loop — **LOW**
- **Where:** `src/main.cpp:989-995` — when `predatorMode`, every loop does `clearBuffer(); sendBuffer();` then returns.
- **Problem:** Pushes a blank 1 KB framebuffer over I2C ~every loop iteration (sub-ms cadence, only gated by the 10 ms `yield`) for a screen that's intentionally blank. Pure wasted I2C bandwidth/power while predator mode is active.
- **Fix:** Send the cleared buffer **once** on entry to predator mode, then just `vTaskDelay` (and keep servicing the LED breathing at `main.cpp:729-736`). No need to re-blank an already-blank panel.
- **Impact:** low (only while predator mode on).
- **Risk:** low.

### 3.4 BMW poll alternation is fine; verify it isn't gated by the render return paths — **LOW (info)**
- **Where:** `src/modules/car/BmwManager.cpp:641-651` polls every `NOCT_IBUS_POLL_INTERVAL_MS = 3000` ms, round-robin over 4 requests. `bmwManager.tick()` is called at `main.cpp:632-633` before any early `return`. Good — no issue. Noted so it's not "fixed" by mistake.

---

## 4. Flash / size

### 4.1 `lib_ldf_mode = deep+` + `build_src_filter = +<*>` compiles all modules even when feature flags disable them — **MED**
- **Where:** all four `platformio.ini` files (root `platformio.ini:25,32`; `apps/bmw/platformio.ini:29-34`; `apps/pc/platformio.ini:30-34`; `apps/hacker`).
- **Problem:** `apps/bmw` and `apps/pc` correctly *exclude* hacker/forza/network `.cpp` via `build_src_filter`, good. But the **root** `platformio.ini` uses `+<*>` for *every* env including `bmw_only` — so `bmw_only` still compiles `ForzaManager.cpp`, `TrapManager.cpp`, `WifiSniffManager.cpp`, `BleManager.cpp`, and the whole `modules/network/` tree. Those files are individually `#if`-guarded so they compile to little, but they still pull headers, lengthen builds, and risk linking unused code. The per-app inis (the real product builds) are the ones that matter and they're already trimmed — except `apps/pc` still compiles `modules/car/ForzaManager` (intended, Forza is on) and the root `bmw_only` env is the outlier.
- **Fix:** Mirror the `apps/bmw` exclusions into the root `[env:bmw_only]` (exclude `modules/network`, `modules/ble`, `ForzaManager.*`). Or treat the per-app inis as canonical and document that root `bmw_only` is dev-only. Verify each product binary's flash use with `pio run -e <env> -v` size output.
- **Impact:** med (build time; modest flash; cleaner link).
- **Risk:** low (the exclusions are already proven in `apps/bmw`).

### 4.2 Many large `const` glyph/icon tables are `static const` in `.cpp` (RAM-resident on some configs) vs. forced flash — **MED** **[HW-VERIFY-build]**
- **Where:** `src/modules/display/SceneManager.cpp:29-182` — `wolf_head_side[128]`, `wolf_head_growl[128]`, `wolf_idle/blink/aggressive/funny[128]` each, `icon_sun_bits[128]`, `icon_cloud_bits[128]`, fan frames, etc. Also `DisplayEngine.cpp:12-87` weather/wolf XBMs. These are `static const unsigned char[]` / `const uint8_t[]`.
- **Problem:** On ESP32/Xtensa, `const` globals normally land in flash (`.rodata`) and are read directly, so usually this is *fine*. But it's worth confirming none are being copied to DRAM. The wolf sprite set alone is 6×128 = 768 B; all icons together are a few KB. If any are accidentally in RAM (e.g. via a non-const reference or PROGMEM mismatch), that's reclaimable SRAM.
- **Fix:** No code change needed if they're already in flash — **verify** via the linker map / `xtensa-esp32s3-elf-size` that these symbols are in `.flash.rodata`, not `.dram`. ESP-IDF/Arduino-ESP32 generally places `const` in flash automatically (PROGMEM is a no-op there), so this is a *check*, not a guaranteed fix. The duplicate wolf/weather icon definitions across `SceneManager.cpp` and `DisplayEngine.cpp` (see 4.3) are the more concrete win.
- **Impact:** med if any are in RAM; otherwise none.
- **Risk:** low (read-only verification).

### 4.3 Duplicated icon/data tables across translation units — **MED**
- **Where:** Weather icons exist **twice**: `DisplayEngine.cpp:30-87` (`icon_weather_sun_32_bits`, `..._cloud_..`, `..._rain_..`, `..._snow_..`, 128 B each) **and** `SceneManager.cpp:160-182` (`icon_sun_bits`, `icon_cloud_bits`, 128 B each). The wolf 32×32 also appears as `icon_wolf_bits` (`DisplayEngine.cpp:17`) and the `wolf_head_*` set (`SceneManager.cpp:29-78`).
- **Problem:** Near-identical bitmaps duplicated in flash. Each 32×32 icon is 128 B; the redundant set is ~0.5–1 KB of flash plus maintenance drift (two sources of "the sun icon").
- **Fix:** Consolidate to one canonical set in `DisplayEngine` (already `extern`-declared in `DisplayEngine.h:43-46`) and have `SceneManager` use those via the header instead of redefining `icon_sun_bits`/`icon_cloud_bits`. `drawWeather` (`SceneManager.cpp:753-759`) and `drawWeatherIcon32` (`1007-1031`) should reference the shared symbols.
- **Impact:** med (flash + maintainability).
- **Risk:** low — verify the two variants are actually pixel-identical before merging; if intentionally different art, keep both but name them clearly.

### 4.4 Font set is broad; audit for unused faces — **LOW**
- **Where:** `lib/nocturne-core/src/DisplayEngine.h:13-30` declares `HEADER_FONT (t0_11)`, `LABEL_FONT (profont10)`, `VALUE_FONT (helvB10)`, `HUGE_FONT (logisoso24)`, `WEATHER_TEMP_FONT (helvB24_tf)`, `CYRILLIC_FONT (6x10_tf)`, `HEXDECOR/HEXSTREAM (5x7_tf)`, `UNIT_FONT (4x6_tr)`, `RAM_PROCESS_FONT (6x12_tr)`, plus `DISPLAY_FONT (5x7_tr)` in `DisplayManager.cpp:13`.
- **Problem:** U8g2 fonts are sizable flash blobs. `helvB24_tf` and `logisoso24` are large 24px faces; `helvB24_**tf**` includes the full extended set (vs `_tr` ascii-only). If only digits + `°` + `+`/`-` are drawn with the weather temp font (`drawWeather`, `SceneManager.cpp:775` uses `"%+d°"`), the `_tf` variant wastes flash on glyphs never rendered.
- **Fix:** (a) Check whether `HUGE_FONT (logisoso24)` is still used anywhere (splash uses it at `DisplayEngine.cpp:949`; confirm that's the only use and whether a smaller face suffices). (b) For `WEATHER_TEMP_FONT`, switch `helvB24_tf` → `helvB24_tr` (ascii) or a digits-only subset if Cyrillic/extended isn't drawn at 24px — saves flash. (c) `CYRILLIC_FONT (6x10_tf)` — confirm Cyrillic is actually rendered (parse path stores UTF-8 weather desc; if the server only sends ASCII descriptions, the `_tf` extended font is unnecessary). U8g2 supports custom glyph subsets if you want to go further.
- **Impact:** low-med (each large font is a few KB of flash).
- **Risk:** low — but **verify on hardware** that no in-use string loses glyphs (especially the `°` / `+` in temps and any Cyrillic UI text) before swapping `_tf`→`_tr`.

### 4.5 Partition table uses ~8 MB of a 16 MB flash; `otadata` present but no OTA slots — **LOW (info)**
- **Where:** `huge_app.csv` — `app0` 0x500000 (5 MB) at 0x10000, `spiffs` 0x2F0000 (~3 MB) at 0x510000; `otadata` (0x2000) declared but only one app partition exists.
- **Problem:** Not a runtime cost, but: (a) ~8 MB of flash is unallocated/unused; (b) `otadata` without an `ota_1` app slot means OTA can't actually A/B update — it's vestigial. The board doc (`HELTEC_V4_BOARD_AND_CONFIG.md:82`) calls this `huge_app.csv`.
- **Fix:** If OTA is desired later, repartition for two ~5 MB app slots (16 MB allows it comfortably) — that's the point of having 16 MB. If OTA is *not* planned, drop `otadata` and grow `spiffs`/app to reflect intent. Either way, document the choice.
- **Impact:** low (capability/clarity, not speed).
- **Risk:** low — repartitioning requires a full reflash; coordinate with any LittleFS data layout.

---

## 5. Server (Python — `server/monitor.py` + submodules)

### 5.1 Two ping subprocesses' worth of confusion: ping interval mismatch (5 s loop vs `last_ping_time` 5.0) is fine, but `psutil.process_iter` is the real cost — **MED**
- **Where:** `monitor.py:586-622` (`get_top_processes_cpu_sync` / `_ram_sync`), invoked via executor at `monitor.py:967-973` every `TOP_PROCS_CACHE_TTL = 2.5 s` (`monitor.py:232`).
- **Problem:** `psutil.process_iter(["name","cpu_percent"])` and again `(["name","memory_info"])` each enumerate **all** processes — two full process-table walks every 2.5 s. On a busy Windows box that's the heaviest recurring server cost (hundreds of processes × OpenProcess/handle work), even though it runs in the executor (off the loop). It also iterates twice (CPU then RAM) when one pass could collect both.
- **Fix:** (a) Merge into a single `process_iter(["name","cpu_percent","memory_info"])` pass that feeds both the CPU and RAM top-N (one enumeration instead of two). (b) Consider raising `TOP_PROCS_CACHE_TTL` to 3–5 s — the device only shows top-3/top-2 process names which change slowly. (c) `cpu_percent` without an interval returns usage since the *last* call per process; the first call per process is 0.0 — the current cached cadence handles this, just don't reduce TTL below ~2 s or values get noisy.
- **Impact:** med (largest steady server CPU draw).
- **Risk:** low (logic-local; keep the same top-N output shape).

### 5.2 `should_send_payload` builds a full payload every 0.5 s even when nothing's sent — **LOW-MED**
- **Where:** `monitor.py:1006-1015` builds `payload` via `build_payload(...)` **every** loop tick (0.5 s), then `should_send_payload` (`811-827`) decides whether to actually transmit.
- **Problem:** `build_payload` (`741-804`) does real work each call: `normalize_hdd` (with possible psutil fallback), `evaluate_alert`, dict construction with ~40 keys, `_build_claude_block`, `_alert_state.snapshot`. With a `HEARTBEAT_INTERVAL = 0.5 s` the payload is sent most ticks anyway, so this is mostly fine — but on quiet periods where the heartbeat just elapsed and nothing changed, it still rebuilds the whole dict to then maybe send it. The change-detection snapshot (`_payload_snapshot`) only needs ~8 fields.
- **Fix:** Cheap reorder: compute the lightweight change-detection snapshot from the cached values *first*; only call the full `build_payload` when `should_send_payload`-equivalent says "will send". Since heartbeat forces a send every 0.5 s anyway, the win is small — file this as low priority unless `HEARTBEAT_INTERVAL` is raised.
- **Impact:** low-med (scales if heartbeat interval increases).
- **Risk:** low.

### 5.3 `json.dumps` of the full payload per client per send — fine, but encode once — **LOW**
- **Where:** `monitor.py:830-839` (`send_data_to_client`) called per client in the broadcast loop `monitor.py:1019-1024`.
- **Problem:** `send_data_to_client` does `json.dumps(payload, …).encode()` **inside** the per-client loop. With one device it's moot, but with N clients the identical payload is serialized N times.
- **Fix:** Serialize once before the `for w in tcp_clients` loop (`monitor.py:1019`) and pass the pre-encoded `bytes` to a thin writer. Encode-once.
- **Impact:** low (only matters with multiple displays).
- **Risk:** low — keep the per-client size-guard/minimal-fallback (`monitor.py:834-837`) by computing it once too.

### 5.4 `walk_sensors` builds a `set(targets.values())` and full `path_to_val` dict every poll — **LOW**
- **Where:** `server/lhm_parse.py:88-90+` (`walk_sensors`), called from `_parse_lhm_json` (`monitor.py:373`) every `POLL_INTERVAL = 0.5 s`.
- **Problem:** Each poll rebuilds `targets_set = set(targets.values())` (constant!) and a fresh `path_to_val` of *every* leaf sensor (LHM trees can have hundreds), then most are discarded. The `targets`/`alias` maps never change.
- **Fix:** (a) Hoist `targets_set` to a module constant (it's derived from the constant `TARGETS`). (b) `path_to_val` is needed for fan/storage/MB extraction so it's hard to avoid fully, but you could pass only the prefixes of interest and skip storing leaves that match nothing relevant. Lower priority — runs in executor? No: `_parse_lhm_json` runs inline in `_get_lhm_raw_async` after `await r.json()` (`monitor.py:409`), i.e. **on the event loop**. See 5.5.
- **Impact:** low (CPU), but see 5.5 for the loop-blocking angle.
- **Risk:** low.

### 5.5 LHM JSON parse + `_parse_lhm_json` run **on the asyncio event loop**, not in the executor — **MED**
- **Where:** `monitor.py:403-413` (`_get_lhm_raw_async`): `await r.json()` then `return _parse_lhm_json(...)` — the parse (`monitor.py:365-400` → `lhm_parse.walk_sensors`, a recursive walk of the whole LHM tree) executes synchronously in the coroutine, i.e. **blocks the event loop**.
- **Problem:** LHM `/data.json` can be large; the recursive `walk_sensors` + all `extract_*` helpers are pure-Python CPU work done inline on the loop every 0.5 s. While it runs, the TCP server can't service clients or the webhook. The codebase is careful to push *other* blocking work (ping, psutil, media) to `run_in_executor` (`monitor.py:938-948, 962-1002`) — LHM parsing is the one heavy synchronous step that slipped through onto the loop. The big `await r.json()` deserialization also happens on the loop.
- **Fix:** Move the parse off the loop: `text = await r.text(); hw = await loop.run_in_executor(executor, _parse_lhm_json_from_text, text)` where the executor function does both `json.loads` and `_parse_lhm_json`. Keeps the loop responsive during the heaviest recurring parse.
- **Impact:** med (loop responsiveness / latency jitter for the device stream, especially with `source=prometheus` adding a second scrape+parse at `monitor.py:461-479`).
- **Risk:** low-med — ensure thread-safety: `_parse_lhm_json` reads module constants and `os.getenv` only (safe); just don't touch shared mutable state from the executor fn.

### 5.6 `get_ping_latency_sync` spawns a `ping` subprocess every 5 s — acceptable, but note the cost — **LOW**
- **Where:** `monitor.py:563-583`, scheduled at `monitor.py:984-990` (every 5 s).
- **Problem:** A `subprocess.run(["ping", ...])` every 5 s = process spawn + ~1 RTT wait, forever. Correctly run in the executor with `CREATE_NO_WINDOW`, so it doesn't block the loop and doesn't flash a console — good. The cost is just the recurring spawn.
- **Fix:** Optional. Could use a persistent ICMP socket (needs admin on Windows) or accept the subprocess. Given it's off-loop and 5 s cadence, **leave as-is** unless profiling flags it. Listed for completeness.
- **Impact:** low.
- **Risk:** n/a (no change recommended).

---

## Quick-win shortlist (highest value / lowest risk first)

1. **1.1-A** — remove the duplicate `JsonDocument` parse in `main.cpp:653-656` (halves per-frame JSON cost; trivial, low-risk).
2. **2.1** — stop the unconditional 60 FPS redraw; only fast-path while animating (biggest battery/I2C win). **[HW-VERIFY]**
3. **5.5** — move LHM parse into the executor (keeps the server loop responsive).
4. **5.1** — single `process_iter` pass for CPU+RAM top-N (biggest server CPU win).
5. **1.2 / 1.3** — reuse one `JsonDocument` member + convert the highest-churn `String`s (`mediaStatus`, `claude.plan`) to fixed buffers.
6. **4.3** — de-duplicate the weather/wolf icon tables.
7. **3.1 / 3.3** — `vTaskDelay` on idle loop paths instead of busy `yield`; blank predator screen once.

## Items requiring hardware verification before action
- **2.1, 3.1** display refresh rate / loop idling — confirm scroll/transition smoothness and input latency on the panel.
- **1.4** FreeRTOS stack trimming — must measure `uxTaskGetStackHighWaterMark` with BLE + BMW traffic; under-sizing crashes.
- **3.2** OLED reset/init delay shortening — scope-check init reliability across units; do not shorten blind.
- **4.2, 4.4** font/const-table placement and `_tf`→`_tr` font swaps — verify no in-use glyph (°, +/−, Cyrillic) is lost and confirm tables are in flash via the linker map.
- **1.5** PSRAM enablement — only with a concrete large allocation to test against.
