"""Run the actual euboot JTAG/NVM handlers against host address-space stubs.

Use CXX to select a compiler. CI requires one and enables ASan/UBSan on Linux.
These tests do not connect to a target or replace real USB/recovery testing.
"""
import os
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest

PLATFORM = Path(__file__).resolve().parents[2]
HOST = Path(__file__).with_name("euboot_host")
BOOT = PLATFORM / "bootloaders/euboot"


def compiler_command():
    compiler = os.environ.get("CXX")
    if not compiler:
        compiler = next((shutil.which(name) for name in ("g++", "clang++", "c++", "cl")
                         if shutil.which(name)), None)
    # A normal Windows shell may not have the Visual Studio environment loaded.
    if not compiler and os.name == "nt":
        vswhere = Path("C:/Program Files (x86)/Microsoft Visual Studio/Installer/vswhere.exe")
        if vswhere.is_file():
            matches = subprocess.check_output([
                str(vswhere), "-latest", "-products", "*", "-find",
                "VC/Tools/MSVC/*/bin/Hostx64/x64/cl.exe",
            ], text=True).splitlines()
            compiler = next(iter(matches), None)
        if not compiler:
            matches = list(Path("C:/Program Files/Microsoft Visual Studio").glob(
                "*/*/VC/Tools/MSVC/*/bin/Hostx64/x64/cl.exe"))
            compiler = str(sorted(matches)[-1]) if matches else None
    if not compiler:
        if os.environ.get("EUBOOT_REQUIRE_COMPILER"):
            raise RuntimeError("A native C++ compiler is required for firmware tests")
        raise unittest.SkipTest("Native C++ compiler unavailable; set CXX to run firmware tests")
    return Path(shutil.which(compiler) or compiler)


class FirmwareTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.compiler = compiler_command()
        cls.temp = tempfile.TemporaryDirectory(prefix="euboot-host-")
        cls.addClassCleanup(cls.temp.cleanup)
        cls.executables = {}
        for capacity in (32768, 65536):
            executable = Path(cls.temp.name) / ("firmware_%d" % capacity + (".exe" if os.name == "nt" else ""))
            includes = [HOST, BOOT / "src", BOOT / "build_support"]
            if cls.compiler.name.lower() in ("cl", "cl.exe"):
                version = cls.compiler.resolve().parents[3]
                kits = Path("C:/Program Files (x86)/Windows Kits/10")
                sdk = max(path.name for path in (kits / "Include").iterdir()
                          if (kits / "Lib" / path.name / "ucrt/x64").is_dir())
                includes += [version / "include", kits / "Include" / sdk / "ucrt"]
                command = [str(cls.compiler), "/nologo", "/std:c++17", "/EHsc", "/MT",
                           "/DHOST_FLASH_SIZE=%d" % capacity,
                           *("/I" + str(path) for path in includes),
                           str(HOST / "firmware.cpp"), "/Fe:" + str(executable),
                           "/Fo:" + str(executable.with_suffix(".obj")), "/link",
                           "/LIBPATH:" + str(version / "lib/x64"),
                           "/LIBPATH:" + str(kits / "Lib" / sdk / "ucrt/x64"),
                           "/LIBPATH:" + str(kits / "Lib" / sdk / "um/x64")]
                if os.environ.get("EUBOOT_SANITIZE"):
                    command.insert(1, "/fsanitize=address")
            else:
                command = [str(cls.compiler), "-std=c++17", "-g", "-O1",
                           "-DHOST_FLASH_SIZE=%d" % capacity,
                           *("-I" + str(path) for path in includes),
                           str(HOST / "firmware.cpp"), "-o", str(executable)]
                if os.environ.get("EUBOOT_SANITIZE"):
                    command += ["-fsanitize=address,undefined", "-fno-sanitize-recover=all",
                                "-fno-omit-frame-pointer"]
            result = subprocess.run(command, capture_output=True, text=True, errors="replace", timeout=60)
            if result.returncode:
                raise RuntimeError("Native firmware compilation failed:\n" + result.stdout + result.stderr)
            cls.executables[capacity] = executable

    def run_firmware(self, capacity):
        result = subprocess.run([str(self.executables[capacity])], capture_output=True, text=True, timeout=30)
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        self.assertIn("PASS:", result.stdout)

    def test_32k_protocol_and_memory(self):
        self.run_firmware(32768)

    def test_64k_protocol_and_memory(self):
        self.run_firmware(65536)


if __name__ == "__main__":
    unittest.main()
