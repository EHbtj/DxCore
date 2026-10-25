#!/usr/bin/env python3
"""Wait for AVR DU euboot's HID interface, then write and verify using AVRDUDE.

The IDE performs the 1200 bps CDC touch. This tool also works without a serial
port when SW0 was held at power-on. Windows only; Python's standard library is
sufficient. No retry is made after a flash operation has started.
"""

import argparse
import ctypes
from ctypes import wintypes
from pathlib import Path
import subprocess
import sys
import time

APP_START = 0x0A00
FLASH_END = 0x10000
BOOT_TIMEOUT = 10.0
RECOVERY = (
    "Hold SW0 (PF6), remove all target power, then reconnect target USB while "
    "holding SW0. Release SW0 and upload again. The onboard debugger's USB "
    "connector is used only for the initial UPDI bootloader installation."
)


def validate_hex(filename):
    """Reject invalid images and writes outside the application's flash region."""
    base = 0
    count = 0
    eof = False
    for line in Path(filename).read_text(encoding="ascii").splitlines():
        if not line.strip():
            continue
        if eof or not line.startswith(":"):
            raise ValueError("Invalid Intel HEX record")
        try:
            record = bytes.fromhex(line[1:])
        except ValueError as error:
            raise ValueError("Invalid Intel HEX encoding") from error
        if len(record) < 5 or record[0] + 5 != len(record) or sum(record) & 255:
            raise ValueError("Invalid Intel HEX length or checksum")
        length, address, kind = record[0], int.from_bytes(record[1:3], "big"), record[3]
        data = record[4:-1]
        if kind == 0:
            if length and not (APP_START <= base + address < base + address + length <= FLASH_END):
                raise ValueError("Image overlaps euboot or exceeds AVR64DU32 flash")
            count += length
        elif kind == 1 and length == 0 and address == 0:
            eof = True
        elif kind in (2, 4) and length == 2 and address == 0:
            base = int.from_bytes(data, "big") << (4 if kind == 2 else 16)
        elif kind in (3, 5) and length == 4 and address == 0:
            pass  # Optional start-address records do not contain flash bytes.
        else:
            raise ValueError("Unsupported Intel HEX record")
    if not eof or not count:
        raise ValueError("Incomplete or empty Intel HEX image")
    return count


def hid_paths(vid, pid):
    """Enumerate present HID interfaces using SetupAPI; never match stale registry entries."""
    if sys.platform != "win32":
        raise RuntimeError("This initial euboot uploader supports Windows only")

    class GUID(ctypes.Structure):
        _fields_ = [
            ("Data1", wintypes.DWORD), ("Data2", wintypes.WORD),
            ("Data3", wintypes.WORD), ("Data4", ctypes.c_ubyte * 8),
        ]

    class InterfaceData(ctypes.Structure):
        _fields_ = [
            ("cbSize", wintypes.DWORD), ("InterfaceClassGuid", GUID),
            ("Flags", wintypes.DWORD), ("Reserved", ctypes.c_void_p),
        ]

    hid = ctypes.WinDLL("hid", use_last_error=True)
    setup = ctypes.WinDLL("setupapi", use_last_error=True)
    guid = GUID()
    hid.HidD_GetHidGuid.argtypes = [ctypes.POINTER(GUID)]
    hid.HidD_GetHidGuid(ctypes.byref(guid))
    setup.SetupDiGetClassDevsW.argtypes = [ctypes.POINTER(GUID), wintypes.LPCWSTR, wintypes.HWND, wintypes.DWORD]
    setup.SetupDiGetClassDevsW.restype = ctypes.c_void_p
    setup.SetupDiEnumDeviceInterfaces.argtypes = [
        ctypes.c_void_p, ctypes.c_void_p, ctypes.POINTER(GUID),
        wintypes.DWORD, ctypes.POINTER(InterfaceData),
    ]
    setup.SetupDiEnumDeviceInterfaces.restype = wintypes.BOOL
    setup.SetupDiGetDeviceInterfaceDetailW.argtypes = [
        ctypes.c_void_p, ctypes.POINTER(InterfaceData), ctypes.c_void_p,
        wintypes.DWORD, ctypes.POINTER(wintypes.DWORD), ctypes.c_void_p,
    ]
    setup.SetupDiGetDeviceInterfaceDetailW.restype = wintypes.BOOL
    setup.SetupDiDestroyDeviceInfoList.argtypes = [ctypes.c_void_p]
    handle = setup.SetupDiGetClassDevsW(ctypes.byref(guid), None, None, 0x12)  # PRESENT | DEVICEINTERFACE
    if handle == ctypes.c_void_p(-1).value:
        raise ctypes.WinError(ctypes.get_last_error())
    paths = []
    try:
        index = 0
        while True:
            interface = InterfaceData()
            interface.cbSize = ctypes.sizeof(interface)
            if not setup.SetupDiEnumDeviceInterfaces(handle, None, ctypes.byref(guid), index, ctypes.byref(interface)):
                error = ctypes.get_last_error()
                if error == 259:  # ERROR_NO_MORE_ITEMS
                    break
                raise ctypes.WinError(error)
            required = wintypes.DWORD()
            setup.SetupDiGetDeviceInterfaceDetailW(
                handle, ctypes.byref(interface), None, 0, ctypes.byref(required), None,
            )
            if not required.value:
                raise ctypes.WinError(ctypes.get_last_error())
            detail = ctypes.create_string_buffer(required.value)
            ctypes.c_uint32.from_buffer(detail).value = 8 if ctypes.sizeof(ctypes.c_void_p) == 8 else 6
            if not setup.SetupDiGetDeviceInterfaceDetailW(
                handle, ctypes.byref(interface), detail, required.value, None, None,
            ):
                raise ctypes.WinError(ctypes.get_last_error())
            path = ctypes.wstring_at(ctypes.addressof(detail) + 4)
            if interface.Flags & 1 and "vid_%04x&pid_%04x" % (vid, pid) in path.lower():
                paths.append(path)
            index += 1
    finally:
        setup.SetupDiDestroyDeviceInfoList(handle)
    return paths


def wait_for_bootloader(vid, pid, timeout=BOOT_TIMEOUT):
    deadline = time.monotonic() + timeout
    while True:
        paths = hid_paths(vid, pid)
        if len(paths) > 1:
            raise RuntimeError("Multiple euboot devices found; connect only one target")
        if paths:
            return
        if time.monotonic() >= deadline:
            raise TimeoutError("euboot HID device did not appear within 10 seconds. " + RECOVERY)
        time.sleep(0.1)


def upload(args):
    if args.mcu != "avr64du32":
        raise ValueError("This uploader is configured for AVR64DU32 only")
    count = validate_hex(args.hex)
    command = [
        args.avrdude, "-C" + args.config, "-p" + args.mcu, "-cjtag3updi",
        "-Pusb:%04x:%04x" % (args.vid, args.pid), "-D",
        "-Uflash:w:" + str(Path(args.hex).resolve()) + ":i",
    ]
    if args.verbose:
        command.append("-v")
    if args.dry_run:
        print("Validated %d application bytes. Upload command:" % count)
        print(subprocess.list2cmdline(command))
        return 0
    print("Waiting for euboot HID %04X:%04X..." % (args.vid, args.pid), flush=True)
    wait_for_bootloader(args.vid, args.pid)
    print("Writing and verifying %d application bytes..." % count, flush=True)
    result = subprocess.run(command, check=False, timeout=120).returncode
    if result:
        print("AVRDUDE failed; no automatic retry. " + RECOVERY, file=sys.stderr)
    return result


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--avrdude", required=True)
    parser.add_argument("--config", required=True)
    parser.add_argument("--mcu", default="avr64du32")
    parser.add_argument("--vid", type=lambda value: int(value, 0), required=True)
    parser.add_argument("--pid", type=lambda value: int(value, 0), required=True)
    parser.add_argument("--hex", required=True)
    parser.add_argument("--verbose", action="store_true")
    parser.add_argument("--dry-run", action="store_true", help="validate and display the command without contacting hardware")
    args = parser.parse_args(argv)
    try:
        return upload(args)
    except (OSError, ValueError, RuntimeError, subprocess.TimeoutExpired) as error:
        print("euboot upload failed: " + str(error), file=sys.stderr)
        return 1


if __name__ == "__main__":
    sys.exit(main())
