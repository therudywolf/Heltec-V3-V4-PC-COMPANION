/*
 * NOCTURNE_OS — A2DP Sink: receive Bluetooth audio from phone, output via I2S to external DAC.
 * Only active when NOCT_A2DP_SINK_ENABLED and in BMW Assistant mode.
 * Pins per config.h (NOCT_I2S_BCK_PIN, NOCT_I2S_WS_PIN, NOCT_I2S_DOUT_PIN).
 */
#ifndef NOCTURNE_A2DP_SINK_H
#define NOCTURNE_A2DP_SINK_H

class A2dpSink {
 public:
  A2dpSink() = default;
  void begin();
  void end();
  bool isActive() const { return active_; }

 private:
  bool active_ = false;
};

#endif
