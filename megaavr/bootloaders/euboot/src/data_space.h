/* SPDX-License-Identifier: MIT */
#ifndef EUBOOT_DATA_SPACE_H
#define EUBOOT_DATA_SPACE_H

namespace NVM::V4 {
  /* Keep data-space access separate from program-space memcpy_P. */
  inline uint8_t *data_pointer(uint16_t address) {
    return (uint8_t *)address;
  }
};

#endif
