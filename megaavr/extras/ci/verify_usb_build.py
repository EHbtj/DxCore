#!/usr/bin/env python3
"""Validate a native-USB ELF/HEX pair without target hardware.

Usage: python megaavr/extras/ci/verify_usb_build.py path/to/sketch.ino.elf [avr32du28]
"""
import importlib.util
from pathlib import Path
import struct
import sys


def verify(filename, mcu="avr64du32"):
    filename = Path(filename)
    raw = filename.read_bytes()
    header = struct.unpack_from("<16sHHIIIIIHHHHHH", raw)
    if header[0][:6] != b"\x7fELF\x01\x01":
        raise ValueError("Expected an ELF32 little-endian AVR executable")
    sections = [
        struct.unpack_from("<IIIIIIIIII", raw, header[6] + index * header[11])
        for index in range(header[12])
    ]
    names = sections[header[13]]
    names = raw[names[4]:names[4] + names[5]]

    def name(table, offset):
        return table[offset:table.index(0, offset)].decode("ascii")

    by_name = {name(names, section[0]): section for section in sections}
    assert by_name[".text"][3] == 0xA00, "Application vectors must start at 0x0A00"
    symbols = {}
    for section in sections:
        if section[1] != 2:
            continue
        strings = sections[section[6]]
        strings = raw[strings[4]:strings[4] + strings[5]]
        for offset in range(section[4], section[4] + section[5], section[9]):
            index, value, size, info, other, shndx = struct.unpack_from("<IIIBBH", raw, offset)
            if index and shndx < len(sections):
                symbols[name(strings, index)] = (value, size, sections[shndx])

    def symbol(needle):
        matches = [(key, value) for key, value in symbols.items() if needle in key]
        assert len(matches) == 1, "Expected exactly one symbol containing " + needle
        return matches[0][1]

    def data(needle, wire_size=None):
        address, size, section = symbol(needle)
        if wire_size is not None:
            size = wire_size  # GCC reports only the header size for flexible-array objects.
        offset = section[4] + address - section[3]
        return raw[offset:offset + size]

    device = data("device_descriptor")
    assert len(device) == 18 and device[:2] == b"\x12\x01"
    assert device[4:7] == b"\xef\x02\x01", "IAD composite class must match the CDC association"
    assert device[7] == 64 and device[8:12] == b"\xd8\x04\x15\x0b"
    assert data("language_string", 4) == b"\x04\x03\x09\x04"
    config = data("general_configuration_descriptor")
    assert int.from_bytes(config[2:4], "little") == len(config)
    assert config[4] == 2 and config[7] == 0x80
    interfaces, endpoints = [], []
    offset = 0
    while offset < len(config):
        length, kind = config[offset:offset + 2]
        assert length >= 2 and offset + length <= len(config)
        item = config[offset:offset + length]
        if kind == 4:
            interfaces.append(item[2])
        if kind == 5:
            endpoints.append(item[2])
        offset += length
    assert interfaces == [0, 1]
    assert endpoints == [0x81, 0x02, 0x82]
    for needle in ("EP_TABLE", "EP_BUFFER"):
        assert symbol(needle)[0] % 2 == 0, "USB DMA allocation must have even alignment"

    module = Path(__file__).resolve().parents[2] / "tools/euboot_upload.py"
    spec = importlib.util.spec_from_file_location("euboot_upload", module)
    uploader = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(uploader)
    count = uploader.validate_hex(filename.with_suffix(".hex"), mcu)
    ram_start = 0x8000 - uploader.FLASH_SIZES[mcu] // 8
    for section_name in (".data", ".bss", ".noinit"):
        section = by_name.get(section_name)
        if section and section[5]:
            assert 0x800000 + ram_start <= section[3] < section[3] + section[5] <= 0x808000, \
                section_name + " must fit the selected MCU's SRAM"
    print("PASS: vectors at 0x0A00, %d HEX bytes within application flash, CDC descriptors and DMA alignment" % count)


if __name__ == "__main__":
    verify(sys.argv[1], sys.argv[2] if len(sys.argv) > 2 else "avr64du32")
