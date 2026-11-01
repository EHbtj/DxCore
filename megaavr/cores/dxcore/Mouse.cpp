/* Native USB mouse for AVR DU. Part of DxCore, LGPL-2.1-or-later. */
#include <Arduino.h>
#if defined(DXCORE_USB_CDC) && defined(DXCORE_USB_HID)
#include "Mouse.h"
#include "usb/HID.h"

Mouse_ Mouse;
void Mouse_::begin() {
  DxCoreUSB::start();
}
void Mouse_::end() {
  release(MOUSE_ALL);
}
Mouse_::operator bool() {
  return DxCoreUSB::is_ready();
}
bool Mouse_::send(uint8_t next, int8_t x, int8_t y, int8_t wheel) {
  // -128 is outside the descriptor's logical range.
  uint8_t report[] = {uint8_t(next & MOUSE_ALL), uint8_t(x == -128 ? -127 : x),
                      uint8_t(y == -128 ? -127 : y), uint8_t(wheel == -128 ? -127 : wheel)};
  if (!DxCoreUSB::hid_send_mouse(report)) {
    return false;
  }
  buttons = next & MOUSE_ALL;
  return true;
}
void Mouse_::move(int8_t x, int8_t y, int8_t wheel) {
  send(buttons, x, y, wheel);
}
void Mouse_::press(uint8_t button) {
  send(buttons | button, 0, 0, 0);
}
void Mouse_::release(uint8_t button) {
  send(buttons & ~button, 0, 0, 0);
}
void Mouse_::click(uint8_t button) {
  uint8_t previous = buttons;
  if (send(previous | button, 0, 0, 0)) {
    send(previous, 0, 0, 0);
  }
}
bool Mouse_::isPressed(uint8_t button) {
  return buttons & button & MOUSE_ALL;
}
#endif
