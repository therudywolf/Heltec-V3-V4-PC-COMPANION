/*
 * NOCTURNE_OS — A2DP Sink implementation (I2S to external DAC).
 */
#include "A2dpSink.h"
#include "nocturne/config.h"

#if NOCT_A2DP_SINK_ENABLED
#include "BluetoothA2DPSink.h"

static BluetoothA2DPSink s_a2dp_sink;
#endif

void A2dpSink::begin() {
#if NOCT_A2DP_SINK_ENABLED
  if (active_)
    return;
  i2s_pin_config_t pin_config = {
    .mck_io_num = -1,
    .bck_io_num = NOCT_I2S_BCK_PIN,
    .ws_io_num = NOCT_I2S_WS_PIN,
    .data_out_num = NOCT_I2S_DOUT_PIN,
    .data_in_num = -1
  };
  s_a2dp_sink.set_pin_config(pin_config);
  s_a2dp_sink.start("BMW E39 Audio");
  active_ = true;
#endif
}

void A2dpSink::end() {
#if NOCT_A2DP_SINK_ENABLED
  if (!active_)
    return;
  s_a2dp_sink.end();
  active_ = false;
#endif
}
