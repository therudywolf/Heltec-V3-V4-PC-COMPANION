/*
 * NOCTURNE_OS — Config: pins (SDA/SCL/RST), display 128x64, timeouts,
 * NOCT_SCENE_* indices, NOCT_GRAPH_SAMPLES. WiFi/PC IP in secrets.h.
 */
#ifndef NOCTURNE_CONFIG_H
#define NOCTURNE_CONFIG_H

#define NOCTURNE_VERSION "1.0"

/* Display contrast: long-press / DIM toggle between min and max only */
#define NOCT_CONTRAST_MIN 12
#define NOCT_CONTRAST_MAX 255

/* V4 Pinout (standard Heltec): SDA=17, SCL=18, RST=21, Vext=36. LED=35;
 * BUTTON=0 Vext=36: LOW = OLED powered. RST=21: reset pulse LOW then HIGH. */
#define NOCT_SDA_PIN 17
#define NOCT_SCL_PIN 18
#define NOCT_RST_PIN 21 /* OLED reset (do not use 18 — that is SCL) */
#define NOCT_VEXT_PIN                                                          \
  36 /* Vext: LOW = enable OLED power. Never drive HIGH.                       \
      */
#define NOCT_LED_ALERT_PIN                                                     \
  35 /* White programmable LED; blink on any active alert */
#define NOCT_BUTTON_PIN 0
/* Battery (Heltec V4: GPIO 1 for ADC read, GPIO 37 for divider control) */
#define NOCT_BAT_PIN 1 /* ADC1_CH0 - Battery voltage read pin */
#define NOCT_BAT_CTRL_PIN                                                      \
  37 /* Control pin: HIGH=Enable divider, LOW=Disable                          \
      */
#define NOCT_BAT_READ_DELAY 10
#define NOCT_BAT_DIVIDER_FACTOR                                                \
  4.9f                          /* Voltage divider: (100+390)/100 = 4.9        \
                                 */
#define NOCT_VOLT_MIN 3.3f      /* 0% charge (LiPo empty) */
#define NOCT_VOLT_MAX 4.2f      /* 100% charge (LiPo full, 103450-LP 3.7V) */
#define NOCT_VOLT_CHARGING 4.4f /* Above this voltage = charging */
/* Calibration: if full charge shows ~88%, ADC/divider reads low. Add offset
 * so that displayed voltage matches real (e.g. +0.11f for 103450-LP 2000mAh). */
#define NOCT_BAT_CALIBRATION_OFFSET 0.11f
/* Adaptive battery reading intervals */
#define NOCT_BAT_READ_INTERVAL_STABLE_MS                                       \
  10000 /* Stable voltage: read every 10s */
#define NOCT_BAT_READ_INTERVAL_CHARGING_MS                                     \
  2000                                     /* Charging/discharging: every 2s */
#define NOCT_BAT_READ_INTERVAL_LOW_MS 5000 /* Low battery (<20%): every 5s */
#define NOCT_BAT_SMOOTHING_SAMPLES 3       /* Moving average samples */

#define NOCT_ALERT_LED_BLINK_MS 175
#define NOCT_ALERT_LED_BLINKS 5 /* 5 blinks then fade out */
#define NOCT_ALERT_MAX_BLINKS 2 /* Strict limit: 2 blinks only */
#define NOCT_ALERT_LED_BLINK_MAX_MS 5000

#define NOCT_DISP_W 128
#define NOCT_DISP_H 64

/* Uncomment to log incoming Forza UDP packets to Serial (len, hex, parsed) */
/* #define FORZA_DEBUG_UDP */
#define NOCT_MARGIN 2
/* === Dimensions (Protocol Alpha Wolf) === */
#define NOCT_HEADER_H 14
#define NOCT_HEADER_MARGIN 2
#define NOCT_HEADER_BASELINE_Y 11
#define NOCT_HEADER_SEP_Y 13
/* Right edge of WOOF!/NET:-- status text; battery HUD sits to the right. Keep
 * status end <= 60 so battery percentage text (e.g. NO BAT) does not overlap. */
#define NOCT_HEADER_STATUS_RIGHT_ANCHOR 60
#define NOCT_CONTENT_START                                                     \
  17 /* Content start below header; 17 avoids ascent overlap (was 14)          \
      */
#define NOCT_CONTENT_TOP NOCT_CONTENT_START
#define NOCT_FOOTER_H 14
/* Unified V4 grid (2x2 tech brackets): CPU, GPU, MB scenes */
#define NOCT_GRID_ROW1_Y 19
#define NOCT_GRID_ROW2_Y 44
#define NOCT_GRID_COL1_X 0
#define NOCT_GRID_COL2_X 64
/* Main scene grid */
#define MAIN_TOP_H 28        /* Height of CPU/GPU brackets */
#define MAIN_BAR_Y_OFFSET 32 /* Relative to bracket top */
#define MAIN_BAR_H 4         /* Slimmer bars */
#define MAIN_LIFEBAR_H 3     /* CPU/GPU load bar height (visible on 128x64) */
#define MAIN_RAM_Y 50        /* RAM box top */
#define MAIN_RAM_H 13
/* Legacy aliases for compatibility */
#define MAIN_TOP_Y_END 45
#define MAIN_RAM_BOX_TOP MAIN_RAM_Y
#define MAIN_RAM_BOX_H MAIN_RAM_H
#define MAIN_RAM_TEXT_Y (MAIN_RAM_Y + MAIN_RAM_H - 4)
/* Font baseline offsets (for alignment) */
#define BASELINE_OFFSET_BIG 13
#define BASELINE_OFFSET_TINY 7
/* Font vertical metrics (manual corrections for helvB10 / profont10) */
#define FONT_BIG_ASCENT 12 /* For helvB10 */
#define FONT_TINY_ASCENT 7 /* For profont10 */
#define LINE_HEIGHT_BIG 16 /* Minimum safe distance for Big Font lines */
#define LINE_HEIGHT_TINY 9 /* Minimum safe distance for Tiny Font lines */
#define FONT_BIG_HEIGHT 14 /* Legacy alias */
#define FONT_LBL_HEIGHT 8
#define ROW_GAP 2
#define NOCT_ROW_DY 14     /* Minimum for small fonts (grid law) */
#define NOCT_ROW_DY_BIG 18 /* Minimum for large fonts */
#define NOCT_CONTENT_MAX_ROWS 5
/* Card grid: unified layout for all scene screens */
#define NOCT_CARD_LEFT 2
#define NOCT_CARD_TOP NOCT_CONTENT_TOP
#define NOCT_CARD_ROW_DY NOCT_ROW_DY
#define NOCT_FOOTER_Y 50
/* Single-line footer text: top Y so that top + font height <= 64 */
#define NOCT_FOOTER_TEXT_Y 52
/* Mode screens (WiFi/BLE/Trap etc.): header bar height, match menu bar */
#define NOCT_MODE_HEADER_H 10
#define NOCT_LINE_H_DATA 12
#define NOCT_LINE_H_LABEL 5
#define NOCT_LINE_H_HEAD 10
#define NOCT_LINE_H_BIG 14
#define NOCT_TCP_LINE_MAX 4096
#define NOCT_TCP_CONNECT_TIMEOUT_MS 5000
#define NOCT_TCP_RECONNECT_INTERVAL_MS 2000
#define NOCT_SIGNAL_TIMEOUT_MS 5000
#define NOCT_SIGNAL_GRACE_MS 8000
#define NOCT_WIFI_RETRY_INTERVAL_MS 30000
#define NOCT_SPLASH_MS 2500
/* After this many ms in no-WiFi or linking state, show wolf screensaver */
#define NOCT_IDLE_SCREENSAVER_MS 10000
#define NOCT_TRANSITION_MS 180
#define NOCT_TRANSITION_STEP 32
#define NOCT_BUTTON_SHORT_MS 500
#define NOCT_BUTTON_LONG_MS 1000
#define NOCT_BUTTON_PREDATOR_MS 2500
#define NOCT_GLITCH_INTERVAL_MS 10000 /* Glitch once every 10s, subtle */
#define NOCT_GLITCH_DURATION_MS 100
/* GUI redraw interval (ms). 17 = ~60 FPS. */
#define NOCT_REDRAW_INTERVAL_MS 17 /* ~60 FPS */
#define NOCT_GRAPH_SAMPLES 32 /* Lower = less RAM; 32 enough for sparkline */
#define NOCT_GRAPH_HEIGHT 11

/* Menu: full screen (no header), box fills 128x64 */
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

/* Battery HUD in header (frame + terminal nub) */
#define NOCT_BAT_FRAME_W 16
#define NOCT_BAT_FRAME_H 8
#define NOCT_BAT_TERM_W 3
#define NOCT_BAT_TERM_H 3

/* RED ALERT thresholds (must match monitor.py) */
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

/* 9 screens: MAIN, CPU, GPU, RAM, DISKS, MEDIA, FANS, MB, WEATHER */
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

/* alert_metric codes (match monitor.py: ct, gt, cl, gl, gv, ram) */
#define NOCT_ALERT_CT 0
#define NOCT_ALERT_GT 1
#define NOCT_ALERT_CL 2
#define NOCT_ALERT_GL 3
#define NOCT_ALERT_GV 4
#define NOCT_ALERT_RAM 5

/* BMW E39 Assistant: I-Bus UART (9600 8E1). 0 = off (no UART, BLE key + list only). Set to 1 when I-Bus transceiver is connected. */
#define NOCT_IBUS_ENABLED 0
#define NOCT_IBUS_TX_PIN 39
#define NOCT_IBUS_RX_PIN 38

/* OBD-II / ELM327: 0 = off. When 1, ObdClient uses Serial2 on the pins below to request RPM/coolant/oil and call setObdData. */
#define NOCT_OBD_ENABLED 0
#define NOCT_OBD_TX_PIN 9
#define NOCT_OBD_RX_PIN 10

/* I-Bus poll interval (ms): rotate IKE status, GM door/lid, IKE ignition. */
#define NOCT_IBUS_POLL_INTERVAL_MS 3000

/* I-Bus full stream: when 1, log every received packet to Serial (hex). For debugging and reverse-engineering. */
#define NOCT_IBUS_MONITOR_VERBOSE 0

/* BMW Assistant debug: when 1, log BLE init, connection changes, and each "run" action to Serial (115200). Use for debugging BLE OFF / reboot on run. */
#define NOCT_BMW_DEBUG 1

/* BMW Assistant demo mode: when enabled from menu (prefs bmw_demo), BmwManager injects fake status so app can be tested without real I-Bus. Compile-time flag NOCT_BMW_DEMO_MODE no longer used for logic; interval below still used for demo injection. */
#define NOCT_BMW_DEMO_MODE 0
#define NOCT_BMW_DEMO_INTERVAL_MS 4000

/* USB CDC: 0 = off. When 1, device exposes virtual COM over built-in USB (GPIO19=D-, GPIO20=D+ per DataSheets/WiFi_LoRa_32_V4.2.0.pdf). */
#define NOCT_USB_CDC_ENABLED 0

/* A2DP Sink + I2S DAC: 0 = off. When 1, board acts as Bluetooth audio sink; PCM output on I2S pins to external DAC (e.g. PCM5102A) -> AUX. Pins per datasheet V4.2.0 (J3 free GPIOs). NOT SUPPORTED on Heltec V4 (ESP32-S3): ESP32-A2DP library supports original ESP32 only. */
#define NOCT_A2DP_SINK_ENABLED 0
#define NOCT_I2S_BCK_PIN 45  /* I2S bit clock -> DAC BCK */
#define NOCT_I2S_WS_PIN  46  /* I2S word select (LRCK) -> DAC LRCK */
#define NOCT_I2S_DOUT_PIN 40 /* I2S data out -> DAC DIN */

#endif
