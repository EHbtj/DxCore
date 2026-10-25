# AVR DU CDC source provenance

Adapted from askn37/MacroMicroAPI_lib at
1804a5239181f33b2f07ab7bac4c83bfc74d9c8b, src/USB/CDC*.
Copyright (c) 2024 askn37 at github.com, MIT License; see LICENSE.

DxCore changes:

- Replaced MacroMicroAPI helpers with avr-libc, DxCore Stream and USBConfig.h.
- Added a separate SerialUSB object; UART Serial / Serial0 retain their meanings.
- Replaced host-dependent ISR waits with an EP0 DATA/STATUS state machine.
- Enabled EP0 completion interrupts; SOF handles deferred transmit/reset work.
- Used dedicated even-aligned buffers for SETUP, control DATA and CDC endpoints.
- Corrected contiguous interface numbers, descriptor lengths and IAD class codes;
  full-speed-only Device Qualifier requests are stalled.
- Used DOSTALL to implement endpoint halt and standard clear-halt handling.
- Made reception nonblocking and transmission bounded independently of millis().
- Kept USB suspend/resume separate from USB bus reset.
- Deferred the euboot SRAM marker/reset until the 1200 bps DTR-close status is ACKed.

The fixed euboot image is vendored separately under bootloaders/euboot.
Public Arduino APIs are declared in ../SerialUSB.h. USB/CDC internals are not
a supported PluggableUSB or HID API.
