/* Native USB mouse for AVR DU. Part of DxCore, LGPL-2.1-or-later. */
#pragma once
#include <Arduino.h>
#if !defined(DXCORE_USB_CDC) || !defined(DXCORE_USB_HID)
  #error "Select Tools > USB Mode > CDC + Keyboard + Mouse on an AVR DU USB board."
#endif
#define MOUSE_LEFT   1
#define MOUSE_RIGHT  2
#define MOUSE_MIDDLE 4
#define MOUSE_ALL    7

class Mouse_ {
  private:
    uint8_t buttons = 0;
    bool send(uint8_t next, int8_t x, int8_t y, int8_t wheel);
  public:
    void begin();
    void end();
    void click(uint8_t button = MOUSE_LEFT);
    void move(int8_t x, int8_t y, int8_t wheel = 0);
    void press(uint8_t button = MOUSE_LEFT);
    void release(uint8_t button = MOUSE_LEFT);
    bool isPressed(uint8_t button = MOUSE_LEFT);
    explicit operator bool();
};
extern Mouse_ Mouse;
