/* SPDX-License-Identifier: MIT
 * Minimal avr-libc compatibility layer for the pinned non-debug euboot build.
 * Pin numbers encode the hardware port (A=0..F=5) and bit, not Arduino pins.
 */
#pragma once
#include <avr/io.h>
#include <avr/interrupt.h>
#include <avr/wdt.h>
#include <util/atomic.h>
#include <util/delay.h>
#include <stdint.h>
#include <string.h>
#include "../variant.h"

#ifdef DEBUG
  #error "The standalone euboot build supports NDEBUG only."
#endif
#define OUTPUT 1
#define TOGGLE 2
#define bit_set(reg, bit) ((reg) |= (uint8_t)(1U << (bit)))
#define __QUOTE_INNER(value) #value
#define __QUOTE(value) __QUOTE_INNER(value)
#define EUBOOT_PORT(pin) (*(PORT_t *)(0x0400 + (((pin) >> 3) * 0x20)))
#define pinControlRegister(pin) (*((volatile uint8_t *)&EUBOOT_PORT(pin).PIN0CTRL + ((pin) & 7)))
#define digitalReadMacro(pin) (_SFR_MEM8((((pin) >> 3) * 4) + 2) & (1U << ((pin) & 7)))
#define digitalWriteMacro(pin, value) (EUBOOT_PORT(pin).OUTTGL = (1U << ((pin) & 7)))
#define pinMode(pin, mode) (EUBOOT_PORT(pin).DIRSET = (1U << ((pin) & 7)))
#define delay_micros(value) _delay_us(value)
#define delay_millis(value) _delay_ms(value)
