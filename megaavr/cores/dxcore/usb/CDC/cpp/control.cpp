/**
 * USB control transfers, adapted for DxCore.
 * Copyright (c) 2024 askn37 at github.com. MIT License (see ../LICENSE).
 * Based on MacroMicroAPI_lib 1804a5239181f33b2f07ab7bac4c83bfc74d9c8b.
 * DxCore: asynchronous EP0 state machine; no host-dependent waits in an ISR.
 */
#include <avr/io.h>
#if defined(DXCORE_USB_CDC) && defined(USB0)
#include <string.h>
#include "../../CDC.h"
#include "../constants.h"

namespace USB_NAMESPACE {
  enum class ControlPhase : uint8_t { Idle, LineCoding, AddressStatus, BootStatus };
  static ControlPhase phase;
  static Setup_Packet request;
  static uint8_t pending_address;
  static uint8_t boot_ticks;

  void bus_attach() { USB0_CTRLB |= USB_ATTACH_bm; }
  void bus_detach() { USB0_CTRLB &= ~USB_ATTACH_bm; }
  void bus_resume() { USB0_CTRLB |= USB_URESUME_bm; }

  void bus_reset() {
    USB0_CTRLA = 0;
    phase = ControlPhase::Idle;
    boot_ticks = pending_address = 0;
    cb_clear_state();
    ep_setup_clear();
    USB0_INTFLAGSA = 0xff;
    USB0_INTFLAGSB = USB_SETUP_bm | USB_TRNCOMPL_bm;
    // EP0 completion interrupts plus SOF service; the transaction FIFO is unused.
    USB0_CTRLA = USB_ENABLE_bm | USB_STFRNUM_bm | (USB_ENDPOINTS_MAX - 1);
    bus_attach();
  }

  static void control_in(uint16_t count) {
    USB_EP_RES.CNT = count;
    ep_setup_in_listen();
    // OUT accepts the status stage (or the next SETUP packet).
    ep_setup_out_listen();
  }

  static bool valid_interface(uint16_t index) {
    return USBSTATE.CONFIG && index <= USB_INTERFACE_ID_CDC_DCI;
  }

  static bool valid_endpoint(uint16_t address) {
    return address == 0 || address == 0x80 ||
           (USBSTATE.CONFIG && (address == 0x81 || address == 0x02 || address == 0x82));
  }

  static uint8_t endpoint_id(uint8_t address) {
    return ((address & 0x0f) << 4) | ((address & 0x80) ? 8 : 0);
  }

  void handle_setup() {
    // Preserve SETUP while the asynchronous DATA/STATUS stages are in flight.
    memcpy(&request, &USB_SETUP_DATA, sizeof(request));
    USB_EP_REQ.CTRL &= ~USB_DOSTALL_bm;
    USB_EP_RES.CTRL &= ~USB_DOSTALL_bm;
    phase = ControlPhase::Idle;
    bool accepted = true;
    uint16_t size = 0;
    uint8_t *data = &USB_DATA_BUFFER;
    const uint8_t recipient = request.bmRequestType & USB_REQTYPE_RECIPIENT_gm;

    if ((request.bmRequestType & USB_REQTYPE_TYPE_gm) == USB_REQTYPE_STANDARD_gc) {
      switch (request.bRequest) {
        case USB_REQ_GetDescriptor:
          accepted = request.bmRequestType == 0x80;
          if (accepted) {
            size = cb_get_descriptor_data(data, request.wValue);
            accepted = size != 0;
          }
          break;
        case USB_REQ_SetAddress:
          accepted = request.bmRequestType == 0 && !request.wIndex &&
                     !request.wLength && request.wValue < 128 && !USBSTATE.CONFIG;
          if (accepted) {
            pending_address = request.wValue;
            phase = ControlPhase::AddressStatus;
          }
          break;
        case USB_REQ_GetConfiguration:
          accepted = request.bmRequestType == 0x80 && !request.wValue &&
                     !request.wIndex && request.wLength == 1;
          data[0] = cb_get_configuration();
          size = 1;
          break;
        case USB_REQ_SetConfiguration:
          accepted = request.bmRequestType == 0 && !request.wIndex &&
                     !request.wLength && request.wValue <= 1;
          if (accepted) accepted = cb_set_configuration(request.wValue);
          break;
        case USB_REQ_GetStatus:
          accepted = (request.bmRequestType & 0x80) && !request.wValue && request.wLength == 2;
          data[0] = data[1] = 0;
          if (recipient == USB_RECIPIENT_DEVICE_gc) accepted &= request.wIndex == 0;
          else if (recipient == USB_RECIPIENT_INTERFACE_gc) accepted &= valid_interface(request.wIndex);
          else if (recipient == USB_RECIPIENT_ENDPOINT_gc) {
            accepted &= valid_endpoint(request.wIndex);
            if (accepted) data[0] = !!(USB_EP(endpoint_id(request.wIndex)).CTRL & USB_DOSTALL_bm);
          } else accepted = false;
          size = 2;
          break;
        case USB_REQ_GetInterface:
          accepted = request.bmRequestType == 0x81 && !request.wValue &&
                     request.wLength == 1 && valid_interface(request.wIndex);
          data[0] = 0;
          size = 1;
          break;
        case USB_REQ_SetInterface:
          accepted = request.bmRequestType == 0x01 && !request.wValue &&
                     !request.wLength && valid_interface(request.wIndex);
          break;
        case USB_REQ_ClearFeature:
        case USB_REQ_SetFeature:
          accepted = request.bmRequestType == 0x02 && !request.wValue &&
                     !request.wLength && valid_endpoint(request.wIndex) && (request.wIndex & 0x0f);
          if (accepted) {
            uint8_t id = endpoint_id(request.wIndex);
            if (request.bRequest == USB_REQ_SetFeature) {
              USB_EP(id).CTRL |= USB_DOSTALL_bm;
            } else {
              loop_until_bit_is_clear(USB0_INTFLAGSB, USB_RMWBUSY_bp);
              USB_EP_STATUS_SET(id) = USB_BUSNAK_bm;
              loop_until_bit_is_clear(USB0_INTFLAGSB, USB_RMWBUSY_bp);
              USB_EP_STATUS_CLR(id) = (uint8_t)~USB_BUSNAK_bm; // Reset to DATA0.
              USB_EP(id).CTRL &= ~USB_DOSTALL_bm;
              USB_EP(id).CNT = 0;
              if (id == USB_EP_BULK_IN) USBSTATE.SENDCNT = 0;
              if (id == USB_EP_BULK_OUT) {
                USBSTATE.RECVCNT = 0;
                loop_until_bit_is_clear(USB0_INTFLAGSB, USB_RMWBUSY_bp);
                USB_EP_STATUS_CLR(id) = ~USB_TOGGLE_bm;
              }
            }
          }
          break;
        default:
          accepted = false;
      }
    } else if ((request.bmRequestType & USB_REQTYPE_TYPE_gm) == USB_REQTYPE_CLASS_gc &&
               request.wIndex == USB_INTERFACE_ID_CDC_CCI && USBSTATE.CONFIG) {
      switch (request.wRequestType) {
        case CDC_REQTYPE_SetLineEncoding:
          accepted = !request.wValue && request.wLength == sizeof(LineEncoding_t);
          if (accepted) {
            phase = ControlPhase::LineCoding;
            ep_setup_out_listen();
            USB0_INTFLAGSB = USB_SETUP_bm;
            return; // ACK with a status IN only after the DATA OUT arrives.
          }
          break;
        case CDC_REQTYPE_GetLineEncoding:
          accepted = !request.wValue && request.wLength == sizeof(LineEncoding_t);
          size = sizeof(LineEncoding_t);
          memcpy(data, &USBSTATE.LineEncoding, size);
          break;
        case CDC_REQTYPE_SetLineState: {
          accepted = !request.wLength;
          uint8_t previous = USBSTATE.bLineState;
          USBSTATE.bLineState = request.wValue & 3;
          #if defined(USING_EUBOOT)
            if (accepted && (previous & 1) && !(request.wValue & 1) &&
                USBSTATE.LineEncoding.dwDTERate == 1200)
              phase = ControlPhase::BootStatus;
          #else
            (void)previous;
          #endif
          break;
        }
        case CDC_REQTYPE_SendBreak:
          accepted = !request.wLength;
          USBSTATE.SerialState.bBreak = request.wValue != 0;
          send_serialstate(USBSTATE.SerialState);
          break;
        default:
          accepted = false;
      }
    } else accepted = false;

    if (accepted) {
      if (size > request.wLength) size = request.wLength;
      control_in(size);
    } else {
      phase = ControlPhase::Idle;
      ep_setup_stalled();
    }
    USB0_INTFLAGSB = USB_SETUP_bm;
  }

  void service_control() {
    if (USB0_INTFLAGSB & USB_SETUP_bm) {
      handle_setup();
      return; // RMW endpoint rearming may still be in flight.
    }
    constexpr uint8_t completed = USB_BUSNAK_bm | USB_TRNCOMPL_bm;
    if (phase == ControlPhase::LineCoding && (USB_EP_REQ.STATUS & completed) == completed) {
      if (USB_EP_REQ.CNT == sizeof(LineEncoding_t)) {
        // AVR DU uses EP0 IN.DATAPTR for both directions of the DATA stage.
        memcpy(&USBSTATE.LineEncoding, &USB_DATA_BUFFER, sizeof(LineEncoding_t));
        phase = ControlPhase::Idle;
        control_in(0);
      } else {
        phase = ControlPhase::Idle;
        ep_setup_stalled();
      }
    } else if ((USB_EP_RES.STATUS & completed) == completed) {
      if (phase == ControlPhase::AddressStatus) {
        USB0_ADDR = pending_address;
        phase = ControlPhase::Idle;
      } else if (phase == ControlPhase::BootStatus) {
        // The host has ACKed the status transfer. Allow it to release the COM handle.
        boot_ticks = 50;
        phase = ControlPhase::Idle;
      }
    }
  }

  void tick_bootloader() {
    #if defined(USING_EUBOOT)
      if (boot_ticks && --boot_ticks == 0) {
        bus_detach();
        USB0_CTRLA = 0;
        cli();
        // Match euboot's SRAM-top marker. No function call after this store.
        *(volatile uint16_t*)(RAMEND - 1) = 0;
        _PROTECTED_WRITE(RSTCTRL_SWRR, 1);
        __builtin_unreachable();
      }
    #endif
  }

  void handling_bus_events() {
    uint8_t flags = USB0_INTFLAGSA;
    USB0_INTFLAGSA = flags; // W1C: acknowledge exactly the sampled flags.
    if (flags & USB_RESET_bm) { bus_reset(); return; }
    if (flags & USB_SUSPEND_bm) bus_suspend();
    if (flags & USB_RESUME_bm) bus_wake();
    if (flags & USB_SOF_bm) cb_event_class_sof();
  }

  void handling_transaction_events() {
    service_control();
    USB0_INTFLAGSB = USB_TRNCOMPL_bm;
  }
}
#endif
