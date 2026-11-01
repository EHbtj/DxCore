/**
 * @file USB/CDC/callback.cpp
 * @author askn (K.Sato) multix.jp
 * @brief Example of using SerialUSB class
 * @version 0.1
 * @date 2024-06-16
 *
 * @copyright Copyright (c) 2024 askn37 at github.com
 *
 */
// MIT License : https://askn37.github.io/LICENSE.html

#pragma once
#include <avr/io.h>
#if defined(DXCORE_USB_CDC) && defined(USB0)

#include "../CDC.h"
#include "define.h"

/* CDC-ACM general specs */
#if defined(DXCORE_USB_HID)
  #define USB_ENDPOINTS_MAX   5
#else
  #define USB_ENDPOINTS_MAX   3
#endif
#define USB_BULK_INTERVAL     0     /* As large as possible */
#define USB_INTR_INTERVAL     5     /* As large as possible */

#define USB_SETUP_PK_SIZE     64    /* This length cannot be changed. */
#define USB_DATA_PK_SIZE      64    /* 64 is the maximum allowed by USB 2.0. */
#define USB_INTR_PK_SIZE      16    /* 16 is enough. */
#define USB_BULK_INTR_MAX     USB_INTR_PK_SIZE
#define USB_BULK_RECV_MAX     USB_DATA_PK_SIZE  /* Cannot be changed. */
#define USB_BULK_SEND_MAX     64                /* It can be expanded up to 1023. */

#define USB_DATA_BUFFER_SIZE  (USB_BULK_SEND_MAX + USB_BULK_RECV_MAX + USB_BULK_INTR_MAX)

#define USB_EP_SIZE_gc(x)  ((x <= 8 ) ? USB_BUFSIZE_DEFAULT_BUF8_gc :\
                            (x <= 16) ? USB_BUFSIZE_DEFAULT_BUF16_gc:\
                            (x <= 32) ? USB_BUFSIZE_DEFAULT_BUF32_gc:\
                            USB_BUFSIZE_DEFAULT_BUF64_gc)

/* EPFIFO number that identifies the EP */
#define USB_EP_SETUP_OUT  (0x00)
#define USB_EP_SETUP_IN   (0x08)
#define USB_EP_INTR_OUT   (0x10)  /* notused */
#define USB_EP_INTR_IN    (0x18)
#define USB_EP_BULK_OUT   (0x20)
#define USB_EP_BULK_IN    (0x28)
#define USB_EP_KEYBOARD_IN (0x38)
#define USB_EP_MOUSE_IN    (0x48)

/* Shortcut Macros */

#define USB_EP(EPFIFO) (*(USB_EP_t *)(((uint16_t)&get_endpointer_ptr()->EP) + (EPFIFO)))
#define USB_EP_STATUS_CLR(EPFIFO) _SFR_MEM8(&USB0_STATUS0_OUTCLR + ((EPFIFO) >> 2))
#define USB_EP_STATUS_SET(EPFIFO) _SFR_MEM8(&USB0_STATUS0_OUTSET + ((EPFIFO) >> 2))

#define USB_EP_REQ USB_EP(USB_EP_SETUP_OUT)
#define USB_EP_RES USB_EP(USB_EP_SETUP_IN)

#define USB_SETUP_DATA  (*(Setup_Packet*)&get_workmem_ptr()->setup)
#define USB_HEADER_DATA (*(Descriptor_Header*)&get_workmem_ptr()->header)
#define USB_DATA_BUFFER (*(uint8_t*)&get_workmem_ptr()->header)

#define USB_EP_NOOP USB_EP(USB_EP_INTR_OUT)
#define USB_EP_INTR USB_EP(USB_EP_INTR_IN)
#define USB_EP_RECV USB_EP(USB_EP_BULK_OUT)
#define USB_EP_SEND USB_EP(USB_EP_BULK_IN)

#define USB_INTR_BUFFER (get_workmem_ptr()->intr)
#define USB_RECV_BUFFER (get_workmem_ptr()->recv)
#define USB_SEND_BUFFER (get_workmem_ptr()->send)

/* Area for storing the status of the USB interface. */
#define USBSTATE (*(Interface_State*)get_state())  /* *(USB_Interface_State*) */

namespace USB_NAMESPACE {

  typedef struct {
    register8_t   FIFO[USB_ENDPOINTS_MAX * 2];  /* FIFO Index Table */
    USB_EP_PAIR_t EP[USB_ENDPOINTS_MAX];        /* USB Device Controller EP */
    _WORDREGISTER(FRAMENUM);                    /* FRAMENUM count */
  } PACKED USB_EP_TABLE_t;

  struct USB_WM_TABLE_t {
    union {
      Setup_Packet setup;
      uint8_t setup_data[USB_SETUP_PK_SIZE];
    };
    union {
      Descriptor_Header header;
      uint8_t data[USB_DATA_BUFFER_SIZE];
    };
    // Dedicated, even-aligned DMA buffers: EP0 traffic must not overwrite CDC data.
    uint8_t intr[USB_BULK_INTR_MAX];
    uint8_t recv[USB_BULK_RECV_MAX];
    uint8_t send[USB_BULK_SEND_MAX];
    #if defined(DXCORE_USB_HID)
    uint8_t keyboard[8];
    uint8_t mouse[4];
    #endif
  };

  static_assert(offsetof(USB_EP_TABLE_t, EP) % 2 == 0, "USB endpoint table must be even-aligned");
  static_assert(offsetof(USB_WM_TABLE_t, header) % 2 == 0, "EP0 DMA buffer must be even-aligned");
  static_assert(offsetof(USB_WM_TABLE_t, intr) % 2 == 0, "Notification DMA buffer must be even-aligned");
  static_assert(offsetof(USB_WM_TABLE_t, recv) % 2 == 0, "Receive DMA buffer must be even-aligned");
  static_assert(offsetof(USB_WM_TABLE_t, send) % 2 == 0, "Transmit DMA buffer must be even-aligned");
  #if defined(DXCORE_USB_HID)
  static_assert(offsetof(USB_WM_TABLE_t, keyboard) % 2 == 0, "Keyboard DMA buffer must be even-aligned");
  static_assert(offsetof(USB_WM_TABLE_t, mouse) % 2 == 0, "Mouse DMA buffer must be even-aligned");
  #endif
  static_assert(sizeof(Setup_Packet) == 8 && sizeof(LineEncoding_t) == 7, "USB wire structures must be packed");

  /*** Callback functions for them. ***/

  USB_EP_TABLE_t *get_endpointer_ptr(void);
  USB_WM_TABLE_t *get_workmem_ptr(void);

  /* Static descriptor getter */
  size_t cb_get_descriptor_data(uint8_t *_buffer, uint16_t _index);

  /* Endpoint Setup IN/OUT */
  void ep_setup_clear(void);
  void ep_setup_in_listen(void);
  void ep_setup_out_listen(void);
  void ep_setup_stalled(void);

  uint8_t cb_get_configuration(void);
  bool cb_set_configuration(uint8_t _config);
  void cb_clear_state(void);


} /* USB_CDC */

#endif
// end of header
