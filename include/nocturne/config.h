/*
 * NOCTURNE_OS — Config: pins (SDA/SCL/RST), display 128x64, timeouts,
 * NOCT_SCENE_* indices, NOCT_GRAPH_SAMPLES. WiFi/PC IP in secrets.h.
 */
#ifndef NOCTURNE_CONFIG_H
#define NOCTURNE_CONFIG_H

#define NOCT_SDA_PIN 17
#define NOCT_SCL_PIN 18
#define NOCT_RST_PIN 21
#define NOCT_VEXT_PIN 36
#define NOCT_LED_CPU_PIN 35 /* White: CPU temp alert */
#define NOCT_LED_GPU_PIN 4  /* Red: GPU temp alert */
#define NOCT_BUTTON_PIN 0
#define NOCT_ALERT_LED_BLINK_MAX_MS 5000

#define NOCT_DISP_W 128
#define NOCT_DISP_H 64
#define NOCT_MARGIN 2
/* Header: zone Y 0..11 (Height 12); Content Area: 14..63 */
#define NOCT_HEADER_H 12
#define NOCT_HEADER_MARGIN 2
#define NOCT_CONTENT_START 14
#define NOCT_CONTENT_TOP NOCT_CONTENT_START
#define NOCT_FOOTER_H 16
/* Main scene: top block 13..45, RAM box 48..63 */
#define MAIN_TOP_Y_END 45
#define MAIN_RAM_BOX_TOP 48
#define MAIN_RAM_BOX_H 16
#define MAIN_RAM_TEXT_Y 58
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
#define NOCT_TRANSITION_MS 180
#define NOCT_TRANSITION_STEP 4
#define NOCT_BUTTON_SHORT_MS 500
#define NOCT_BUTTON_LONG_MS 1000
#define NOCT_BUTTON_PREDATOR_MS 2500
#define NOCT_GLITCH_INTERVAL_MS 10000 /* Glitch once every 10s, subtle */
#define NOCT_GLITCH_DURATION_MS 100
#define NOCT_GRAPH_SAMPLES 32 /* Lower = less RAM; 32 enough for sparkline */
#define NOCT_GRAPH_HEIGHT 11

/* RED ALERT thresholds (must match monitor.py) */
#ifndef CPU_TEMP_ALERT
#define CPU_TEMP_ALERT 87
#endif
#ifndef GPU_TEMP_ALERT
#define GPU_TEMP_ALERT 68
#endif
#define CPU_LOAD_ALERT 99
#define GPU_LOAD_ALERT 99
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

#endif
