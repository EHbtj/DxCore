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


class USBAssetTests(unittest.TestCase):
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
        self.assertEqual(start, int(board["avrduusb.bootloader.BOOTSIZE"], 0) * 512)
        self.assertEqual(start + int(board["avrduusb.upload.maximum_size"]), 65536)
        self.assertEqual(board["avrduusb.bootloader.SYSCFG0"], "0xD1")
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
                self.assertLessEqual(len(image), 2560)
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
        self.assertLessEqual(len(image), 2560)
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
                    config = {key: value for key, value in entries.items() if not key.startswith("menu.")}
                    for menu_prefix in ("menu.chip." + mcu + ".", "menu.usbpower." + power + "."):
                        config.update((key[len(menu_prefix):], value) for key, value in entries.items() if key.startswith(menu_prefix))
                    capacity = 65536 if "64" in mcu else 32768
                    self.assertEqual(config["build.mcu"], mcu)
                    self.assertEqual(config["build.variant"], mcu[-2:] + "pin-duseries")
                    self.assertEqual(int(config["upload.maximum_size"]), capacity - 2560)
                    self.assertEqual(int(config["upload.maximum_data_size"]), capacity // 8)
                    self.assertEqual(config["build.usbvreg"], "1" if power == "regulator" else "0")
                    self.assertEqual(config["bootloader.SYSCFG1"], "0x08" if power == "regulator" else "0x00")
                    self.assertEqual(config["bootloader.BOOTSIZE"], "0x05")
                    self.assertEqual(config["bootloader.SYSCFG0"], "0xD1")
                    self.assertIn("5:{bootloader.SYSCFG0}", config["bootloader.serupdifuse5"])
                    filename = re.sub(r"\{([^}]+)\}", lambda match: config[match[1]], config["bootloader.file"])
                    path = PLATFORM / "bootloaders" / filename
                    self.assertTrue(path.is_file(), filename)
                    self.assertEqual(manifest[path.name]["mcu"], mcu)
                    self.assertEqual(manifest[path.name]["power"], power)


if __name__ == "__main__":
    unittest.main()
