"""Check the distributed bootloader and the board's memory/fuse contract."""
import hashlib
from pathlib import Path
import unittest

PLATFORM = Path(__file__).resolve().parents[2]


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


if __name__ == "__main__":
    unittest.main()
