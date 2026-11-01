#!/usr/bin/env python3
"""Compile USB sketches with Arduino CLI and validate each resulting ELF/HEX."""
import argparse
import os
from pathlib import Path
import subprocess

from verify_usb_build import verify


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--fqbn", required=True)
    parser.add_argument("--mcu", default="avr64du32")
    parser.add_argument("--build-root", type=Path, required=True)
    parser.add_argument("sketches", nargs="+", type=Path)
    args = parser.parse_args()
    for sketch in args.sketches:
        build = args.build_root.resolve() / sketch.name
        command = [os.environ.get("ARDUINO_CLI", "arduino-cli"), "compile", "--fqbn", args.fqbn,
                   "--build-path", str(build), str(sketch.resolve())]
        print("Compiling " + str(sketch) + " for " + args.fqbn, flush=True)
        subprocess.run(command, check=True)
        verify(build / (sketch.name + ".ino.elf"), args.mcu)


if __name__ == "__main__":
    main()
