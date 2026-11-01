/* AVR DU USB HID. Part of DxCore, LGPL-2.1-or-later. */
#include <avr/io.h>
#if defined(DXCORE_USB_HID) && defined(DXCORE_USB_CDC) && defined(USB0)
#include <avr/pgmspace.h>
#include <string.h>
#include "HID.h"
#include "CDC/constants.h"
#include "USBConfig.h"

namespace DxCoreUSB {
  // Separate boot interfaces, without report IDs. Keyboard: modifiers/reserved/6 keys.
  const uint8_t keyboard_report_descriptor[] PROGMEM = {
    0x05, 0x01, 0x09, 0x06, 0xa1, 0x01,
    0x05, 0x07, 0x19, 0xe0, 0x29, 0xe7, 0x15, 0x00, 0x25, 0x01,
    0x75, 0x01, 0x95, 0x08, 0x81, 0x02,
    0x95, 0x01, 0x75, 0x08, 0x81, 0x01,
    0x95, 0x05, 0x75, 0x01, 0x05, 0x08, 0x19, 0x01, 0x29, 0x05, 0x91, 0x02,
    0x95, 0x01, 0x75, 0x03, 0x91, 0x01,
    0x95, 0x06, 0x75, 0x08, 0x15, 0x00, 0x25, 0x73,
    0x05, 0x07, 0x19, 0x00, 0x29, 0x73, 0x81, 0x00, 0xc0
  };
  // Report protocol: buttons/X/Y/wheel. Boot protocol omits the wheel byte.
  const uint8_t mouse_report_descriptor[] PROGMEM = {
    0x05, 0x01, 0x09, 0x02, 0xa1, 0x01, 0x09, 0x01, 0xa1, 0x00,
    0x05, 0x09, 0x19, 0x01, 0x29, 0x03, 0x15, 0x00, 0x25, 0x01,
    0x95, 0x03, 0x75, 0x01, 0x81, 0x02,
    0x95, 0x01, 0x75, 0x05, 0x81, 0x01,
    0x05, 0x01, 0x09, 0x30, 0x09, 0x31, 0x09, 0x38,
    0x15, 0x81, 0x25, 0x7f, 0x75, 0x08, 0x95, 0x03, 0x81, 0x06, 0xc0, 0xc0
  };
  static_assert(sizeof(keyboard_report_descriptor) == HID_KEYBOARD_REPORT_SIZE, "Keyboard report length");
  static_assert(sizeof(mouse_report_descriptor) == HID_MOUSE_REPORT_SIZE, "Mouse report length");

  struct HIDState {
    uint8_t report[8];
    uint8_t protocol;
    uint8_t idle;
    uint16_t elapsed;
  };
  static HIDState states[2];
  static volatile uint8_t keyboard_leds;

  static uint8_t endpoint(uint8_t index) {
    return index ? USB_EP_MOUSE_IN : USB_EP_KEYBOARD_IN;
  }
  static uint8_t report_size(uint8_t index) {
    return index ? (states[1].protocol ? 4 : 3) : 8;
  }
  static bool available(uint8_t index) {
    return is_ready() && !(USB_EP(endpoint(index)).CTRL & USB_DOSTALL_bm) &&
           (USB_EP(endpoint(index)).STATUS & USB_BUSNAK_bm);
  }
  static void queue(uint8_t index, const uint8_t *report) {
    uint8_t *buffer = index ? get_workmem_ptr()->mouse : get_workmem_ptr()->keyboard;
    memcpy(buffer, report, report_size(index));
    USB_EP(endpoint(index)).CNT = report_size(index);
    USB_EP(endpoint(index)).MCNT = 0;
    loop_until_bit_is_clear(USB0_INTFLAGSB, USB_RMWBUSY_bp);
    USB_EP_STATUS_CLR(endpoint(index)) = ~USB_TOGGLE_bm;
    states[index].elapsed = 0;
  }
  void hid_reset() {
    memset(states, 0, sizeof(states));
    states[0].protocol = states[1].protocol = 1;
    keyboard_leds = 0;
  }
  void hid_configure(uint8_t config) {
    hid_reset();
    for (uint8_t i = 0; i < 2; ++i) {
      auto &ep = USB_EP(endpoint(i));
      ep.CTRL = USB_TYPE_DISABLE_gc;
      // Unused OUT halves remain disabled after bus reset.
      if (config) {
        ep.STATUS = USB_BUSNAK_bm;
        ep.CNT = ep.MCNT = 0;
        ep.DATAPTR = (uint16_t)(i ? get_workmem_ptr()->mouse : get_workmem_ptr()->keyboard);
        ep.CTRL = USB_TYPE_BULKINT_gc | USB_TCDSBL_bm | USB_EP_SIZE_gc(8);
      }
    }
  }
  size_t hid_descriptor(uint8_t *data, uint16_t interface, uint16_t value) {
    if (interface != HID_KEYBOARD_INTERFACE && interface != HID_MOUSE_INTERFACE) {
      return 0;
    }
    bool mouse = interface == HID_MOUSE_INTERFACE;
    uint16_t length = mouse ? HID_MOUSE_REPORT_SIZE : HID_KEYBOARD_REPORT_SIZE;
    if (value == 0x2200) {
      memcpy_P(data, mouse ? mouse_report_descriptor : keyboard_report_descriptor, length);
      return length;
    }
    if (value == 0x2100) {
      HIDDescriptor descriptor = {9, 0x21, 0x0111, 0, 1, 0x22, length};
      memcpy(data, &descriptor, sizeof(descriptor));
      return sizeof(descriptor);
    }
    return 0;
  }
  bool hid_set_output_request(const Setup_Packet &request) {
    return request.bmRequestType == 0x21 && request.bRequest == 9 &&
           request.wIndex == HID_KEYBOARD_INTERFACE && request.wValue == 0x0200 &&
           request.wLength == 1;
  }
  void hid_set_leds(uint8_t leds) {
    keyboard_leds = leds & 0x1f;
  }
  uint8_t hid_leds() {
    return keyboard_leds;
  }
  bool hid_setup(const Setup_Packet &request, uint8_t *data, uint16_t &size) {
    if (request.wIndex != HID_KEYBOARD_INTERFACE && request.wIndex != HID_MOUSE_INTERFACE) {
      return false;
    }
    uint8_t index = request.wIndex - HID_KEYBOARD_INTERFACE;
    auto &state = states[index];
    if (request.bmRequestType == 0xa1) {
      switch (request.bRequest) {
        case 1: // GET_REPORT: report ID must be zero.
          if (request.wValue == 0x0100) {
            size = report_size(index);
            memcpy(data, state.report, size);
            return true;
          }
          if (!index && request.wValue == 0x0200) {
            data[0] = keyboard_leds;
            size = 1;
            return true;
          }
          return false;
        case 2: // GET_IDLE
          if (request.wValue || request.wLength != 1) {
            return false;
          }
          data[0] = state.idle;
          size = 1;
          return true;
        case 3: // GET_PROTOCOL
          if (request.wValue || request.wLength != 1) {
            return false;
          }
          data[0] = state.protocol;
          size = 1;
          return true;
      }
    } else if (request.bmRequestType == 0x21 && !request.wLength) {
      if (request.bRequest == 10 && !(request.wValue & 0xff)) { // SET_IDLE
        state.idle = request.wValue >> 8;
        state.elapsed = 0;
        return true;
      }
      if (request.bRequest == 11 && request.wValue <= 1) { // SET_PROTOCOL
        state.protocol = request.wValue;
        // Discard an in-flight report in the old protocol before accepting new reports.
        loop_until_bit_is_clear(USB0_INTFLAGSB, USB_RMWBUSY_bp);
        USB_EP_STATUS_SET(endpoint(index)) = USB_BUSNAK_bm;
        loop_until_bit_is_clear(USB0_INTFLAGSB, USB_RMWBUSY_bp);
        USB_EP(endpoint(index)).CNT = 0;
        state.elapsed = 0;
        return true;
      }
    }
    return false;
  }
  void hid_sof() {
    if (!is_ready()) {
      return;
    }
    for (uint8_t i = 0; i < 2; ++i) {
      auto &state = states[i];
      if (state.idle) {
        uint16_t interval = (uint16_t)state.idle * 4;
        if (state.elapsed < interval) {
          ++state.elapsed;
        }
        if (state.elapsed >= interval && available(i)) {
          queue(i, state.report);
        }
      }
    }
  }
  static bool send(uint8_t index, const uint8_t *report) {
    if (!(SREG & CPU_I_bm)) {
      return false;
    }
    for (uint16_t tick = 0; tick < USB_TX_TIMEOUT_MS * 10U; ++tick) {
      if (!is_ready() || (USB_EP(endpoint(index)).CTRL & USB_DOSTALL_bm)) {
        return false;
      }
      bool sent = false;
      ATOMIC_BLOCK(ATOMIC_RESTORESTATE) {
        if (available(index)) {
          queue(index, report);
          memcpy(states[index].report, report, index ? 4 : 8);
          if (index) {
            // Relative movement must never be repeated by GET_REPORT or idle.
            memset(states[index].report + 1, 0, 3);
          }
          sent = true;
        }
      }
      if (sent) {
        return true;
      }
      _delay_us(100);
    }
    return false;
  }
  bool hid_send_keyboard(const uint8_t *report) {
    return send(0, report);
  }
  bool hid_send_mouse(const uint8_t *report) {
    return send(1, report);
  }
}
#endif
