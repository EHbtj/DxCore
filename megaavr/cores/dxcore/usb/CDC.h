/**
 * AVR DU CDC internals, adapted for DxCore from MacroMicroAPI_lib 1804a52.
 * Copyright (c) 2024 askn37 at github.com. MIT License (see LICENSE).
 */
#pragma once
#include <avr/io.h>
#if defined(DXCORE_USB_CDC) && defined(USB0)
#if F_CPU < 12000000L
  #error "AVR DU USB requires a peripheral clock of at least 12 MHz."
#endif
#include <stddef.h>
#define USB_NAMESPACE DxCoreUSB
#include "CDC/define.h"

namespace USB_NAMESPACE {
  void start();
  void stop();
  bool is_ready();
  bool is_busy();
  bool is_recv_ready();
  bool is_send_ready();
  Interface_State* get_state();
  LineState_t& get_line_state();
  LineEncoding_t& get_line_encoding();
  uint16_t get_frame();
  size_t write_byte(uint8_t value);
  size_t write_available();
  void write_flush();
  size_t read_available();
  int read_byte();
  int peek_byte();
  void send_serialstate(SerialState_t& state);
  void bus_attach();
  void bus_detach();
  void bus_reset();
  void bus_resume();
  void bus_suspend();
  void bus_wake();
  void service_control();
  void tick_bootloader();
  void handle_setup();
  void cb_event_class_sof();
  void handling_bus_events();
  void handling_transaction_events();
}
#endif
