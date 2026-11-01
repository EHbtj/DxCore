/* Minimal Print contract for exercising the production Keyboard/Mouse APIs. */
#pragma once
#include <avr/io.h>
class Print {
  int error = 0;
protected:
  void setWriteError(int value = 1) { error = value; }
public:
  int getWriteError() { return error; }
  void clearWriteError() { error = 0; }
  virtual size_t write(uint8_t) = 0;
  size_t write(const char *text) {
    size_t count = 0;
    while (*text && write((uint8_t)*text++)) { ++count; }
    return count;
  }
};
