/* Native USB keyboard for AVR DU. Part of DxCore, LGPL-2.1-or-later. */
#pragma once
#include <Arduino.h>
#if !defined(DXCORE_USB_CDC) || !defined(DXCORE_USB_HID)
  #error "Select Tools > USB Mode > CDC + Keyboard + Mouse on an AVR DU USB board."
#endif

#define KEY_LEFT_CTRL   0x80
#define KEY_LEFT_SHIFT  0x81
#define KEY_LEFT_ALT    0x82
#define KEY_LEFT_GUI    0x83
#define KEY_RIGHT_CTRL  0x84
#define KEY_RIGHT_SHIFT 0x85
#define KEY_RIGHT_ALT   0x86
#define KEY_RIGHT_GUI   0x87
#define KEY_UP_ARROW    0xDA
#define KEY_DOWN_ARROW  0xD9
#define KEY_LEFT_ARROW  0xD8
#define KEY_RIGHT_ARROW 0xD7
#define KEY_BACKSPACE   0xB2
#define KEY_TAB         0xB3
#define KEY_RETURN      0xB0
#define KEY_ESC         0xB1
#define KEY_INSERT      0xD1
#define KEY_DELETE      0xD4
#define KEY_PAGE_UP     0xD3
#define KEY_PAGE_DOWN   0xD6
#define KEY_HOME        0xD2
#define KEY_END         0xD5
#define KEY_CAPS_LOCK   0xC1
#define KEY_PRINT_SCREEN 0xCE
#define KEY_SCROLL_LOCK 0xCF
#define KEY_PAUSE       0xD0
#define KEY_NUM_LOCK    0xDB
#define KEY_MENU        0xED
#define KEY_F1  0xC2
#define KEY_F2  0xC3
#define KEY_F3  0xC4
#define KEY_F4  0xC5
#define KEY_F5  0xC6
#define KEY_F6  0xC7
#define KEY_F7  0xC8
#define KEY_F8  0xC9
#define KEY_F9  0xCA
#define KEY_F10 0xCB
#define KEY_F11 0xCC
#define KEY_F12 0xCD
#define KEY_F13 0xF0
#define KEY_F14 0xF1
#define KEY_F15 0xF2
#define KEY_F16 0xF3
#define KEY_F17 0xF4
#define KEY_F18 0xF5
#define KEY_F19 0xF6
#define KEY_F20 0xF7
#define KEY_F21 0xF8
#define KEY_F22 0xF9
#define KEY_F23 0xFA
#define KEY_F24 0xFB

class Keyboard_ : public Print {
  private:
    uint8_t report[8] = {};
    uint8_t explicitModifiers = 0;
    uint8_t shiftedKeys = 0;
    size_t update(uint8_t key, bool down);
  public:
    void begin();
    void end();
    size_t press(uint8_t key);
    size_t release(uint8_t key);
    void releaseAll();
    size_t write(uint8_t key) override;
    using Print::write;
    uint8_t getLeds(); // Bit 0 Num Lock, bit 1 Caps Lock, bit 2 Scroll Lock.
    explicit operator bool();
};
extern Keyboard_ Keyboard;
