"""Execute production HID and Keyboard/Mouse code using host USB register stubs."""
import os
from pathlib import Path
import subprocess
import tempfile
import unittest

from test_euboot_firmware import compiler_command

PLATFORM = Path(__file__).resolve().parents[2]
HOST = Path(__file__).with_name("usb_hid_host")


class HIDTests(unittest.TestCase):
    def test_hid_firmware(self):
        compiler = compiler_command()
        with tempfile.TemporaryDirectory(prefix="usb-hid-host-") as folder:
            executable = Path(folder) / ("hid.exe" if os.name == "nt" else "hid")
            includes = [HOST]
            if compiler.name.lower() in ("cl", "cl.exe"):
                version = compiler.resolve().parents[3]
                kits = Path("C:/Program Files (x86)/Windows Kits/10")
                sdk = max(path.name for path in (kits / "Include").iterdir()
                          if (kits / "Lib" / path.name / "ucrt/x64").is_dir())
                includes += [version / "include", kits / "Include" / sdk / "ucrt"]
                command = [str(compiler), "/nologo", "/std:c++17", "/EHsc", "/MT",
                           *("/I" + str(path) for path in includes), str(HOST / "hid.cpp"),
                           "/Fe:" + str(executable), "/Fo:" + str(executable.with_suffix(".obj")),
                           "/link", "/LIBPATH:" + str(version / "lib/x64"),
                           "/LIBPATH:" + str(kits / "Lib" / sdk / "ucrt/x64"),
                           "/LIBPATH:" + str(kits / "Lib" / sdk / "um/x64")]
            else:
                command = [str(compiler), "-std=c++17", "-g", "-O1", "-fpermissive",
                           *("-I" + str(path) for path in includes), str(HOST / "hid.cpp"),
                           "-o", str(executable)]
                if os.environ.get("EUBOOT_SANITIZE"):
                    command += ["-fsanitize=address,undefined", "-fno-sanitize-recover=all",
                                "-fno-omit-frame-pointer"]
            result = subprocess.run(command, capture_output=True, text=True, errors="replace", timeout=60)
            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
            result = subprocess.run([str(executable)], capture_output=True, text=True, timeout=30)
            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
            self.assertIn("PASS:", result.stdout)


if __name__ == "__main__":
    unittest.main()
