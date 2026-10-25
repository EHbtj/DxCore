/* Native USB CDC for AVR DU. Part of DxCore, LGPL-2.1-or-later. */
#pragma once

#if defined(DXCORE_USB_CDC)
#include "api/Stream.h"

class SerialUSBClass : public Stream {
public:
  void begin();
  void begin(unsigned long baud) { (void)baud; begin(); }
  void end();
  int available() override;
  int availableForWrite() override;
  int read() override;
  int peek() override;
  size_t write(uint8_t value) override;
  size_t write(const uint8_t *buffer, size_t size) override;
  using Print::write;
  void flush() override;
  explicit operator bool();
};

extern SerialUSBClass SerialUSB;
#endif
