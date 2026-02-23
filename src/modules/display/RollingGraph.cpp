/*
 * NOCTURNE_OS — Rolling sparkline implementation
 */
#include "RollingGraph.h"

RollingGraph::RollingGraph() : head(0), count(0), maxVal(100) {
  for (int i = 0; i < NOCT_GRAPH_SAMPLES; i++)
    values[i] = 0.0f;
}

void RollingGraph::push(float value) {
  if (value < 0.0f)
    value = 0.0f;
  if (value > (float)maxVal)
    value = (float)maxVal;
  values[head] = value;
  head = (head + 1) % NOCT_GRAPH_SAMPLES;
  if (count < NOCT_GRAPH_SAMPLES)
    count++;
}

void RollingGraph::setMax(int max) {
  if (max < 1)
    max = 1;
  maxVal = max;
}

void RollingGraph::clear() {
  head = 0;
  count = 0;
  for (int i = 0; i < NOCT_GRAPH_SAMPLES; i++)
    values[i] = 0.0f;
}
