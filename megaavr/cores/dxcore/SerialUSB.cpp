/* Native USB CDC for AVR DU. Part of DxCore, LGPL-2.1-or-later. */
#include <Arduino.h>

#if defined(DXCORE_USB_CDC)
#include "usb/CDC.h"

SerialUSBClass SerialUSB;

void SerialUSBClass::begin() {
  DxCoreUSB::start();
}
void SerialUSBClass::end() {
  DxCoreUSB::stop();
}
int SerialUSBClass::available() {
  return DxCoreUSB::read_available();
}
int SerialUSBClass::availableForWrite() {
  return DxCoreUSB::write_available();
}
int SerialUSBClass::read() {
  return DxCoreUSB::read_byte();
}
int SerialUSBClass::peek() {
  return DxCoreUSB::peek_byte();
}
size_t SerialUSBClass::write(uint8_t value) {
  return DxCoreUSB::write_byte(value);
}
size_t SerialUSBClass::write(const uint8_t *buffer, size_t size) {
  size_t written = 0;
  while (written < size && write(buffer[written])) {
    ++written;
  }
  return written;
}
void SerialUSBClass::flush() {
  DxCoreUSB::write_flush();
}
SerialUSBClass::operator bool() {
  return DxCoreUSB::is_ready() && (DxCoreUSB::get_state()->bLineState & 1);
}

ISR(USB0_BUSEVENT_vect) {
  DxCoreUSB::handling_bus_events();
}
ISR(USB0_TRNCOMPL_vect) {
  DxCoreUSB::handling_transaction_events();
}
#endif
