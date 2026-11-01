/* Exercise production HID, EP0 request handling and Arduino APIs against USB registers.
 * This models firmware behavior, not physical USB timing or enumeration. */
#include <assert.h>
#include <stdio.h>
#include <vector>
#include <array>
#define DXCORE_USB_CDC
#define DXCORE_USB_HID
#define USING_EUBOOT
#define F_CPU 20000000L
#define RAMEND (reinterpret_cast<uintptr_t>(host_marker) + 1)
#define _PROTECTED_WRITE(reg, value) host_reboot()
// MSVC has no GCC packed attribute; USB wire types are packed in this test TU.
#ifdef _MSC_VER
  #define __attribute__(value)
  #define __builtin_unreachable() __assume(0)
  #pragma pack(push, 1)
#endif
#include "../../../cores/dxcore/usb/HID.h"
#include "../../../cores/dxcore/usb/CDC/constants.h"
#ifdef _MSC_VER
  #pragma pack(pop)
#endif
using namespace DxCoreUSB;
uint8_t SREG = CPU_I_bm, USB0_INTFLAGSB, USB0_INTFLAGSA, USB0_CTRLA, USB0_CTRLB, USB0_ADDR;
static USB_EP_TABLE_t table;
static USB_WM_TABLE_t work;
static Interface_State state;
static bool ready = true, consume = true;
static unsigned delays;
static uint8_t host_marker[2] = {0xff, 0xff};
static void host_reboot() { throw 1; }
static std::vector<std::array<uint8_t, 8>> keyboard_packets;
static std::vector<std::array<uint8_t, 4>> mouse_packets;

#undef USB_EP
#define USB_EP(id) (*(USB_EP_t *)(reinterpret_cast<uint8_t *>(table.EP) + (id)))
struct StatusWrite {
  uint8_t id;
  bool set;
  void operator=(uint8_t value) {
    auto &ep = USB_EP(id);
    ep.STATUS = set ? ep.STATUS | value : ep.STATUS & ~value;
    if (!set && (value & USB_BUSNAK_bm)) {
      if (id == USB_EP_KEYBOARD_IN) {
        std::array<uint8_t, 8> packet;
        memcpy(packet.data(), work.keyboard, 8);
        keyboard_packets.push_back(packet);
      } else if (id == USB_EP_MOUSE_IN) {
        std::array<uint8_t, 4> packet = {};
        memcpy(packet.data(), work.mouse, ep.CNT);
        mouse_packets.push_back(packet);
      }
    }
  }
};
#undef USB_EP_STATUS_CLR
#undef USB_EP_STATUS_SET
#define USB_EP_STATUS_CLR(id) (StatusWrite{(id), false})
#define USB_EP_STATUS_SET(id) (StatusWrite{(id), true})

void host_delay() {
  ++delays;
  if (consume) {
    USB_EP(USB_EP_KEYBOARD_IN).STATUS |= USB_BUSNAK_bm;
    USB_EP(USB_EP_MOUSE_IN).STATUS |= USB_BUSNAK_bm;
  }
}
namespace DxCoreUSB {
  USB_EP_TABLE_t *get_endpointer_ptr() { return &table; }
  USB_WM_TABLE_t *get_workmem_ptr() { return &work; }
  Interface_State *get_state() { return &state; }
  bool is_ready() { return ready && state.CONFIG; }
  void start() { ready = true; }
  void cb_clear_state() { state.CONFIG = 0; hid_reset(); }
  bool cb_set_configuration(uint8_t config) { state.CONFIG = config; hid_configure(config); return true; }
  uint8_t cb_get_configuration() { return state.CONFIG; }
  void ep_setup_clear() { memset(&table, 0, sizeof(table)); }
  void ep_setup_in_listen() { USB_EP_RES.STATUS = 0; }
  void ep_setup_out_listen() { USB_EP_REQ.STATUS = 0; }
  void ep_setup_stalled() { USB_EP_REQ.CTRL |= USB_DOSTALL_bm; USB_EP_RES.CTRL |= USB_DOSTALL_bm; }
  size_t cb_get_descriptor_data(uint8_t *, uint16_t) { return 0; }
  void send_serialstate(SerialState_t &) {}
  void cb_event_class_sof() { hid_sof(); }
  void bus_suspend() { ready = false; }
  void bus_wake() { ready = true; }
}
#include "../../../cores/dxcore/usb/HID.cpp"
#include "../../../cores/dxcore/usb/CDC/cpp/control.cpp"
#include "../../../cores/dxcore/Keyboard.cpp"
#include "../../../cores/dxcore/Mouse.cpp"

static void setup_request(uint8_t type, uint8_t request, uint16_t value, uint16_t iface, uint16_t length) {
  Setup_Packet packet = { {0}, value, iface, length };
  packet.bmRequestType = type;
  packet.bRequest = request;
  memcpy(&work.setup, &packet, sizeof(packet));
  handle_setup();
  USB0_INTFLAGSB = 0; // W1C SETUP flag in the hardware.
}
static bool stalled() {
  return USB_EP_RES.CTRL & USB_DOSTALL_bm;
}
static void ack() { host_delay(); }

static void requests() {
  setup_request(0x81, 6, 0x2200, 2, 255);
  assert(!stalled() && USB_EP_RES.CNT == 63 && work.data[0] == 5);
  setup_request(0x81, 6, 0x2100, 3, 9);
  assert(!stalled() && USB_EP_RES.CNT == 9 && work.data[7] == 52);
  setup_request(0x81, 6, 0x2201, 2, 255);
  assert(stalled());
  setup_request(0x81, 6, 0x2200, 4, 255);
  assert(stalled());
  setup_request(0xa1, 3, 0, 3, 1);
  assert(!stalled() && work.data[0] == 1);
  setup_request(0x21, 11, 0, 3, 0);
  assert(!stalled());
  const uint8_t mouse[] = {1, 5, 6, 7};
  assert(hid_send_mouse(mouse) && USB_EP(USB_EP_MOUSE_IN).CNT == 3);
  setup_request(0xa1, 1, 0x0100, 3, 255);
  assert(USB_EP_RES.CNT == 3 && work.data[0] == 1 && work.data[1] == 0 && work.data[2] == 0);
  setup_request(0x21, 11, 1, 3, 0);
  assert(!stalled() && hid_send_mouse(mouse) && USB_EP(USB_EP_MOUSE_IN).CNT == 4);
  setup_request(0x21, 11, 2, 3, 0);
  assert(stalled());
  setup_request(0x21, 9, 0x0200, 2, 1); // LED DATA OUT must arrive before status IN.
  assert(!stalled() && hid_leds() == 0);
  work.data[0] = 0xff;
  USB_EP_REQ.CNT = 1;
  USB_EP_REQ.STATUS = USB_BUSNAK_bm | USB_TRNCOMPL_bm;
  service_control();
  assert(hid_leds() == 0x1f && USB_EP_RES.CNT == 0);
  setup_request(0xa1, 1, 0x0200, 2, 1);
  assert(!stalled() && work.data[0] == 0x1f);
  setup_request(0x21, 9, 0x0200, 2, 1);
  USB_EP_REQ.CNT = 2;
  USB_EP_REQ.STATUS = USB_BUSNAK_bm | USB_TRNCOMPL_bm;
  service_control();
  assert(stalled() && hid_leds() == 0x1f);
  setup_request(0x21, 9, 0x0200, 3, 1);
  assert(stalled());
  setup_request(0x21, 10, 0x0101, 2, 0);
  assert(stalled()); // Nonzero report ID unsupported.
  setup_request(0x21, 10, 0x0100, 2, 0);
  assert(!stalled());
  setup_request(0xa1, 2, 0, 2, 1);
  assert(!stalled() && work.data[0] == 1);
  setup_request(0x81, 10, 0, 3, 1);
  assert(!stalled() && work.data[0] == 0);
  setup_request(0x02, 3, 0, 0x83, 0);
  assert(!stalled() && (USB_EP(USB_EP_KEYBOARD_IN).CTRL & USB_DOSTALL_bm));
  setup_request(0x02, 1, 0, 0x83, 0);
  assert(!stalled() && !(USB_EP(USB_EP_KEYBOARD_IN).CTRL & USB_DOSTALL_bm));
  setup_request(0x02, 3, 0, 0x03, 0);
  assert(stalled()); // No HID OUT endpoint advertised.
  state.CONFIG = 0;
  setup_request(0x21, 11, 0, 2, 0);
  assert(stalled());
  cb_set_configuration(1);
}

static void idle_and_timeouts() {
  const uint8_t mouse[] = {2, 17, 0, 1};
  assert(hid_send_mouse(mouse));
  setup_request(0x21, 10, 0x0100, 3, 0);
  ack();
  size_t before = mouse_packets.size();
  for (int i = 0; i < 3; ++i) { hid_sof(); }
  assert(mouse_packets.size() == before);
  hid_sof();
  assert(mouse_packets.size() == before + 1);
  assert((mouse_packets.back() == std::array<uint8_t, 4>{2, 0, 0, 0}));
  setup_request(0x21, 10, 0, 3, 0);
  before = mouse_packets.size();
  ack();
  for (int i = 0; i < 1100; ++i) { hid_sof(); }
  assert(mouse_packets.size() == before);
  consume = false;
  USB_EP(USB_EP_MOUSE_IN).STATUS = 0;
  delays = 0;
  assert(!hid_send_mouse(mouse) && delays == 1000); // Finite without millis().
  USB_EP(USB_EP_MOUSE_IN).CTRL |= USB_DOSTALL_bm;
  delays = 0;
  assert(!hid_send_mouse(mouse) && delays == 0);
  USB_EP(USB_EP_MOUSE_IN).CTRL &= ~USB_DOSTALL_bm;
  ready = false;
  assert(!hid_send_mouse(mouse));
  ready = true;
  SREG = 0;
  assert(!hid_send_mouse(mouse));
  SREG = CPU_I_bm;
  consume = true;
  cb_set_configuration(1);
}

static void api() {
  Keyboard.begin();
  Mouse.begin();
  assert(Keyboard.press(KEY_LEFT_CTRL));
  assert(Keyboard.press('A'));
  assert(Keyboard.press('B'));
  assert(keyboard_packets.back()[0] == 3);
  assert(Keyboard.release('A') && keyboard_packets.back()[0] == 3);
  assert(Keyboard.release('B') && keyboard_packets.back()[0] == 1);
  Keyboard.releaseAll();
  assert(Keyboard.press(KEY_LEFT_SHIFT));
  assert(Keyboard.press('A') && Keyboard.release('A'));
  assert(keyboard_packets.back()[0] == 2); // Explicit shift survives ASCII release.
  Keyboard.releaseAll();
  for (uint8_t key = 'a'; key <= 'f'; ++key) { assert(Keyboard.press(key)); }
  size_t before = keyboard_packets.size();
  assert(!Keyboard.press('g') && Keyboard.getWriteError());
  assert(keyboard_packets.size() == before);
  assert(Keyboard.release('c') && Keyboard.press('g'));
  Keyboard.releaseAll();
  assert(Keyboard.press(KEY_F24) && keyboard_packets.back()[2] == 0x73);
  Keyboard.releaseAll();
  before = keyboard_packets.size();
  assert(Keyboard.write("Ab!\r\n") == 5);
  assert(keyboard_packets.size() == before + 8); // CR ignored, one Enter press/release.
  assert((keyboard_packets.back() == std::array<uint8_t, 8>{}));
  Mouse.press(MOUSE_RIGHT);
  assert(Mouse.isPressed(MOUSE_RIGHT));
  Mouse.move(-128, 127, -128);
  assert((mouse_packets.back() == std::array<uint8_t, 4>{2, 129, 127, 129}));
  Mouse.click(MOUSE_LEFT);
  assert(mouse_packets.back()[0] == MOUSE_RIGHT); // Preserve held buttons.
  Mouse.end();
  assert(!Mouse.isPressed(MOUSE_ALL));
  ready = false;
  Keyboard.clearWriteError();
  assert(!Keyboard.press('x') && Keyboard.getWriteError());
  ready = true;
  assert(Keyboard.press('y') && keyboard_packets.back()[2] == 0x1c);
  Keyboard.end();
  assert(is_ready()); // end() must leave CDC operational.
  cb_set_configuration(0);
  assert(USB_EP(USB_EP_MOUSE_IN).CTRL == 0 && USB_EP(USB_EP_KEYBOARD_IN).CTRL == 0);
  hid_reset();
  assert(hid_leds() == 0);
}

static void bootloader_touch() {
  cb_set_configuration(1);
  state.LineEncoding.dwDTERate = 1200;
  setup_request(0x21, 0x22, 1, 0, 0); // CDC DTR open.
  assert(!stalled());
  setup_request(0x21, 0x22, 0, 0, 0); // CDC DTR close while HID is configured.
  assert(!stalled());
  for (int i = 0; i < 60; ++i) { tick_bootloader(); }
  assert(host_marker[0] == 0xff); // Never reboot before status ACK.
  USB_EP_RES.STATUS = USB_BUSNAK_bm | USB_TRNCOMPL_bm;
  service_control();
  for (int i = 0; i < 49; ++i) { tick_bootloader(); }
  assert(host_marker[0] == 0xff);
  bool rebooted = false;
  try {
    tick_bootloader();
  } catch (int) {
    rebooted = true;
  }
  assert(rebooted && host_marker[0] == 0 && host_marker[1] == 0);
  assert(!(USB0_CTRLB & USB_ATTACH_bm) && USB0_CTRLA == 0);
}

int main() {
  cb_set_configuration(1);
  requests();
  idle_and_timeouts();
  api();
  bootloader_touch();
  puts("PASS: HID EP0, protocols, idle, timeouts, Keyboard/Mouse APIs and CDC bootloader touch");
}
