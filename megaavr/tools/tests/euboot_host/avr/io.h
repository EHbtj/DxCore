/* SPDX-License-Identifier: MIT
 * Host-only register definitions for compiling the actual firmware handlers.
 */
#pragma once
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#ifdef _MSC_VER
  #define __attribute__(value)
#endif
struct USB_EP_t {
  uint8_t STATUS, CTRL;
  uint16_t CNT, DATAPTR, MCNT;
};
struct USB_EP_PAIR_t {
  USB_EP_t OUT, IN;
};
extern uint8_t host_gpr0, host_gpr2, host_flmap, host_wdt;
#define GPR_GPR0 host_gpr0
#define GPR_GPR2 host_gpr2
#define NVMCTRL_CTRLB host_flmap
#define NVMCTRL_FLMAP_SECTION0_gc 0
#define NVMCTRL_FLMAP_SECTION1_gc 1
#define NVMCTRL_CMD_NONE_gc 0
#define NVMCTRL_CMD_EEERWR_gc 1
#define NVMCTRL_CMD_FLPER_gc 2
#define NVMCTRL_CMD_FLWR_gc 3
#define WDT_STATUS host_wdt
#define WDT_CTRLA host_wdt
#define WDT_SYNCBUSY_bp 0
#define WDT_PERIOD_128CLK_gc 1
#define PROGMEM_SIZE HOST_FLASH_SIZE
#define PROGMEM_END (PROGMEM_SIZE - 1)
#define PROGMEM_PAGE_SIZE 512
#define EEPROM_START 0x1400
#define EEPROM_SIZE 256
#define EEPROM_END (EEPROM_START + EEPROM_SIZE - 1)
#define BOOTROW_START 0x1100
#define BOOTROW_SIZE 256
#define BOOTROW_END (BOOTROW_START + BOOTROW_SIZE - 1)
#define USER_SIGNATURES_START 0x1200
#define USER_SIGNATURES_SIZE 512
#define USER_SIGNATURES_END (USER_SIGNATURES_START + USER_SIGNATURES_SIZE - 1)
#define _PROTECTED_WRITE(reg, value) ((reg) = (value))
