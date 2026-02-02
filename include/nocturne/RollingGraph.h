/*
 * NOCTURNE_OS — Rolling sparkline (CPU/Net history at bottom of screen)
 */
#ifndef NOCTURNE_ROLLING_GRAPH_H
#define NOCTURNE_ROLLING_GRAPH_H

#include "config.h"

#define NOCT_GRAPH_SAMPLES 64
#define NOCT_GRAPH_HEIGHT 11

class RollingGraph {
public:
  RollingGraph();
  void push(float value);  // 0..100 or raw; we normalize for display
  void pushRaw(int value); // e.g. KB/s for net
  void setMax(int max);    // for scaling (e.g. 100 for %, 1024 for KB/s)
  void draw(int x, int y, int w,
            int h); // draw into region (uses U8g2 externally or we need ref)
  void clear();

  // Direct buffer for external drawing (DisplayEngine reads these)
  float values[NOCT_GRAPH_SAMPLES];
  int count;
  int maxVal;
  int head;
};

#endif
