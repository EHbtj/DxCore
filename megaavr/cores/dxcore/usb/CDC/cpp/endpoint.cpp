/**
 * @file USB/CDC/cpp/endpoint.cpp
 * @author askn (K.Sato) multix.jp
 * @brief Example of using SerialUSB class
 * @version 0.1
 * @date 2024-06-16
 *
 * @copyright Copyright (c) 2024 askn37 at github.com
 *
 */
// MIT License : https://askn37.github.io/LICENSE.html

#include <avr/io.h>
#if defined(DXCORE_USB_CDC) && defined(USB0)

#include "../../CDC.h"
#include "../constants.h"
#include <string.h>

namespace USB_NAMESPACE {
  /*
   * Implementing a USB endpoint
   */

  /*** Here we must allocate memory and define callback functions to operate on it. ***/
  alignas(2) NOINIT USB_EP_TABLE_t EP_TABLE;
  alignas(2) NOINIT USB_WM_TABLE_t EP_BUFFER;

  WEAK USB_EP_TABLE_t *get_endpointer_ptr(void) {
    return &EP_TABLE;
  }
  WEAK USB_WM_TABLE_t *get_workmem_ptr(void) {
    return &EP_BUFFER;
  }

  /* Returns the USB frame number. */
  uint16_t get_frame(void) {
    return EP_TABLE.FRAMENUM & 0x7FF;
  }

  WEAK void ep_setup_clear(void) {
    memset(&EP_TABLE, 0, sizeof(EP_TABLE));
    USB0_ADDR = 0;
    USB0_FIFOWP = 0;
    USB0_EPPTR = (uint16_t)&EP_TABLE.EP;

    /* EP0 uses completion interrupts; CDC data endpoints are serviced at SOF. */
    USB_EP_REQ.CTRL = USB_TYPE_CONTROL_gc | USB_EP_SIZE_gc(USB_SETUP_PK_SIZE);
    USB_EP_REQ.DATAPTR = (uint16_t)&USB_SETUP_DATA;
    USB_EP_REQ.MCNT = USB_SETUP_PK_SIZE;
    USB_EP_REQ.STATUS = 0;

    USB_EP_RES.CTRL = USB_TYPE_CONTROL_gc | USB_MULTIPKT_bm | USB_AZLP_bm | USB_EP_SIZE_gc(USB_SETUP_PK_SIZE);
    USB_EP_RES.DATAPTR = (uint16_t)&USB_DATA_BUFFER;
    USB_EP_RES.MCNT = 0;
    USB_EP_RES.STATUS = USB_BUSNAK_bm;
  }

  WEAK void ep_setup_in_listen(void) {
    USB_EP_RES.MCNT = 0;
    loop_until_bit_is_clear(USB0_INTFLAGSB, USB_RMWBUSY_bp);
    USB_EP_STATUS_CLR(USB_EP_SETUP_IN) = ~USB_TOGGLE_bm;
  }

  WEAK void ep_setup_out_listen(void) {
    USB_EP_REQ.CNT = 0;
    loop_until_bit_is_clear(USB0_INTFLAGSB, USB_RMWBUSY_bp);
    USB_EP_STATUS_CLR(USB_EP_SETUP_OUT) = ~USB_TOGGLE_bm;
  }

  WEAK void ep_setup_stalled(void) {
    USB_EP_REQ.CTRL |= USB_DOSTALL_bm;
    USB_EP_RES.CTRL |= USB_DOSTALL_bm;
    loop_until_bit_is_clear(USB0_INTFLAGSB, USB_RMWBUSY_bp);
    USB_EP_STATUS_CLR(USB_EP_SETUP_IN) = USB_EPSETUP_bm;
    loop_until_bit_is_clear(USB0_INTFLAGSB, USB_RMWBUSY_bp);
    USB_EP_STATUS_CLR(USB_EP_SETUP_OUT) = USB_EPSETUP_bm;
  }

} /* USB_CDC */

#endif
// end of code
