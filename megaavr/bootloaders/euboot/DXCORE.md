# euboot in DxCore

This directory vendors askn37/euboot at commit
66cdc00dac142bb1cf70f5fcbf5fbca5d45894ad (firmware 3.72.50).
Source files, LICENSE, the upstream Makefile, CRC generator and documentation
are retained. The bootloader sources and the supplied HEX are unmodified.
Only the PF2 LED / PF6 switch Curiosity Nano binary is distributed here.

The Intel HEX blob matches upstream Git blob
9abf09a11cda4b84f36048df285749b5a50569ad.
Its LF-normalized file SHA-256 is
cbfef177a209fc62970cb2fb55ee1a56400deacea618f9aeb7dceaab5a6bef18.
The HEX contains 2,428 bytes at flash 0x0000..0x097b, including the upstream
CRC. The full reserved region extends through 0x09ff.

The USB board reserves five 512-byte sectors (BOOTSIZE=5), links the application
at 0x0a00 and preserves UPDI. Its PF6 switch uses GPIO mode (SYSCFG0=0xd1).
Do not substitute an Optiboot image or a debug euboot image: their application
origins and startup conventions differ.

## Rebuild

The upstream source uses MacroMicroAPI, not the DxCore API. Rebuild with the
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
For installation of this fixed version, use the supplied verified HEX rather
than rebuilding with an arbitrary newer compiler.

## Application integration

euboot saves reset flags in GPR0 and clears RSTCTRL.RSTFR. DxCore's
USING_EUBOOT startup branch preserves this saved value and selects the
application vector table. Do not define USING_OPTIBOOT or SPM_FROM_APP.

On a 1200 bps DTR falling edge, the CDC implementation finishes the control
status transfer, waits 50 USB frames, detaches, clears the SRAM-top marker
expected by this euboot revision and issues a software reset. It performs no
Arduino delay() call inside a USB ISR.

The uploader uses AVRDUDE jtag3updi over HID (VID:PID 04d8:0b12), with -D and
verification enabled. It validates the application HEX before contacting
hardware, waits at most ten seconds for a single target, and never retries a
failed flash operation.

See [USB setup and recovery](../../extras/Ref_USB.md) for user instructions.
