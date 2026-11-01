/* AVR DU USB HID. Part of DxCore, LGPL-2.1-or-later. */
#pragma once
#include "CDC.h"
#if defined(DXCORE_USB_HID) && defined(DXCORE_USB_CDC)
namespace DxCoreUSB {
  constexpr uint8_t HID_KEYBOARD_INTERFACE = 2;
  constexpr uint8_t HID_MOUSE_INTERFACE = 3;
  constexpr uint16_t HID_KEYBOARD_REPORT_SIZE = 63;
  constexpr uint16_t HID_MOUSE_REPORT_SIZE = 52;

  struct __attribute__((packed)) HIDDescriptor {
    uint8_t length, type;
    uint16_t version;
    uint8_t country, count, reportType;
    uint16_t reportLength;
  };
  static_assert(sizeof(HIDDescriptor) == 9, "HID descriptor must be packed");
  void hid_reset();
  void hid_configure(uint8_t config);
  void hid_sof();
  size_t hid_descriptor(uint8_t *data, uint16_t interface, uint16_t value);
  bool hid_setup(const Setup_Packet &request, uint8_t *data, uint16_t &size);
  bool hid_set_output_request(const Setup_Packet &request);
  void hid_set_leds(uint8_t leds);
  uint8_t hid_leds();
  bool hid_send_keyboard(const uint8_t *report);
  bool hid_send_mouse(const uint8_t *report);
}
#endif
