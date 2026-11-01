/* Native USB keyboard for AVR DU. Part of DxCore, LGPL-2.1-or-later. */
#include <Arduino.h>
#if defined(DXCORE_USB_CDC) && defined(DXCORE_USB_HID)
#include "Keyboard.h"
#include "usb/HID.h"
#include <string.h>

Keyboard_ Keyboard;

// US ASCII to Keyboard/Keypad usage. Bit 7 indicates an implicit left shift.
static uint8_t asciiUsage(uint8_t key) {
  if (key >= 'a' && key <= 'z') {
    return key - 'a' + 4;
  }
  if (key >= 'A' && key <= 'Z') {
    return (key - 'A' + 4) | 0x80;
  }
  if (key >= '1' && key <= '9') {
    return key - '1' + 0x1e;
  }
  switch (key) {
    case '0': return 0x27;
    case '\n': return 0x28;
    case '\b': return 0x2a;
    case '\t': return 0x2b;
    case ' ': return 0x2c;
    case '-': return 0x2d;
    case '_': return 0xad;
    case '=': return 0x2e;
    case '+': return 0xae;
    case '[': return 0x2f;
    case '{': return 0xaf;
    case ']': return 0x30;
    case '}': return 0xb0;
    case '\\': return 0x31;
    case '|': return 0xb1;
    case ';': return 0x33;
    case ':': return 0xb3;
    case '\'': return 0x34;
    case '"': return 0xb4;
    case '`': return 0x35;
    case '~': return 0xb5;
    case ',': return 0x36;
    case '<': return 0xb6;
    case '.': return 0x37;
    case '>': return 0xb7;
    case '/': return 0x38;
    case '?': return 0xb8;
    case '!': return 0x9e;
    case '@': return 0x9f;
    case '#': return 0xa0;
    case '$': return 0xa1;
    case '%': return 0xa2;
    case '^': return 0xa3;
    case '&': return 0xa4;
    case '*': return 0xa5;
    case '(': return 0xa6;
    case ')': return 0xa7;
    default: return 0;
  }
}

void Keyboard_::begin() {
  DxCoreUSB::start();
}
void Keyboard_::end() {
  releaseAll();
}
Keyboard_::operator bool() {
  return DxCoreUSB::is_ready();
}
uint8_t Keyboard_::getLeds() {
  return DxCoreUSB::hid_leds();
}
size_t Keyboard_::update(uint8_t key, bool down) {
  uint8_t next[8];
  memcpy(next, report, sizeof(next));
  uint8_t modifiers = explicitModifiers;
  uint8_t shifted = shiftedKeys;
  if (key >= 0x80 && key < 0x88) {
    uint8_t mask = 1 << (key - 0x80);
    modifiers = down ? modifiers | mask : modifiers & ~mask;
  } else {
    uint8_t usage = key >= 0x88 ? key - 0x88 : asciiUsage(key);
    bool shift = key < 0x80 && (usage & 0x80);
    usage &= 0x7f;
    if (!usage || usage > 0x73) {
      setWriteError();
      return 0;
    }
    uint8_t slot = 0;
    while (slot < 6 && next[slot + 2] != usage) {
      ++slot;
    }
    if (down && slot == 6) {
      slot = 0;
      while (slot < 6 && next[slot + 2]) {
        ++slot;
      }
      if (slot == 6) {
        setWriteError();
        return 0;
      }
    }
    if (slot < 6) {
      next[slot + 2] = down ? usage : 0;
      if (down && shift) {
        shifted |= 1 << slot;
      } else if (!down) {
        shifted &= ~(1 << slot);
      }
    }
  }
  next[0] = modifiers | (shifted ? 2 : 0);
  if (!DxCoreUSB::hid_send_keyboard(next)) {
    setWriteError();
    return 0;
  }
  memcpy(report, next, sizeof(report));
  explicitModifiers = modifiers;
  shiftedKeys = shifted;
  return 1;
}
size_t Keyboard_::press(uint8_t key) {
  return update(key, true);
}
size_t Keyboard_::release(uint8_t key) {
  return update(key, false);
}
void Keyboard_::releaseAll() {
  uint8_t empty[8] = {};
  if (!DxCoreUSB::hid_send_keyboard(empty)) {
    setWriteError();
  }
  memset(report, 0, sizeof(report));
  explicitModifiers = shiftedKeys = 0;
}
size_t Keyboard_::write(uint8_t key) {
  if (key == '\r') {
    return 1; // Print::println supplies CR/LF; send a single Enter.
  }
  if (!press(key)) {
    return 0;
  }
  return release(key);
}
#endif
