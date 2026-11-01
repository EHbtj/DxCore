/* SPDX-License-Identifier: MIT. Host-only USB register model. */
#pragma once
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#define USB0 1
#define _WORDREGISTER(name) volatile uint16_t name
typedef volatile uint8_t register8_t;
typedef volatile uint16_t register16_t;
struct USB_EP_t {
  uint8_t STATUS, CTRL;
  uint16_t CNT, DATAPTR, MCNT;
};
struct USB_EP_PAIR_t { USB_EP_t OUT, IN; };
extern uint8_t SREG, USB0_INTFLAGSB, USB0_INTFLAGSA, USB0_CTRLA, USB0_CTRLB, USB0_ADDR;
#define CPU_I_bm 0x80
#define USB_BUSNAK_bm 2
#define USB_DOSTALL_bm 4
#define USB_TOGGLE_bm 1
#define USB_TRNCOMPL_bm 0x20
#define USB_SETUP_bm 0x10
#define USB_RMWBUSY_bp 2
#define USB_TCDSBL_bm 8
#define USB_ATTACH_bm 1
#define USB_URESUME_bm 2
#define USB_ENABLE_bm 0x80
#define USB_STFRNUM_bm 0x20
#define USB_RESET_bm 1
#define USB_SUSPEND_bm 2
#define USB_RESUME_bm 4
#define USB_SOF_bm 8
#define USB_TYPE_DISABLE_gc 0
#define USB_TYPE_BULKINT_gc 0x80
#define USB_BUFSIZE_DEFAULT_BUF8_gc 0
#define USB_BUFSIZE_DEFAULT_BUF16_gc 1
#define USB_BUFSIZE_DEFAULT_BUF32_gc 2
#define USB_BUFSIZE_DEFAULT_BUF64_gc 3
#define loop_until_bit_is_clear(reg, bit) ((void)0)
