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
#define NOCT_LED_PIN 35
#define NOCT_BUTTON_PIN 0

#define NOCT_DISP_W 128
#define NOCT_DISP_H 64
#define NOCT_MARGIN 2
#define NOCT_HEADER_H 9 /* Grid: header 0..8, dotted line at Y=9 */
/* Card grid: unified layout for all scene screens */
#define NOCT_CARD_LEFT 2
#define NOCT_CARD_TOP 12
#define NOCT_CARD_ROW_DY 12
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
#define NOCT_SPLASH_MS 2000
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

/* 6 screens: MAIN, CPU, GPU, RAM, DISKS, MEDIA */
#define NOCT_SCENE_MAIN 0
#define NOCT_SCENE_CPU 1
#define NOCT_SCENE_GPU 2
#define NOCT_SCENE_RAM 3
#define NOCT_SCENE_DISKS 4
#define NOCT_SCENE_MEDIA 5
#define NOCT_TOTAL_SCENES 6

/* alert_metric codes (match monitor.py: ct, gt, cl, gl, gv, ram) */
#define NOCT_ALERT_CT 0
#define NOCT_ALERT_GT 1
#define NOCT_ALERT_CL 2
#define NOCT_ALERT_GL 3
#define NOCT_ALERT_GV 4
#define NOCT_ALERT_RAM 5

#endif
