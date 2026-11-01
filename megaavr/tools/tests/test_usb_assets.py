"""Check the distributed bootloader and the board's memory/fuse contract."""
import hashlib
import importlib.util
import json
from pathlib import Path
import re
import unittest

PLATFORM = Path(__file__).resolve().parents[2]
BOOT = PLATFORM / "bootloaders/euboot"
BOOT_SPEC = importlib.util.spec_from_file_location("build_generic", BOOT / "build_generic.py")
generic = importlib.util.module_from_spec(BOOT_SPEC)
BOOT_SPEC.loader.exec_module(generic)


def board_entries(board):
    entries = {}
    prefix = board + "."
    for line in (PLATFORM / "boards.txt").read_text(encoding="utf-8").splitlines():
        if line.startswith(prefix):
            key, value = line[len(prefix):].split("=", 1)
            if key in entries:
                raise AssertionError("Duplicate board property: " + prefix + key)
            entries[key] = value
    return entries


def board_config(board, **choices):
    entries = board_entries(board)
    config = {key: value for key, value in entries.items() if not key.startswith("menu.")}
    defaults = {}
    for key in entries:
        fields = key.split(".")
        if fields[0] == "menu" and len(fields) == 3:
            defaults.setdefault(fields[1], fields[2])
    for menu, default in defaults.items():
        prefix = "menu." + menu + "." + choices.pop(menu, default) + "."
        config.update((key[len(prefix):], value) for key, value in entries.items() if key.startswith(prefix))
    if choices:
        raise AssertionError("Unknown menus: " + str(choices))
    return config


def resolved(config, key):
    value = config[key]
    for _ in range(10):
        if "{" not in value:
            return value
        value = re.sub(r"\{([^}]+)\}", lambda match: config[match[1]], value)
    raise AssertionError("Recursive property: " + key)


class USBAssetTests(unittest.TestCase):
    def test_usb_hid_mode_preserves_bootloader_and_upload_contract(self):
        for board in ("avrduusb", "avrduusb_generic"):
            cdc = board_config(board)
            hid = board_config(board, usbmode="hid")
            with self.subTest(board=board):
                self.assertEqual(resolved(cdc, "build.pid"), "0x0B15")
                self.assertEqual(resolved(hid, "build.pid"), "0x0B16")
                self.assertNotIn("-DDXCORE_USB_HID", resolved(cdc, "build.extra_flags"))
                self.assertIn("-DDXCORE_USB_HID", resolved(hid, "build.extra_flags"))
                self.assertIn("-DDXCORE_USB_CDC", resolved(hid, "build.extra_flags"))
                self.assertEqual(hid["upload.use_1200bps_touch"], "true")
                for key in cdc:
                    if key.startswith(("bootloader.", "upload.")):
                        self.assertEqual(cdc[key], hid[key], key)

    def test_bootloader_matches_pinned_upstream_blob(self):
        text = (PLATFORM / "bootloaders/euboot/hex/euboot_LF2_SF6.hex").read_text(encoding="ascii")
        data = text.encode("ascii")
        blob = b"blob " + str(len(data)).encode("ascii") + b"\0" + data
        self.assertEqual(hashlib.sha1(blob).hexdigest(), "9abf09a11cda4b84f36048df285749b5a50569ad")

    def test_bootloader_fits_reserved_region(self):
        base = 0
        addresses = []
        for line in (PLATFORM / "bootloaders/euboot/hex/euboot_LF2_SF6.hex").read_text().splitlines():
            entry = bytes.fromhex(line[1:])
            self.assertEqual(sum(entry) & 255, 0)
            length, kind = entry[0], entry[3]
            self.assertEqual(length + 5, len(entry))
            address = int.from_bytes(entry[1:3], "big")
            if kind == 0:
                addresses.extend(range(base + address, base + address + length))
            elif kind == 4:
                base = int.from_bytes(entry[4:-1], "big") << 16
        self.assertEqual(min(addresses), 0)
        self.assertLess(max(addresses), 0xA00)

    def test_board_memory_and_boot_fuses_agree(self):
        board = {}
        for line in (PLATFORM / "boards.txt").read_text(encoding="utf-8").splitlines():
            if line.startswith("avrduusb."):
                key, value = line.split("=", 1)
                self.assertNotIn(key, board)
                board[key] = value
        start = int(board["avrduusb.build.text_section_start"].split("=")[1], 0)
        self.assertEqual(start, generic.BOOT_BYTES)
        self.assertEqual(start, int(board["avrduusb.bootloader.BOOTSIZE"], 0) * 512)
        self.assertEqual(start + int(board["avrduusb.upload.maximum_size"]), 65536)
        self.assertEqual(int(resolved(board_config("avrduusb"), "bootloader.SYSCFG0"), 0), 0xD1)
        self.assertEqual(board["avrduusb.bootloader.CODESIZE"], "0x00")
        self.assertEqual(board["avrduusb.upload.wait_for_upload_port"], "false")
        self.assertEqual(board["avrduusb.upload.use_1200bps_touch"], "true")
        self.assertIn("-DUSING_EUBOOT", board["avrduusb.build.extra_flags"])
        self.assertNotIn("SPM_FROM_APP", board["avrduusb.build.extra_flags"])
        # CLI resolves a board's default programmer even for plain "upload".
        self.assertNotIn("avrduusb.programmer.default", board)
        self.assertEqual(board["avrduusb.upload.tool.default"], "euboot")

    def test_generic_images_match_manifest_and_boot_abi(self):
        manifest = json.loads((BOOT / "hex/generic/manifest.json").read_text())
        self.assertEqual(len(manifest["images"]), 8)
        for filename, metadata in manifest["images"].items():
            with self.subTest(filename=filename):
                image = generic.image_bytes(BOOT / "hex/generic" / filename)
                self.assertEqual(hashlib.sha256(image).hexdigest(), metadata["sha256"])
                self.assertEqual(len(image), metadata["bytes"])
                self.assertLessEqual(len(image), generic.BOOT_BYTES)
                self.assertEqual(metadata["app_start"], generic.BOOT_BYTES)
                self.assertEqual(int.from_bytes(image[:2], "little") & 0xF000, 0xC000)
                self.assertEqual(image[2:14], bytes.fromhex("6193089581910895f8950895"))
                self.assertIn(bytes.fromhex("090229000101008032"), image)
                expected_start = 0x6000 if "64" in metadata["mcu"] else 0x7000
                self.assertEqual(metadata["ram_start"], expected_start)
                self.assertEqual(metadata["ramend"], 0x7FFF)
                self.assertLess(metadata["static_ram_bytes"], 4096 - 256)
        for relative, expected_hash in manifest["sources"].items():
            content = (BOOT / relative).read_bytes().replace(b"\r\n", b"\n")
            self.assertEqual(hashlib.sha256(content).hexdigest(), expected_hash, relative)

    def test_nano_board_selects_reproducible_local_bootloader(self):
        folder = BOOT / "hex/nano"
        manifest = json.loads((folder / "manifest.json").read_text())
        self.assertEqual(len(manifest["images"]), 1)
        filename, metadata = next(iter(manifest["images"].items()))
        self.assertEqual(filename, "euboot_avr64du32_LF2_SF6_regulator.hex")
        self.assertEqual(metadata["mcu"], "avr64du32")
        self.assertEqual(metadata["power"], "regulator")
        image = generic.image_bytes(folder / filename)
        self.assertEqual(hashlib.sha256(image).hexdigest(), metadata["sha256"])
        self.assertEqual(len(image), metadata["bytes"])
        self.assertLessEqual(len(image), generic.BOOT_BYTES)
        self.assertEqual(metadata["app_start"], generic.BOOT_BYTES)
        self.assertEqual(image[2:14], bytes.fromhex("6193089581910895f8950895"))
        self.assertIn(bytes.fromhex("090229000101008032"), image)
        self.assertEqual(metadata["ram_start"], 0x6000)
        self.assertEqual(metadata["ramend"], 0x7FFF)
        self.assertLess(metadata["static_ram_bytes"], 8192 - 256)
        for relative, expected_hash in manifest["sources"].items():
            content = (BOOT / relative).read_bytes().replace(b"\r\n", b"\n")
            self.assertEqual(hashlib.sha256(content).hexdigest(), expected_hash, relative)
        board = (PLATFORM / "boards.txt").read_text(encoding="utf-8")
        self.assertIn("avrduusb.bootloader.avrdudestring=-Uflash:w:{runtime.platform.path}/bootloaders/euboot/hex/nano/" + filename + ":i", board)

    def test_generic_board_combinations_select_matching_boot_and_memory(self):
        entries = {}
        prefix = "avrduusb_generic."
        for line in (PLATFORM / "boards.txt").read_text(encoding="utf-8").splitlines():
            if line.startswith(prefix):
                key, value = line[len(prefix):].split("=", 1)
                self.assertNotIn(key, entries)
                entries[key] = value
        self.assertNotIn("programmer.default", entries)
        manifest = json.loads((BOOT / "hex/generic/manifest.json").read_text())["images"]
        for mcu in generic.MCUS:
            for power in ("regulator", "external"):
                with self.subTest(mcu=mcu, power=power):
                    config = board_config("avrduusb_generic", chip=mcu, usbpower=power)
                    capacity = 65536 if "64" in mcu else 32768
                    self.assertEqual(config["build.mcu"], mcu)
                    self.assertEqual(config["build.variant"], mcu[-2:] + "pin-duseries")
                    self.assertEqual(int(config["upload.maximum_size"]), capacity - generic.BOOT_BYTES)
                    self.assertEqual(int(config["upload.maximum_data_size"]), capacity // 8)
                    self.assertEqual(config["build.usbvreg"], "1" if power == "regulator" else "0")
                    self.assertEqual(int(resolved(config, "bootloader.SYSCFG1"), 0), 0x08 if power == "regulator" else 0x00)
                    self.assertEqual(config["bootloader.BOOTSIZE"], "0x06")
                    self.assertEqual(int(resolved(config, "bootloader.SYSCFG0"), 0), 0xD1)
                    self.assertIn("5:{bootloader.SYSCFG0}", config["bootloader.serupdifuse5"])
                    filename = re.sub(r"\{([^}]+)\}", lambda match: config[match[1]], config["bootloader.file"])
                    path = PLATFORM / "bootloaders" / filename
                    self.assertTrue(path.is_file(), filename)
                    self.assertEqual(manifest[path.name]["mcu"], mcu)
                    self.assertEqual(manifest[path.name]["power"], power)

    def test_usb_clock_and_build_options(self):
        original = board_entries("avrdu")
        for board in ("avrduusb", "avrduusb_generic"):
            entries = board_entries(board)
            for menu in ("clock", "millis", "printf", "attach", "wiremode", "flmap"):
                choices = [key.split(".")[2] for key in entries
                           if key.startswith("menu." + menu + ".") and len(key.split(".")) == 3]
                expected = {key.split(".")[2] for key in original
                            if key.startswith("menu." + menu + ".") and len(key.split(".")) == 3}
                self.assertEqual(set(choices), expected)
                for choice in choices:
                    with self.subTest(board=board, menu=menu, choice=choice):
                        config = board_config(board, **{menu: choice})
                        self.assertGreaterEqual(int(resolved(config, "build.f_cpu")[:-1]), 12000000)
                        self.assertIn(config["build.clocksource"], ("0", "1", "2"))
                        if menu == "clock":
                            speed = int(re.match(r"\d+", choice)[0])
                            source = "0" if choice.endswith("internal") else "1" if choice.endswith("crystal") else "2"
                            self.assertEqual(resolved(config, "build.f_cpu"), str(speed * 1000000) + "L")
                            self.assertEqual(config["build.clocksource"], source)
                        self.assertIn("-DDXCORE_USB_CDC", resolved(config, "build.extra_flags"))
                        for key in ("build.flmapopts", "build.flmapabr", "build.printf", "build.attachmode",
                                    "build.millistimer", "build.wiremode"):
                            self.assertNotIn("{", resolved(config, key))
            default = board_config(board)
            self.assertEqual(resolved(default, "build.f_cpu"), "20000000L")
            self.assertEqual(default["build.millistimer"], "B1")
            self.assertEqual(int(resolved(default, "bootloader.BODCFG"), 0), 0)
            self.assertEqual(int(resolved(default, "bootloader.SYSCFG0"), 0), 0xD1)
            self.assertEqual(int(resolved(default, "bootloader.SYSCFG1"), 0), 8)

    def test_usb_fuse_options_preserve_boot_and_power(self):
        modes = {"disabled": 0, "enabled": 5, "ensampfast": 6, "ensampslow": 22,
                 "sampledfast": 10, "sampledslow": 26, "sampdisfast": 8,
                 "sampdisslow": 24, "endisholdwake": 12}
        for board in ("avrduusb", "avrduusb_generic"):
            for level, voltage in enumerate(("1v9", "2v45", "2v70", "2v85")):
                for mode, bits in modes.items():
                    config = board_config(board, bodvoltage=voltage, bodmode=mode)
                    self.assertEqual(int(resolved(config, "bootloader.BODCFG"), 0), level * 32 + bits)
            powers = ("regulator", "external") if board.endswith("generic") else ("regulator",)
            for power in powers:
                for eesave in ("enable", "disable"):
                    for sut, delay in enumerate(("0", "1", "2", "4", "8", "16", "32", "64")):
                        choices = dict(eesave=eesave, startuptimeopti=delay)
                        if board.endswith("generic"):
                            choices["usbpower"] = power
                        config = board_config(board, **choices)
                        self.assertEqual(int(resolved(config, "bootloader.SYSCFG0"), 0), 0xD0 | (eesave == "enable"))
                        self.assertEqual(int(resolved(config, "bootloader.SYSCFG1"), 0), sut | (8 if power == "regulator" else 0))
                        self.assertEqual(int(config["bootloader.BOOTSIZE"], 0) * 512, generic.BOOT_BYTES)
                        self.assertEqual(config["bootloader.CODESIZE"], "0x00")

    def test_usb_watchdog_choices_leave_bootloader_watchdog_unlocked(self):
        periods = ("disabled", "63msec", "125msec", "250msec", "500msec",
                   "1second", "2seconds", "4seconds", "8seconds")
        for board in ("avrduusb", "avrduusb_generic"):
            for timeout_index, timeout in enumerate(periods):
                for window_index, window in enumerate(periods):
                    with self.subTest(board=board, timeout=timeout, window=window):
                        config = board_config(board, usbWDTtimeout=timeout, usbWDTwindow=window)
                        self.assertEqual(int(config["bootloader.WDTCFG"], 0), 0)
                        timeout_bits = timeout_index + 3 if timeout_index else 0
                        window_bits = window_index + 3 if window_index else 0
                        flags = resolved(config, "build.extra_flags").split()
                        self.assertIn("-DDXCORE_USB_WDTCFG=" + format(window_bits * 16 + timeout_bits, "#010b"), flags)


if __name__ == "__main__":
    unittest.main()
