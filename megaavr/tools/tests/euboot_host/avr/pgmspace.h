/* SPDX-License-Identifier: MIT */
#pragma once
#include "io.h"
#define PROGMEM
extern uint8_t host_flash[65536];
inline void *memcpy_P(void *dest, const void *source, size_t count) {
  uintptr_t address = (uintptr_t)source;
  return memcpy(dest, address < 65536 ? host_flash + address : source, count);
}
