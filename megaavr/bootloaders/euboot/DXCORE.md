# euboot in DxCore

This directory vendors askn37/euboot at commit
66cdc00dac142bb1cf70f5fcbf5fbca5d45894ad (firmware 3.72.50).
LICENSE, the upstream Makefile, CRC generator and documentation are retained.
The historical PF2 LED / PF6 switch Curiosity Nano HEX is retained unmodified
at hex/euboot_LF2_SF6.hex for provenance. The Nano board now selects the locally
rebuilt hex/nano/euboot_avr64du32_LF2_SF6_regulator.hex.
The hardened Nano image is 2,848 bytes; standalone images are 2,850..2,878 bytes.
Nano and standalone 28/32-pin images are generated from locally adapted sources:
configuration.h selects the internal-regulator/external-VUSB mode, and nvm_cmd
uses .trampolines so the fixed reset/SPM ABI is independent of LTO function order.
usb.cpp preserves the USB session on RESUME instead of rebooting: Windows can
resume an idle HID device when an application opens it. Configuration descriptor
bmAttributes also sets the mandatory bit 7. On 2026-11-01 the user confirmed
installation of the 2,400-byte Nano image, a manual USB sketch upload with
AVRDUDE 8.1, CDC echo/LED operation, and an automatic upload without SW0 on
one Windows / Arduino IDE 2.x setup. These observations do not isolate which
change resolved the initial disconnect; sleep/resume and stress/recovery tests
remain outstanding. See ../../extras/Ref_USB.md for the validation record.

The current local builds harden HID fragment ordering/lengths, stop exhausted
responses, validate memory lengths and received write payloads, restrict writes
to application flash/EEPROM/USERROW/BOOTROW, and return RSP3_FAILED for refused
requests. Flash aliases use the same application-region protection. Parameter
headers and version-table reads are bounded too; unused parameter data is zeroed.
The native regression harness compiles the actual JTAG/NVM handlers for 32/64 KiB
targets with register/address-space stubs. Linux CI also enables ASan/UBSan.
This does not validate USB timing or physical NVM behavior.

The Intel HEX blob matches upstream Git blob
9abf09a11cda4b84f36048df285749b5a50569ad.
Its LF-normalized file SHA-256 is
cbfef177a209fc62970cb2fb55ee1a56400deacea618f9aeb7dceaab5a6bef18.
The HEX contains 2,428 bytes at flash 0x0000..0x097b, including the upstream
CRC. The full reserved region extends through 0x09ff.

Current local builds reserve six 512-byte sectors (BOOTSIZE=6), link the application
at 0x0c00 and preserve UPDI. Their extra checks exceed the old 2,560-byte reservation.
Reinstall the bootloader and fuses over UPDI and rebuild the sketch when upgrading
from the five-sector version. Do not mix old/new bootloaders, fuses and sketches.
The earlier 2,400-byte image's real-device results above do not validate this
hardened version; its USB, stress and recovery tests still need real hardware.
Its PF6 switch uses GPIO mode (SYSCFG0=0xd1).
Do not substitute an Optiboot image or a debug euboot image: their application
origins and startup conventions differ.

## Rebuild the original Nano firmware

The upstream Nano source uses MacroMicroAPI, not the DxCore API. For an
unmodified historical rebuild, check out upstream commit 66cdc00 and use the
MultiX Zinnia modernAVR SDK as documented in README.md / README_jp.md.
Install the SDK with AVR DU support and Arduino CLI, plus GNU make, AVR binutils
and Perl if the supplied CRC/padding format is required. Use WSL or another
supported POSIX build environment on Windows.

The supplied Makefile's Curiosity Nano target is:

```sh
make hex/euboot_LF2_SF6.hex
```

Set ACLIPATH and AVRROOT if arduino-cli / avr-objcopy are not on PATH. The target
sets LED_BUILTIN=PIN_PF2, SW_BUILTIN=PIN_PF6, NDEBUG and the internal 20 MHz clock.
The generated application origin must remain 0x0a00; reject a generated binary
that exceeds 2560 bytes. Before replacing the vendored binary, compare its
fuse values with this board, record the exact SDK/compiler versions, and run
the distributed asset tests and real-device recovery/upload checks.

An exact byte-for-byte rebuild also requires the original SDK/toolchain
revisions; the upstream repository does not lock those dependencies.
The historical image is useful for comparison. Normal Nano installation now
uses the adapted image described below, with a six-sector application layout.

## Standalone 28/32-pin builds

build_generic.py builds all four supported MCUs and both VUSB modes using
avr-g++ 7.3 and avr-libc, without installing the MacroMicroAPI SDK.
build_support supplies only the pin, delay and memory-header compatibility
needed by the pinned non-debug source; debug UART builds are intentionally excluded.
Its pin numbers encode physical ports and are not Arduino pin numbers.

```sh
python build_generic.py --toolchain /path/to/avr-gcc/bin --output /path/to/build
```

Add --write-assets to update hex/generic and its manifest after validation.
Add --nano to build only the Nano PF2 LED / PF6 switch image; with --write-assets
this updates hex/nano and its separate manifest. The historical HEX is untouched.
The script rejects images larger than 3072 bytes, changed reset/SPM ABI,
RAM that needs CRT initialization, and workspace placed outside the target SRAM.
The generated images use PA7 for the LED and PF6 for the active-low BOOT button.
No CRC is appended to these generic images; the board fuses disable CRCSCAN.

AVR64DU SRAM starts at 0x6000, AVR32DU SRAM at 0x7000; both end at 0x7fff.
Link separately for each capacity so global workspace addresses are valid.
The manifest records source hashes, compiler/device-header hashes and image
hashes and application origin. All generic boards reserve six sectors and preserve UPDI.
External VUSB uses USBVREG=0 and SYSCFG1=0x00; internal regulation uses
USBVREG=1 and SYSCFG1=0x08. Burn the matching bootloader whenever changing mode.

## Application integration

euboot saves reset flags in GPR0 and clears RSTCTRL.RSTFR. DxCore's
USING_EUBOOT startup branch preserves this saved value and selects the
application vector table. Do not define USING_OPTIBOOT or SPM_FROM_APP.

On a 1200 bps DTR falling edge, the CDC implementation finishes the control
status transfer, waits 50 USB frames, detaches, clears the SRAM-top marker
expected by this euboot revision and issues a software reset. It performs no
Arduino delay() call inside a USB ISR.

The uploader uses AVRDUDE's jtag3updi backend over HID (VID:PID 04d8:0b12), with
-D and verification enabled. A temporary additional configuration inherits
jtag3updi as dxcore_euboot and sets usbvid/usbpid explicitly; the port is usb.
AVRDUDE 8.1's HID backend can try usb:VID:PID directly, but after hid_open fails
it falls back to treating that suffix as a serial-number filter on the default
programmer IDs. The explicit configuration avoids this misleading fallback;
it does not by itself repair a HID device that cannot be opened.
It validates the application HEX before contacting
hardware, waits at most ten seconds for a single target, and never retries a
failed flash operation.

See [USB setup and recovery](../../extras/Ref_USB.md) for user instructions.
