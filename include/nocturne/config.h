/*
 * NOCTURNE_OS — Config (non-secret)
 * Pins, dimensions, timeouts. Secrets live in secrets.h.
 */
#ifndef NOCTURNE_CONFIG_H
#define NOCTURNE_CONFIG_H

// Hardware pins (Heltec WiFi LoRa 32 V3)
#define NOCT_SDA_PIN 17
#define NOCT_SCL_PIN 18
#define NOCT_RST_PIN 21
#define NOCT_VEXT_PIN 36
#define NOCT_LED_PIN 35
#define NOCT_BUTTON_PIN 0

#define NOCT_DISP_W 128
#define NOCT_DISP_H 64
#define NOCT_MARGIN 2

// Safe zones for scene content (pixels)
#define NOCT_SAFE_TOP 18    // below header
#define NOCT_SAFE_BOTTOM 48 // above graph and footer
#define NOCT_FOOTER_Y 50    // start of footer (graph + indicators)
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
#define NOCT_MENU_TIMEOUT_MS (5 * 60 * 1000)
#define NOCT_SPLASH_MS 2000
#define NOCT_HEARTBEAT_INTERVAL_MS 2000

#ifndef CPU_TEMP_ALERT
#define CPU_TEMP_ALERT 87
#endif
#ifndef GPU_TEMP_ALERT
#define GPU_TEMP_ALERT 72
#endif

// Scene indices
#define NOCT_SCENE_CORTEX 0
#define NOCT_SCENE_NEURAL 1
#define NOCT_SCENE_THERMAL 2
#define NOCT_SCENE_MEMBANK 3
#define NOCT_SCENE_TASKKILL 4
#define NOCT_SCENE_DECK 5
#define NOCT_SCENE_WEATHER 6
#define NOCT_SCENE_RADAR 7
#define NOCT_TOTAL_SCENES 8

// Long-press threshold for menu vs Predator (ms)
#define NOCT_BUTTON_MENU_MS 800
#define NOCT_BUTTON_PREDATOR_MS 2500

#endif
