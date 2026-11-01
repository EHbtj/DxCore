"""Host-side euboot uploader regression tests. No target is programmed."""
import argparse
import importlib.util
from pathlib import Path
import subprocess
import tempfile
import unittest
from unittest.mock import Mock, patch

SPEC = importlib.util.spec_from_file_location(
    "euboot_upload", Path(__file__).resolve().parents[1] / "euboot_upload.py",
)
uploader = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(uploader)


def record(address, kind, data=b""):
    body = bytes([len(data), address >> 8, address & 255, kind]) + data
    return ":" + (body + bytes([-sum(body) & 255])).hex().upper()


class UploaderTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.image = Path(self.temp.name) / "application with spaces.hex"

    def write_image(self, *records):
        self.image.write_text("\n".join(records), encoding="ascii")
        return str(self.image)

    def valid_image(self):
        return self.write_image(record(0xA00, 0, b"\x00\x00"), record(0, 1))

    def test_valid_application_and_last_flash_byte(self):
        name = self.write_image(record(0xA00, 0, b"\x01"), record(0xFFFF, 0, b"\x02"), record(0, 1))
        self.assertEqual(uploader.validate_hex(name), 2)

    def test_bootloader_and_flash_overflow_are_rejected(self):
        for address, payload in ((0, b"\x01"), (0x9FF, b"\x01\x02"), (0xFFFF, b"\x01\x02")):
            with self.subTest(address=address):
                name = self.write_image(record(address, 0, payload), record(0, 1))
                with self.assertRaises(ValueError):
                    uploader.validate_hex(name)

    def test_extended_address_cannot_bypass_flash_limit(self):
        name = self.write_image(record(0, 4, b"\x00\x01"), record(0xA00, 0, b"\x01"), record(0, 1))
        with self.assertRaises(ValueError):
            uploader.validate_hex(name)

    def test_each_target_has_its_own_flash_boundary(self):
        for mcu, capacity in uploader.FLASH_SIZES.items():
            with self.subTest(mcu=mcu):
                name = self.write_image(record(capacity - 1, 0, b"\x01"), record(0, 1))
                self.assertEqual(uploader.validate_hex(name, mcu), 1)
                if capacity == 32768:
                    name = self.write_image(record(0x8000, 0, b"\x01"), record(0, 1))
                    with self.assertRaisesRegex(ValueError, mcu.upper()):
                        uploader.validate_hex(name, mcu)

    def test_unsupported_mcu_is_rejected_before_contacting_hardware(self):
        args = self.args()
        args.mcu = "avr16du32"
        with patch.object(uploader, "wait_for_bootloader") as wait:
            with self.assertRaisesRegex(ValueError, "Unsupported"):
                uploader.upload(args)
        wait.assert_not_called()

    def test_upload_passes_the_selected_mcu_to_avrdude(self):
        for mcu in uploader.FLASH_SIZES:
            args = self.args()
            args.mcu = mcu
            with self.subTest(mcu=mcu), patch.object(uploader, "wait_for_bootloader"), \
                 patch.object(uploader.subprocess, "run", return_value=Mock(returncode=0)) as run:
                self.assertEqual(uploader.upload(args), 0)
            self.assertIn("-p" + mcu, run.call_args.args[0])

    def test_invalid_checksum_and_missing_eof_are_rejected(self):
        for content in (":020A00000000F5", record(0xA00, 0, b"\x01"), record(0, 1)):
            self.image.write_text(content, encoding="ascii")
            with self.subTest(content=content), self.assertRaises(ValueError):
                uploader.validate_hex(self.image)

    def test_no_records_after_eof(self):
        name = self.write_image(record(0xA00, 0, b"\x01"), record(0, 1), record(0xA01, 0, b"\x01"))
        with self.assertRaises(ValueError):
            uploader.validate_hex(name)

    def test_waits_for_hid_without_a_com_port(self):
        with patch.object(uploader, "hid_paths", side_effect=[[], [], ["hid-device"]]), \
             patch.object(uploader.time, "sleep") as sleep:
            uploader.wait_for_bootloader(0x04D8, 0x0B12)
        self.assertEqual(sleep.call_count, 2)

    def test_wait_timeout_includes_manual_recovery(self):
        with patch.object(uploader, "hid_paths", return_value=[]), \
             patch.object(uploader.time, "monotonic", side_effect=[0, 0, 10]), \
             patch.object(uploader.time, "sleep"):
            with self.assertRaisesRegex(TimeoutError, "SW0"):
                uploader.wait_for_bootloader(0x04D8, 0x0B12)

    def test_multiple_targets_are_rejected(self):
        with patch.object(uploader, "hid_paths", return_value=["one", "two"]):
            with self.assertRaisesRegex(RuntimeError, "Multiple"):
                uploader.wait_for_bootloader(0x04D8, 0x0B12)

    def args(self):
        return argparse.Namespace(
            avrdude="C:/tools with spaces/avrdude.exe", config="C:/tools/avrdude.conf",
            mcu="avr64du32", vid=0x04D8, pid=0x0B12, hex=self.valid_image(), verbose=False, dry_run=False,
        )

    def test_upload_preserves_bootloader_and_verifies(self):
        args = self.args()
        with patch.object(uploader, "wait_for_bootloader"), \
             patch.object(uploader.subprocess, "run", return_value=Mock(returncode=0)) as run:
            self.assertEqual(uploader.upload(args), 0)
        command = run.call_args.args[0]
        self.assertIn("-D", command)
        self.assertNotIn("-V", command)
        self.assertIn("-cdxcore_euboot", command)
        self.assertIn("-Pusb", command)
        self.assertFalse(any(item.startswith("-Pusb:") for item in command))
        self.assertIn("-C" + args.config, command)
        self.assertTrue(any(item.startswith("-C+") for item in command))
        self.assertIn("-Uflash:w:" + str(self.image.resolve()) + ":i", command)
        self.assertNotIn("shell", run.call_args.kwargs)

    def test_programmer_configuration_is_available_to_child_and_removed_afterwards(self):
        args = self.args()
        configs = []

        def inspect_config(command, **kwargs):
            filename = Path(next(item[3:] for item in command if item.startswith("-C+")))
            configs.append(filename)
            text = filename.read_text(encoding="ascii")
            self.assertIn('programmer parent "jtag3updi"', text)
            self.assertIn('id = "dxcore_euboot";', text)
            self.assertIn("usbvid = 0x04d8;", text)
            self.assertIn("usbpid = 0x0b12;", text)
            return Mock(returncode=0)

        with patch.object(uploader, "wait_for_bootloader"), \
             patch.object(uploader.subprocess, "run", side_effect=inspect_config):
            self.assertEqual(uploader.upload(args), 0)
        self.assertEqual(len(configs), 1)
        self.assertFalse(configs[0].exists())

    def test_custom_usb_ids_are_used_in_configuration_and_hid_search(self):
        args = self.args()
        args.vid, args.pid = 0x1234, 0x5678

        def inspect_config(command, **kwargs):
            filename = next(item[3:] for item in command if item.startswith("-C+"))
            text = Path(filename).read_text(encoding="ascii")
            self.assertIn("usbvid = 0x1234;", text)
            self.assertIn("usbpid = 0x5678;", text)
            return Mock(returncode=0)

        with patch.object(uploader, "wait_for_bootloader") as wait, \
             patch.object(uploader.subprocess, "run", side_effect=inspect_config):
            self.assertEqual(uploader.upload(args), 0)
        wait.assert_called_once_with(0x1234, 0x5678)

    def test_invalid_usb_ids_are_rejected_before_contacting_hardware(self):
        for vid, pid in ((-1, 0xB12), (0x10000, 0xB12), (0x4D8, -1), (0x4D8, 0x10000)):
            args = self.args()
            args.vid, args.pid = vid, pid
            with self.subTest(vid=vid, pid=pid), \
                 patch.object(uploader, "wait_for_bootloader") as wait, \
                 self.assertRaisesRegex(ValueError, "16-bit"):
                uploader.upload(args)
            wait.assert_not_called()

    def test_flash_failure_is_returned_without_retry(self):
        args = self.args()
        with patch.object(uploader, "wait_for_bootloader"), \
             patch.object(uploader.subprocess, "run", return_value=Mock(returncode=7)) as run:
            self.assertEqual(uploader.upload(args), 7)
        run.assert_called_once()

    def test_dry_run_never_contacts_target(self):
        args = self.args()
        args.dry_run = True
        with patch.object(uploader, "wait_for_bootloader") as wait, \
             patch.object(uploader.subprocess, "run") as run:
            self.assertEqual(uploader.upload(args), 0)
        wait.assert_not_called()
        run.assert_not_called()

    def test_invalid_image_never_contacts_target(self):
        args = self.args()
        self.write_image(record(0, 0, b"\x01"), record(0, 1))
        with patch.object(uploader, "wait_for_bootloader") as wait:
            with self.assertRaises(ValueError):
                uploader.upload(args)
        wait.assert_not_called()

    def test_subprocess_timeout_is_reported(self):
        args = self.args()
        with patch.object(uploader, "wait_for_bootloader"), \
             patch.object(uploader.subprocess, "run", side_effect=subprocess.TimeoutExpired("avrdude", 120)):
            with self.assertRaises(subprocess.TimeoutExpired):
                uploader.upload(args)


if __name__ == "__main__":
    unittest.main()
