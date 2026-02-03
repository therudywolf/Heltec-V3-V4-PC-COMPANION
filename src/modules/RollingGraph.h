/*
 * NOCTURNE_OS — Rolling sparkline (CPU/GPU/Net history)
 */
#ifndef NOCTURNE_ROLLING_GRAPH_H
#define NOCTURNE_ROLLING_GRAPH_H

#include "../../include/nocturne/config.h"

class RollingGraph {
public:
  RollingGraph();
  void push(float value);
  void setMax(int max);
  void clear();

  float values[NOCT_GRAPH_SAMPLES];
  int count;
  int maxVal;
  int head;
};

#endif
