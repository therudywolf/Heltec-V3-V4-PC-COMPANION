/*
 * NOCTURNE_OS — Config: feature flags, pins, display, timeouts.
 * Feature flags set via platformio.ini build_flags (-D NOCT_FEATURE_*).
 */
#ifndef NOCTURNE_CONFIG_H
#define NOCTURNE_CONFIG_H

#define NOCTURNE_VERSION "2.0"

/* ── Feature flags (set by platformio.ini, defaults = BMW-only) ────────── */
#ifndef NOCT_FEATURE_BMW
#define NOCT_FEATURE_BMW 1
#endif
#ifndef NOCT_FEATURE_MONITORING
#define NOCT_FEATURE_MONITORING 0
#endif
#ifndef NOCT_FEATURE_FORZA
#define NOCT_FEATURE_FORZA 0
#endif
#ifndef NOCT_FEATURE_HACKER
#define NOCT_FEATURE_HACKER 0
#endif

/* ── Display contrast ──────────────────────────────────────────────────── */
#define NOCT_CONTRAST_MIN 12
#define NOCT_CONTRAST_MAX 255

/* ── V4 Pinout (Heltec WiFi LoRa 32 V4) ───────────────────────────────── */
#define NOCT_SDA_PIN 17
#define NOCT_SCL_PIN 18
#define NOCT_RST_PIN 21
#define NOCT_VEXT_PIN 36
#define NOCT_LED_ALERT_PIN 35
#define NOCT_BUTTON_PIN 0

/* ── Battery (GPIO 1 ADC, GPIO 37 divider control) ────────────────────── */
#define NOCT_BAT_PIN 1
#define NOCT_BAT_CTRL_PIN 37
#define NOCT_BAT_READ_DELAY 10
#define NOCT_BAT_DIVIDER_FACTOR 4.9f
#define NOCT_VOLT_MIN 3.3f
#define NOCT_VOLT_MAX 4.2f
#define NOCT_VOLT_CHARGING 4.4f
#define NOCT_BAT_CALIBRATION_OFFSET 0.11f
#define NOCT_BAT_READ_INTERVAL_STABLE_MS 10000
#define NOCT_BAT_READ_INTERVAL_CHARGING_MS 2000
#define NOCT_BAT_READ_INTERVAL_LOW_MS 5000
#define NOCT_BAT_SMOOTHING_SAMPLES 3

/* ── Alert LED ─────────────────────────────────────────────────────────── */
#define NOCT_ALERT_LED_BLINK_MS 175
#define NOCT_ALERT_LED_BLINKS 5
#define NOCT_ALERT_MAX_BLINKS 2
#define NOCT_ALERT_LED_BLINK_MAX_MS 5000

/* ── Display geometry (128x64 OLED) ────────────────────────────────────── */
#define NOCT_DISP_W 128
#define NOCT_DISP_H 64

#define NOCT_MARGIN 2
#define NOCT_HEADER_H 14
#define NOCT_HEADER_MARGIN 2
#define NOCT_HEADER_BASELINE_Y 11
#define NOCT_HEADER_SEP_Y 13
#define NOCT_HEADER_STATUS_RIGHT_ANCHOR 60
#define NOCT_CONTENT_START 17
#define NOCT_CONTENT_TOP NOCT_CONTENT_START
#define NOCT_FOOTER_H 14

/* Unified V4 grid (2x2 tech brackets) */
#define NOCT_GRID_ROW1_Y 19
#define NOCT_GRID_ROW2_Y 44
#define NOCT_GRID_COL1_X 0
#define NOCT_GRID_COL2_X 64

/* Main scene grid */
#define MAIN_TOP_H 28
#define MAIN_BAR_Y_OFFSET 32
#define MAIN_BAR_H 4
#define MAIN_LIFEBAR_H 3
#define MAIN_RAM_Y 50
#define MAIN_RAM_H 13
#define MAIN_TOP_Y_END 45
#define MAIN_RAM_BOX_TOP MAIN_RAM_Y
#define MAIN_RAM_BOX_H MAIN_RAM_H
#define MAIN_RAM_TEXT_Y (MAIN_RAM_Y + MAIN_RAM_H - 4)

/* Font metrics */
#define BASELINE_OFFSET_BIG 13
#define BASELINE_OFFSET_TINY 7
#define FONT_BIG_ASCENT 12
#define FONT_TINY_ASCENT 7
#define LINE_HEIGHT_BIG 16
#define LINE_HEIGHT_TINY 9
#define FONT_BIG_HEIGHT 14
#define FONT_LBL_HEIGHT 8
#define ROW_GAP 2
#define NOCT_ROW_DY 14
#define NOCT_ROW_DY_BIG 18
#define NOCT_CONTENT_MAX_ROWS 5

/* Card grid */
#define NOCT_CARD_LEFT 2
#define NOCT_CARD_TOP NOCT_CONTENT_TOP
#define NOCT_CARD_ROW_DY NOCT_ROW_DY
#define NOCT_FOOTER_Y 50
#define NOCT_FOOTER_TEXT_Y 52

/* Mode screens */
#define NOCT_MODE_HEADER_H 10
#define NOCT_LINE_H_DATA 12
#define NOCT_LINE_H_LABEL 5
#define NOCT_LINE_H_HEAD 10
#define NOCT_LINE_H_BIG 14

/* ── Network / TCP (PC monitoring) ─────────────────────────────────────── */
#define NOCT_TCP_LINE_MAX 4096
#define NOCT_TCP_CONNECT_TIMEOUT_MS 5000
#define NOCT_TCP_RECONNECT_INTERVAL_MS 2000
#define NOCT_SIGNAL_TIMEOUT_MS 5000
#define NOCT_SIGNAL_GRACE_MS 8000
#define NOCT_WIFI_RETRY_INTERVAL_MS 30000

/* ── Timing ────────────────────────────────────────────────────────────── */
#define NOCT_SPLASH_MS 2500
#define NOCT_IDLE_SCREENSAVER_MS 10000
#define NOCT_TRANSITION_MS 180
#define NOCT_TRANSITION_STEP 32
#define NOCT_BUTTON_SHORT_MS 500
#define NOCT_BUTTON_LONG_MS 1000
#define NOCT_BUTTON_PREDATOR_MS 2500
#define NOCT_GLITCH_INTERVAL_MS 10000
#define NOCT_GLITCH_DURATION_MS 100
#define NOCT_REDRAW_INTERVAL_MS 17
#define NOCT_GRAPH_SAMPLES 32
#define NOCT_GRAPH_HEIGHT 11

/* ── Menu layout ───────────────────────────────────────────────────────── */
#define NOCT_MENU_BOX_X 0
#define NOCT_MENU_BOX_Y 0
#define NOCT_MENU_BOX_W 128
#define NOCT_MENU_BOX_H 64
#define NOCT_MENU_CONFIG_BAR_X 2
#define NOCT_MENU_CONFIG_BAR_Y 0
#define NOCT_MENU_CONFIG_BAR_W 124
#define NOCT_MENU_CONFIG_BAR_H 10
#define NOCT_MENU_CONFIG_BAR_CHAMFER 2
#define NOCT_MENU_ROW_H 10
#define NOCT_MENU_VISIBLE_ROWS 5
#define NOCT_MENU_LIST_LEFT 2
#define NOCT_MENU_LIST_W 124
#define NOCT_MENU_START_Y 14

/* ── Battery HUD ───────────────────────────────────────────────────────── */
#define NOCT_BAT_FRAME_W 16
#define NOCT_BAT_FRAME_H 8
#define NOCT_BAT_TERM_W 3
#define NOCT_BAT_TERM_H 3

/* ── Alert thresholds (must match monitor.py) ──────────────────────────── */
#ifndef CPU_TEMP_ALERT
#define CPU_TEMP_ALERT 87
#endif
#ifndef GPU_TEMP_ALERT
#define GPU_TEMP_ALERT 68
#endif
#define CPU_LOAD_ALERT 90
#define GPU_LOAD_ALERT 100
#define VRAM_LOAD_ALERT 95
#define RAM_LOAD_ALERT 90

/* ── Scene indices (9 PC monitoring screens) ───────────────────────────── */
#define NOCT_SCENE_MAIN 0
#define NOCT_SCENE_CPU 1
#define NOCT_SCENE_GPU 2
#define NOCT_SCENE_RAM 3
#define NOCT_SCENE_DISKS 4
#define NOCT_SCENE_MEDIA 5
#define NOCT_SCENE_FANS 6
#define NOCT_SCENE_MOTHERBOARD 7
#define NOCT_SCENE_WEATHER 8
#define NOCT_TOTAL_SCENES 9

/* alert_metric codes (match monitor.py) */
#define NOCT_ALERT_CT 0
#define NOCT_ALERT_GT 1
#define NOCT_ALERT_CL 2
#define NOCT_ALERT_GL 3
#define NOCT_ALERT_GV 4
#define NOCT_ALERT_RAM 5

/* ── BMW E39 Assistant ─────────────────────────────────────────────────── */
#define NOCT_IBUS_ENABLED 1
#define NOCT_IBUS_TX_PIN 39
#define NOCT_IBUS_RX_PIN 38
#define NOCT_IBUS_POLL_INTERVAL_MS 3000
#define NOCT_IBUS_MONITOR_VERBOSE 0
#define NOCT_BMW_DEBUG 1
#define NOCT_BMW_DEMO_MODE 0
#define NOCT_BMW_DEMO_INTERVAL_MS 4000
#define NOCT_DEMO_BOOT_HOLD_MS 2500

/* ── OBD-II / ELM327 ──────────────────────────────────────────────────── */
#define NOCT_OBD_ENABLED 0
#define NOCT_OBD_TX_PIN 9
#define NOCT_OBD_RX_PIN 10

/* ── USB CDC ───────────────────────────────────────────────────────────── */
#define NOCT_USB_CDC_ENABLED 0

/* ── A2DP Sink (not supported on ESP32-S3) ─────────────────────────────── */
#define NOCT_A2DP_SINK_ENABLED 0
#define NOCT_I2S_BCK_PIN 45
#define NOCT_I2S_WS_PIN 46
#define NOCT_I2S_DOUT_PIN 40

#endif
