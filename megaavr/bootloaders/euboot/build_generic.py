#!/usr/bin/env python3
"""Build local euboot assets for four AVR DU targets and two VUSB power modes.

python build_generic.py --toolchain path/to/avr-gcc/bin --output path/to/build
Use --write-assets to copy validated HEX files and a manifest to hex/generic.
Use --nano to build the locally adapted PF2 LED Nano image instead.
No external service or target hardware is accessed. The upstream Nano HEX is unchanged.
"""
import argparse
import hashlib
import json
from pathlib import Path
import shutil
import struct
import subprocess

MCUS = ("avr64du32", "avr32du32", "avr64du28", "avr32du28")
BOOT_BYTES = 6 * 512


def elf_sections(filename):
    raw = Path(filename).read_bytes()
    header = struct.unpack_from("<16sHHIIIIIHHHHHH", raw)
    if header[0][:6] != b"\x7fELF\x01\x01":
        raise ValueError("Expected an AVR ELF32 executable")
    sections = [struct.unpack_from("<IIIIIIIIII", raw, header[6] + i * header[11]) for i in range(header[12])]
    names = sections[header[13]]
    names = raw[names[4]:names[4] + names[5]]
    return {
        names[section[0]:names.index(0, section[0])].decode("ascii"):
        {"size": section[5], "address": section[3]} for section in sections
    }


def image_bytes(filename):
    data = {}
    base = 0
    for line in Path(filename).read_text(encoding="ascii").splitlines():
        record = bytes.fromhex(line[1:])
        if len(record) != record[0] + 5 or sum(record) & 255:
            raise ValueError("Invalid generated Intel HEX")
        address = int.from_bytes(record[1:3], "big")
        if record[3] == 0:
            data.update((base + address + i, value) for i, value in enumerate(record[4:-1]))
        elif record[3] == 4:
            base = int.from_bytes(record[4:-1], "big") << 16
    return bytes(data.get(i, 255) for i in range(max(data) + 1))


def build(toolchain, output, write_assets=False, nano=False):
    root = Path(__file__).resolve().parent
    toolchain = Path(toolchain).resolve()
    output = Path(output).resolve()
    output.mkdir(parents=True, exist_ok=True)
    suffix = ".exe" if (toolchain / "avr-g++.exe").exists() else ""
    compiler = str(toolchain / ("avr-g++" + suffix))
    objcopy = str(toolchain / ("avr-objcopy" + suffix))
    version = subprocess.check_output([compiler, "--version"], text=True).splitlines()[0]
    manifest = {
        "upstream": "66cdc00dac142bb1cf70f5fcbf5fbca5d45894ad",
        "compiler": version,
        "compiler_sha256": hashlib.sha256(Path(compiler).read_bytes()).hexdigest(),
        "sources": {},
        "device_headers": {},
        "images": {},
    }
    for path in sorted([Path(__file__), *(root / "src").glob("*"), *(root / "build_support").rglob("*")]):
        if path.is_file():
            normalized = path.read_bytes().replace(b"\r\n", b"\n")
            manifest["sources"][path.relative_to(root).as_posix()] = hashlib.sha256(normalized).hexdigest()
    targets = ("avr64du32",) if nano else MCUS
    for mcu in targets:
        path = toolchain.parent / "avr/include/avr" / ("io" + mcu + ".h")
        manifest["device_headers"][mcu] = hashlib.sha256(path.read_bytes().replace(b"\r\n", b"\n")).hexdigest()
    sources = [str(root / "src" / name) for name in ("main.cpp", "sys.cpp", "usb.cpp", "jtag.cpp", "nvmv4.cpp")]
    for mcu in targets:
        modes = (("regulator", 1),) if nano else (("regulator", 1), ("external", 0))
        for mode, regulator in modes:
            led = "LF2" if nano else "LA7"
            name = "euboot_%s_%s_SF6_%s" % (mcu, led, mode)
            elf, hexfile = output / (name + ".elf"), output / (name + ".hex")
            command = [
                compiler, "-mmcu=" + mcu, "-DF_CPU=20000000L", "-DNDEBUG",
                "-DLED_BUILTIN=" + ("PIN_PF2" if nano else "PIN_PA7"), "-DSW_BUILTIN=PIN_PF6",
                "-DEUBOOT_USB_VREG=" + str(regulator),
                "-I" + str(root / "build_support"),
                "-std=gnu++17", "-Os", "-flto", "-mrelax", "-fshort-enums",
                "-ffunction-sections", "-fdata-sections", "-fno-exceptions",
                "-fno-threadsafe-statics", "-nostartfiles",
                "-Wl,--gc-sections,--section-start=.text=0,-Map=" + str(output / (name + ".map")),
                *sources, "-o", str(elf),
            ]
            subprocess.run(command, check=True)
            subprocess.run([objcopy, "-O", "ihex", "-j", ".text", "-j", ".data", str(elf), str(hexfile)], check=True)
            image = image_bytes(hexfile)
            if not 14 <= len(image) <= BOOT_BYTES:
                raise ValueError(name + " does not fit the six-sector boot reservation")
            # Fixed euboot ABI: RJMP main, ST Z+,R22 / RET / LD R24,Z+ / RET / SPM Z+ / RET.
            if int.from_bytes(image[:2], "little") & 0xF000 != 0xC000 or image[2:14] != bytes.fromhex("6193089581910895f8950895"):
                raise ValueError(name + " has an invalid reset/SPM snippet layout")
            sections = elf_sections(elf)
            sram_size = 8192 if "64" in mcu else 4096
            sram_start = 0x8000 - sram_size
            if sections[".data"]["size"] or sections.get(".bss", {}).get("size", 0):
                raise ValueError(name + " requires CRT RAM initialization")
            workspace = sections[".noinit"]
            if workspace["address"] != 0x800000 + sram_start or workspace["size"] > sram_size - 256:
                raise ValueError(name + " has invalid SRAM workspace/stack placement")
            manifest["images"][hexfile.name] = {
                "mcu": mcu, "power": mode, "bytes": len(image),
                "app_start": BOOT_BYTES,
                "ram_start": sram_start, "ramend": 0x7FFF, "static_ram_bytes": workspace["size"],
                "sha256": hashlib.sha256(image).hexdigest(),
            }
            print(name + ": %d bytes" % len(image), flush=True)
            if write_assets:
                destination = root / ("hex/nano" if nano else "hex/generic")
                destination.mkdir(exist_ok=True)
                shutil.copyfile(hexfile, destination / hexfile.name)
    (output / "manifest.json").write_text(json.dumps(manifest, indent=2) + "\n", encoding="utf-8")
    if write_assets:
        shutil.copyfile(output / "manifest.json", root / ("hex/nano/manifest.json" if nano else "hex/generic/manifest.json"))


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--toolchain", required=True)
    parser.add_argument("--output", required=True)
    parser.add_argument("--write-assets", action="store_true")
    parser.add_argument("--nano", action="store_true", help="Build the adapted Curiosity Nano PF2 LED image")
    arguments = parser.parse_args()
    build(arguments.toolchain, arguments.output, arguments.write_assets, arguments.nano)
