/**
 * USB CDC data endpoints, adapted for DxCore.
 * Copyright (c) 2024 askn37 at github.com. MIT License (see ../LICENSE).
 * Based on MacroMicroAPI_lib 1804a5239181f33b2f07ab7bac4c83bfc74d9c8b.
 * DxCore: nonblocking receive, bounded transmit, SOF service, no SDK dependency.
 */
#include <avr/io.h>
#if defined(DXCORE_USB_CDC) && defined(USB0)
#include <string.h>
#include "../../CDC.h"
#include "../constants.h"
#include "../../USBConfig.h"

namespace USB_NAMESPACE {
  Interface_State USB_STATE;
  static volatile bool running;
  static volatile bool suspended;

  Interface_State* get_state() { return &USB_STATE; }
  LineState_t& get_line_state() { return USBSTATE.bmLineState; }
  LineEncoding_t& get_line_encoding() { return USBSTATE.LineEncoding; }
  uint8_t cb_get_configuration() { return USBSTATE.CONFIG; }
  void cb_clear_state() {
    USB_STATE = Interface_State{};
    suspended = false;
  }
  bool is_ready() { return running && !suspended && USBSTATE.CONFIG == 1; }
  bool is_busy() { return !is_ready(); }
  bool is_recv_ready() {
    return !(USB_EP_RECV.CTRL & USB_DOSTALL_bm) && (USB_EP_RECV.STATUS & USB_BUSNAK_bm);
  }
  bool is_send_ready() {
    return !(USB_EP_SEND.CTRL & USB_DOSTALL_bm) && (USB_EP_SEND.STATUS & USB_BUSNAK_bm);
  }

  static void release_endpoint(uint8_t endpoint) {
    loop_until_bit_is_clear(USB0_INTFLAGSB, USB_RMWBUSY_bp);
    USB_EP_STATUS_CLR(endpoint) = ~USB_TOGGLE_bm;
  }

  static void arm_receive() {
    USBSTATE.RECVCNT = 0;
    USB_EP_RECV.CNT = 0;
    release_endpoint(USB_EP_BULK_OUT);
  }

  static void send_buffer() {
    USB_EP_SEND.CNT = USBSTATE.SENDCNT;
    USB_EP_SEND.MCNT = 0;
    USBSTATE.SENDCNT = 0;
    release_endpoint(USB_EP_BULK_IN);
  }

  bool cb_set_configuration(uint8_t config) {
    if (config > 1) return false;
    USBSTATE.CONFIG = config;
    USBSTATE.SENDCNT = USBSTATE.RECVCNT = 0;
    USBSTATE.bLineState = 0;
    USB_EP_NOOP.CTRL = USB_TYPE_DISABLE_gc;
    USB_EP_INTR.CTRL = USB_TYPE_DISABLE_gc;
    USB_EP_RECV.CTRL = USB_TYPE_DISABLE_gc;
    USB_EP_SEND.CTRL = USB_TYPE_DISABLE_gc;
    if (!config) return true;

    USB_EP_INTR.STATUS = USB_BUSNAK_bm;
    USB_EP_INTR.CNT = USB_EP_INTR.MCNT = 0;
    USB_EP_INTR.DATAPTR = (uint16_t)&USB_INTR_BUFFER;
    USB_EP_INTR.CTRL = USB_TYPE_BULKINT_gc | USB_TCDSBL_bm | USB_EP_SIZE_gc(USB_INTR_PK_SIZE);

    USB_EP_RECV.STATUS = USB_BUSNAK_bm;
    USB_EP_RECV.CNT = 0;
    USB_EP_RECV.DATAPTR = (uint16_t)&USB_RECV_BUFFER;
    USB_EP_RECV.MCNT = USB_BULK_RECV_MAX;
    USB_EP_RECV.CTRL = USB_TYPE_BULKINT_gc | USB_TCDSBL_bm | USB_EP_SIZE_gc(USB_BULK_RECV_MAX);

    USB_EP_SEND.STATUS = USB_BUSNAK_bm;
    USB_EP_SEND.CNT = USB_EP_SEND.MCNT = 0;
    USB_EP_SEND.DATAPTR = (uint16_t)&USB_SEND_BUFFER;
    USB_EP_SEND.CTRL = USB_TYPE_BULKINT_gc | USB_MULTIPKT_bm | USB_AZLP_bm |
                       USB_TCDSBL_bm | USB_EP_SIZE_gc(USB_BULK_SEND_MAX);
    arm_receive();
    return true;
  }

  void send_serialstate(SerialState_t& state) {
    // Notifications are optional; never wait for the host inside an interrupt.
    if (!USBSTATE.CONFIG || !(USB_EP_INTR.STATUS & USB_BUSNAK_bm)) return;
    auto *p = (Notification_SerialState_t*)&USB_INTR_BUFFER;
    p->bmRequestType = 0xA1;
    p->bRequest = CDC_REQ_SerialState;
    p->wValue = 0;
    p->wIndex = USB_INTERFACE_ID_CDC_CCI;
    p->wLength = 2;
    p->wmSerialState = state;
    USB_EP_INTR.CNT = sizeof(*p);
    release_endpoint(USB_EP_INTR_IN);
  }

  void cb_event_class_sof() {
    service_control();
    tick_bootloader();
    if (is_ready() && is_send_ready() && USBSTATE.SENDCNT) send_buffer();
  }

  void bus_suspend() { suspended = true; }
  void bus_wake() { suspended = false; }

  void start() {
    if (running) return;
    // USB's 48 MHz PLL is controlled by the peripheral, using OSCHF.
    // Match the selected bootloader's internal-regulator / external-VUSB mode.
    SYSCFG_VUSBCTRL = DXCORE_USB_VREG ? SYSCFG_USBVREG_bm : 0;
    _PROTECTED_WRITE(CLKCTRL_OSCHFCTRLA,
      (CLKCTRL_OSCHFCTRLA & ~CLKCTRL_AUTOTUNE_gm) | CLKCTRL_AUTOTUNE_SOF_gc);
    _delay_ms(1);
    ATOMIC_BLOCK(ATOMIC_RESTORESTATE) {
      USB0_CTRLA = 0;
      cb_clear_state();
      running = true;
      bus_reset();
      USB0_INTCTRLA = USB_SOF_bm | USB_SUSPEND_bm | USB_RESUME_bm | USB_RESET_bm;
      USB0_INTCTRLB = USB_SETUP_bm | USB_TRNCOMPL_bm;
    }
  }

  void stop() {
    ATOMIC_BLOCK(ATOMIC_RESTORESTATE) {
      USB0_INTCTRLA = USB0_INTCTRLB = 0;
      bus_detach();
      USB0_CTRLA = 0;
      running = false;
      cb_clear_state();
    }
  }

  // Waiting uses calibrated short delays, so it also terminates with millis disabled.
  static bool wait_send() {
    if (!(SREG & CPU_I_bm) || !is_ready() || !(USBSTATE.bLineState & 1)) return false;
    for (uint16_t ticks = 0; ticks < USB_TX_TIMEOUT_MS * 10U; ++ticks) {
      if (!is_ready() || !(USBSTATE.bLineState & 1)) return false;
      if (is_send_ready()) return true;
      _delay_us(100);
    }
    return false;
  }

  size_t write_byte(uint8_t value) {
    if (!wait_send()) return 0;
    size_t written = 0;
    ATOMIC_BLOCK(ATOMIC_RESTORESTATE) {
      if (is_ready() && is_send_ready() && (USBSTATE.bLineState & 1)) {
        USB_SEND_BUFFER[USBSTATE.SENDCNT++] = value;
        if (USBSTATE.SENDCNT == USB_BULK_SEND_MAX) send_buffer();
        written = 1;
      }
    }
    return written;
  }

  size_t write_available() {
    size_t count = 0;
    ATOMIC_BLOCK(ATOMIC_RESTORESTATE) {
      if (is_ready() && (USBSTATE.bLineState & 1) && is_send_ready())
        count = USB_BULK_SEND_MAX - USBSTATE.SENDCNT;
    }
    return count;
  }

  void write_flush() {
    if (!wait_send()) return;
    ATOMIC_BLOCK(ATOMIC_RESTORESTATE) {
      if (is_ready() && is_send_ready() && USBSTATE.SENDCNT) send_buffer();
    }
    (void)wait_send();
  }

  size_t read_available() {
    size_t count = 0;
    ATOMIC_BLOCK(ATOMIC_RESTORESTATE) {
      if (is_ready() && is_recv_ready()) {
        if (USB_EP_RECV.CNT <= USB_BULK_RECV_MAX && USBSTATE.RECVCNT <= USB_EP_RECV.CNT)
          count = USB_EP_RECV.CNT - USBSTATE.RECVCNT;
        if (!count) arm_receive(); // Also release zero-length packets.
      }
    }
    return count;
  }

  int peek_byte() {
    int value = -1;
    ATOMIC_BLOCK(ATOMIC_RESTORESTATE) {
      if (read_available()) value = USB_RECV_BUFFER[USBSTATE.RECVCNT];
    }
    return value;
  }

  int read_byte() {
    int value = -1;
    ATOMIC_BLOCK(ATOMIC_RESTORESTATE) {
      if (read_available()) {
        value = USB_RECV_BUFFER[USBSTATE.RECVCNT++];
        if (USBSTATE.RECVCNT == USB_EP_RECV.CNT) arm_receive();
      }
    }
    return value;
  }
}
#endif
