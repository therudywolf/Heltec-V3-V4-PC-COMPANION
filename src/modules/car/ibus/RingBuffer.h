/*
 * Ring buffer for I-Bus byte stream (ESP32-compatible).
 */
#ifndef RINGBUFFER_H
#define RINGBUFFER_H

#include <Arduino.h>

class RingBuffer {
 public:
  explicit RingBuffer(int size);
  ~RingBuffer();
  int available(void);
  int peek(void);
  int peek(int offset);
  void remove(int n);
  int read(void);
  uint8_t write(int c);

 private:
  int bufferSize;
  unsigned int bufferHead, bufferTail;
  uint8_t *buffer_p;
};

#endif
