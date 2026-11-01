/* SPDX-License-Identifier: MIT
 * Compile the production JTAG/NVM handlers with register and address-space stubs.
 * The stubs do not simulate USB timing or flash-controller electrical behavior.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <initializer_list>
#include <avr/io.h>
#pragma pack(push, 1)
#include "prototype.h"
#pragma pack(pop)

uint8_t host_gpr0, host_gpr2, host_flmap, host_wdt;
uint8_t host_flash[65536], host_data[65536];
static unsigned nvm_commands, checks;
static bool reply_pending;
static uint8_t queued_reply[64];
static void acknowledge_reply();
static USB_EP_t host_dpo;
#undef EP_DPO
#define EP_DPO host_dpo

namespace {
  EP_TABLE_t EP_TABLE;
  EP_DATA_t EP_MEM;
  Device_Desc_t Device_Descriptor;
  JTAG_Packet_t packet;
  size_t _packet_length;
  uint8_t _packet_fragment, _packet_chunks, _packet_endfrag;
  uint8_t _jtag_arch, _jtag_conn, _set_config;
  uint8_t _led_bits, _led_next, _led_mask;
  uint16_t _bootsize;
  uint32_t _before_page;
};

extern "C" void nvm_cmd(uint8_t) {
  ++nvm_commands;
}
namespace USB {
  void ep_dpi_pending() { acknowledge_reply(); }
  void complete_dap_out() {
    memcpy(queued_reply, EP_MEM.dap_data, sizeof(queued_reply));
    reply_pending = true;
  }
};
namespace SYS {
  uint16_t get_vdd() { return 3300; }
};

/* Substitute only the numeric AVR data-space pointer, keeping validation real. */
#define EUBOOT_DATA_SPACE_H
namespace NVM::V4 {
  inline uint8_t *data_pointer(uint16_t address) {
    if (address >= 0x8000) {
      uint32_t offset = (address & 0x7FFF) + (host_flmap ? 0x8000UL : 0);
      return host_flash + offset;
    }
    return host_data + address;
  }
};
#include "nvmv4.cpp"
#include "jtag.cpp"

#define CHECK(condition) do { \
    ++checks; \
    if (!(condition)) { \
      fprintf(stderr, "line %d: %s\n", __LINE__, #condition); \
      exit(1); \
    } \
  } while (0)

static void acknowledge_reply() {
  if (reply_pending) {
    // Model the host consuming a previously queued HID IN report only now.
    CHECK(memcmp(queued_reply, EP_MEM.dap_data, sizeof(queued_reply)) == 0);
    reply_pending = false;
  }
}

static void reset() {
  memset(&packet, 0xA5, sizeof(packet));
  memset(&EP_MEM, 0, sizeof(EP_MEM));
  memset(host_data, 0xFF, sizeof(host_data));
  memset(host_flash, 0xFF, sizeof(host_flash));
  _packet_chunks = _packet_endfrag = _packet_fragment = 0;
  _packet_length = 0;
  _bootsize = 0x0C00;
  _jtag_arch = 5;
  host_dpo.CNT = 64;
  host_flmap = 0;
  nvm_commands = 0;
  reply_pending = false;
}

static bool fragment(uint8_t id, uint8_t total, uint8_t size, const uint8_t *data) {
  memset(EP_MEM.dap_recv, 0, 64);
  EP_MEM.dap_recv[0] = 0x80;
  EP_MEM.dap_recv[1] = (id << 4) | total;
  EP_MEM.dap_recv[3] = size;
  if (size <= 60) {
    memcpy(EP_MEM.dap_recv + 4, data, size);
  }
  return JTAG::dap_command_check();
}

static void execute(const JTAG_Packet_t &request, size_t size) {
  uint8_t total = (uint8_t)((size + 59) / 60);
  for (uint8_t i = 0; i < total; ++i) {
    size_t left = size - i * 60;
    bool complete = fragment(i + 1, total, (uint8_t)(left > 60 ? 60 : left), request.rawData + i * 60);
    CHECK(EP_MEM.dap_data[1] == 1);
    CHECK(complete == (i == total - 1));
  }
  JTAG::jtag_scope_branch();
}

static void memory(uint8_t cmd, uint8_t type, uint32_t address, uint32_t length, size_t received = 0) {
  if (getenv("EUBOOT_TRACE")) {
    fprintf(stderr, "memory %02X:%02X addr=%08X len=%u flash=%p data=%p\n",
            cmd, type, (unsigned)address, (unsigned)length, (void *)host_flash, (void *)host_data);
  }
  JTAG_Packet_t request = {};
  request.out.token = 0x0E;
  request.out.scope = 0x12;
  request.out.cmd = cmd;
  request.out.bMType = type;
  request.out.dwAddr = address;
  request.out.dwLength = length;
  memset(request.out.memData, 0x5A, sizeof(request.out.memData));
  execute(request, received ? received : (cmd == 0x23 ? 17 + length : 16));
}

static void rejected_write(uint8_t type, uint32_t address, uint32_t length, size_t received = 0) {
  reset();
  memory(0x23, type, address, length, received);
  CHECK(packet.in.res == 0xA0);
  CHECK(nvm_commands == 0);
  for (size_t i = 0; i < sizeof(host_flash); ++i) {
    CHECK(host_flash[i] == 0xFF);
    CHECK(host_data[i] == 0xFF);
  }
}

static void test_fragments() {
  uint8_t data[60] = {};
  /* Exercise every fragment/total pair, including 0 and numbers above 9. */
  for (uint8_t id = 0; id < 16; ++id) {
    for (uint8_t total = 0; total < 16; ++total) {
      reset();
      bool complete = fragment(id, total, 60, data);
      CHECK(complete == (id == 1 && total == 1));
      CHECK(EP_MEM.dap_data[1] == (id == 1 && total >= 1 && total <= 9 ? 1 : 0));
    }
  }
  for (unsigned size = 61; size < 256; ++size) {
    reset();
    CHECK(!fragment(1, 1, (uint8_t)size, data));
    CHECK(EP_MEM.dap_data[1] == 0);
  }
  reset();
  CHECK(!fragment(1, 1, 0, data));
  CHECK(EP_MEM.dap_data[1] == 0);
  CHECK(!fragment(1, 3, 59, data));
  CHECK(EP_MEM.dap_data[1] == 0);
  CHECK(!fragment(1, 3, 60, data));
  CHECK(!fragment(3, 3, 60, data)); // Missing fragment 2.
  CHECK(EP_MEM.dap_data[1] == 0);
  CHECK(!fragment(1, 4, 60, data));
  CHECK(!fragment(2, 4, 60, data));
  CHECK(!fragment(2, 4, 60, data)); // Duplicate fragment.
  CHECK(EP_MEM.dap_data[1] == 0);
  CHECK(!fragment(4, 4, 60, data));
  CHECK(!fragment(1, 3, 60, data));
  CHECK(!fragment(2, 2, 60, data)); // Changed total.
  CHECK(EP_MEM.dap_data[1] == 0);
  CHECK(fragment(1, 1, 6, data));  // Fresh command recovers after failure.
  for (uint16_t count = 0; count < 64; ++count) {
    reset();
    host_dpo.CNT = count;
    CHECK(!fragment(1, 1, 6, data));
    CHECK(EP_MEM.dap_data[1] == 0);
  }
  reset();
  fragment(1, 2, 60, data);
  EP_MEM.dap_recv[0] = 0x80;
  EP_MEM.dap_recv[1] = 0x22;
  EP_MEM.dap_recv[2] = 1; // High byte of HID payload length must be zero.
  CHECK(!JTAG::dap_command_check());
  CHECK(EP_MEM.dap_data[1] == 0);
}

static void test_reads_and_responses() {
  reset();
  for (unsigned i = 0; i < 512; ++i) {
    host_flash[0x0C00 + i] = (uint8_t)i;
  }
  memory(0x21, 0xB0, 0x0C00, 512);
  CHECK(packet.in.res == 0x184);
  CHECK(_packet_endfrag == 9);
  uint8_t reply[540] = {};
  size_t count = 0;
  for (unsigned i = 0; i < 9; ++i) {
    EP_MEM.dap_recv[0] = 0x81;
    CHECK(!JTAG::dap_command_check());
    CHECK(EP_MEM.dap_data[1] == ((i + 1) << 4 | 9));
    uint8_t size = EP_MEM.dap_data[3];
    CHECK(size == (i == 8 ? 39 : 60));
    memcpy(reply + count, EP_MEM.dap_data + 4, size);
    count += size;
    for (unsigned j = size; j < 60; ++j) {
      CHECK(EP_MEM.dap_data[4 + j] == 0);
    }
  }
  CHECK(count == 519);
  CHECK(reply[0] == 0x0E && reply[4] == 0x84);
  CHECK(memcmp(reply + 6, host_flash + 0x0C00, 512) == 0);
  for (unsigned i = 0; i < 20; ++i) {
    EP_MEM.dap_recv[0] = 0x81;
    CHECK(!JTAG::dap_command_check());
    CHECK(EP_MEM.dap_data[1] == 0 && EP_MEM.dap_data[3] == 0);
  }
  const uint32_t lengths[] = {0, 513, 65536, 65537, 0xFFFFFFFFUL};
  for (uint32_t length : lengths) {
    reset();
    memory(0x21, 0xB0, 0x0C00, length);
    CHECK(packet.in.res == 0xA0);
  }
  reset();
  memory(0x21, 0xB0, PROGMEM_END, 1);
  CHECK(packet.in.res == 0x184);
  memory(0x21, 0xB0, PROGMEM_END, 2);
  CHECK(packet.in.res == 0xA0);
  memory(0x21, 0xB0, 0x10000, 1);
  CHECK(packet.in.res == 0xA0);
  memory(0x21, 0xD3, 0, 32);
  CHECK(packet.in.res == 0x184);
  memory(0x21, 0xD3, 0, 33);
  CHECK(packet.in.res == 0xA0);
  memory(0x21, 0xD3, 1, 1);
  CHECK(packet.in.res == 0xA0);
  for (size_t received = 6; received < 16; ++received) {
    memory(0x21, 0xB0, 0x0C00, 1, received);
    CHECK(packet.in.res == 0xA0);
  }
}

static void test_writes() {
  for (uint8_t type : {0xB0, 0xC0}) {
    rejected_write(type, 0, 512);
    rejected_write(type, 0x0BFF, 1);
    rejected_write(type, 0x0DFF, 2); // Crosses an erase-page boundary.
    rejected_write(type, PROGMEM_END, 2);
    rejected_write(type, 0x10000UL + 0x0C00, 1);
    rejected_write(type, 0x0C00, 0);
    rejected_write(type, 0x0C00, 513);
    rejected_write(type, 0x0C00, 65537, 18);
    rejected_write(type, 0x0C00, 512, 528); // One missing payload byte.
    rejected_write(type, 0x0C00, 1, 16);    // Missing write header byte.
    reset();
    memory(0x23, type, 0x0C00, 512);
    CHECK(packet.in.res == 0x80 && nvm_commands == 3);
    for (unsigned i = 0; i < 512; ++i) {
      CHECK(host_flash[0x0C00 + i] == 0x5A);
    }
    reset();
    memory(0x23, type, PROGMEM_END, 1);
    CHECK(packet.in.res == 0x80 && host_flash[PROGMEM_END] == 0x5A);
  }
  rejected_write(0x20, 0x0030, 1); // Unknown memory type cannot write registers.
  for (uint8_t type : {0x22, 0xC4}) {
    rejected_write(type, EEPROM_START - 1, 1);
    rejected_write(type, EEPROM_END, 2);
    reset();
    memory(0x23, type, EEPROM_END, 1);
    CHECK(packet.in.res == 0x80 && host_data[EEPROM_END] == 0x5A);
  }
  rejected_write(0xC5, BOOTROW_START - 1, 1);
  rejected_write(0xC5, BOOTROW_END, 2);
  rejected_write(0xC5, USER_SIGNATURES_END, 2);
  for (uint16_t address : {BOOTROW_START, USER_SIGNATURES_START}) {
    reset();
    uint16_t length = address == BOOTROW_START ? BOOTROW_SIZE : USER_SIGNATURES_SIZE;
    memory(0x23, 0xC5, address, length);
    CHECK(packet.in.res == 0x80 && nvm_commands == 3);
    CHECK(host_data[address] == 0x5A && host_data[address + length - 1] == 0x5A);
  }
}

static void test_parameters() {
  JTAG_Packet_t request = {};
  request.out.token = 0x0E;
  request.out.scope = 1;
  request.out.cmd = 2;
  request.out.length = 1;
  reset();
  execute(request, 10);
  CHECK(packet.in.res == 0x184 && packet.in.data[0] == HW_VER);
  request.out.index = 5;
  execute(request, 10);
  CHECK(packet.in.res == 0xA0);
  request.out.index = 255;
  request.out.length = 255;
  execute(request, 10);
  CHECK(packet.in.res == 0xA0);
  request.out.scope = 0x20;
  execute(request, 10);
  CHECK(packet.in.res == 0x184);
  for (unsigned i = 0; i < 255; ++i) {
    CHECK(packet.in.data[i] == 0);
  }
  request.out.scope = 0x12;
  request.out.cmd = 1;
  request.out.section = 0;
  request.out.index = 0;
  request.out.length = 1;
  request.out.setData[0] = 3;
  execute(request, 10); // Truncated SET must not change the architecture.
  CHECK(packet.in.res == 0xA0 && _jtag_arch == 5);
  execute(request, 11);
  CHECK(packet.in.res == 0x80 && _jtag_arch == 3);
  request.out.section = 2;
  request.out.length = (uint8_t)sizeof(Device_Descriptor) + 1;
  execute(request, 10 + request.out.length);
  CHECK(packet.in.res == 0xA0);
  request.out.cmd = 0x10;
  request.out.scope = 0x7F;
  execute(request, 6);
  CHECK(packet.in.res == 0xA0);
}

static void test_control_during_dap_reply() {
  reset();
  uint8_t command[60] = {};
  CHECK(fragment(1, 1, 6, command));
  uint8_t reply[64];
  memcpy(reply, EP_MEM.dap_data, sizeof(reply));
  // A string request may complete while a HID IN report is still queued.
  // Match the UTF-16 "euboot" prefix returned by USB::get_descriptor().
  const uint8_t string[] = {14, 3, 'e', 0, 'u', 0, 'b', 0, 'o', 0, 'o', 0, 't', 0};
  memcpy(EP_MEM.res_data, string, sizeof(string));
  CHECK(memcmp(reply, EP_MEM.dap_data, sizeof(reply)) == 0);
  // The host can queue the next request before reading the previous reply.
  memset(EP_MEM.dap_recv, 0, sizeof(EP_MEM.dap_recv));
  EP_MEM.dap_recv[1] = 0xff; // DAP_INFO_PACKET_SIZE
  CHECK(memcmp(reply, EP_MEM.dap_data, sizeof(reply)) == 0);
  CHECK(!JTAG::dap_command_check());
  CHECK(EP_MEM.dap_data[0] == 0 && EP_MEM.dap_data[1] == 2 && EP_MEM.dap_data[2] == 64);
  acknowledge_reply();
}

int main() {
  static_assert(sizeof(JTAG_Packet_t) == 540, "Wire packet packing changed");
  static_assert(offsetof(JTAG_Packet_t, out.memData) == 17, "Write header changed");
  test_fragments();
  test_reads_and_responses();
  test_writes();
  test_parameters();
  test_control_during_dap_reply();
  printf("PASS: %u checks, %u-byte flash\n", checks, (unsigned)PROGMEM_SIZE);
}
