/*
 * Ring buffer for I-Bus byte stream (ESP32-compatible).
 */
#include "RingBuffer.h"
#include <cstring>

RingBuffer::RingBuffer(int size) {
  bufferSize = size;
  bufferTail = 0;
  bufferHead = 0;
  buffer_p = (uint8_t *)malloc(size);
  if (buffer_p)
    memset(buffer_p, 0, size);
}

RingBuffer::~RingBuffer() {
  if (buffer_p)
    free(buffer_p);
}

int RingBuffer::available(void) {
  return (bufferSize + bufferHead - bufferTail) % bufferSize;
}

int RingBuffer::read(void) {
  if (bufferHead == bufferTail)
    return -1;
  uint8_t c = buffer_p[bufferTail];
  bufferTail = (bufferTail + 1) % bufferSize;
  if (bufferHead == bufferTail) {
    bufferTail = 0;
    bufferHead = 0;
  }
  return (int)c;
}

uint8_t RingBuffer::write(int c) {
  if ((bufferHead + 1) % bufferSize == bufferTail)
    return 0xFF;
  buffer_p[bufferHead] = (uint8_t)c;
  bufferHead = (bufferHead + 1) % bufferSize;
  return 0;
}

void RingBuffer::remove(int n) {
  if (bufferHead != bufferTail)
    bufferTail = (bufferTail + n) % bufferSize;
}

int RingBuffer::peek(void) {
  if (bufferHead == bufferTail)
    return -1;
  return (int)buffer_p[bufferTail];
}

int RingBuffer::peek(int offset) {
  if (bufferHead == bufferTail)
    return -1;
  return (int)buffer_p[(bufferTail + offset) % bufferSize];
}
