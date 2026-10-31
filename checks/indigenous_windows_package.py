#!/usr/bin/env python3
"""Check Windows runtime notice preservation with isolated package fixtures."""
import hashlib
import contextlib
import io
import json
from pathlib import Path
import sys
import tempfile
import unittest
from unittest.mock import patch
import zipfile

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "gadgets"))
from package_windows import stage_windows_notices, stage_windows_runtime
from package_indigenous import main as package_main


class WindowsPackageNotices(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory()
        self.addCleanup(self.temporary.cleanup)
        self.base = Path(self.temporary.name)
        self.runtime = self.base / "msys/ucrt64"
        self.stage = self.base / "package"
        self.runtime.mkdir(parents=True)

    def put(self, relative, content=b"Fixture notice\n"):
        path = self.runtime / relative
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_bytes(content)
        return path

    def test_preserves_records_and_hashes_without_unrelated_docs(self):
        sources = {
            "prefix/share/licenses/library/LICENSE": self.put("share/licenses/library/LICENSE"),
            "prefix/share/licenses/library/README.extra": self.put("share/licenses/library/README.extra"),
            "prefix/share/doc/libuv/LICENSE-extra": self.put("share/doc/libuv/LICENSE-extra"),
            "prefix/share/icu/78.3/LICENSE": self.put("share/icu/78.3/LICENSE"),
            "msys/share/licenses/common/LGPL2.1": self.put("../usr/share/licenses/common/LGPL2.1"),
        }
        self.put("share/doc/library/unrelated.html", b"Not a notice")
        stage_windows_notices(self.stage, self.runtime)
        target = self.stage / "licenses/windows-runtime"
        inventory = json.loads((target / "INVENTORY.json").read_text())
        self.assertEqual(set(sources), {item["file"] for item in inventory["records"]})
        for item in inventory["records"]:
            data = sources[item["file"]].read_bytes()
            self.assertEqual(data, (target / item["file"]).read_bytes())
            self.assertEqual(hashlib.sha256(data).hexdigest(), item["sha256"])
            self.assertEqual(len(data), item["bytes"])

    def test_missing_or_empty_notice_tree_is_rejected(self):
        with self.assertRaisesRegex(ValueError, "requires installed share/licenses"):
            stage_windows_notices(self.stage, self.runtime)
        (self.runtime / "share/licenses").mkdir(parents=True)
        with self.assertRaisesRegex(ValueError, "contains no notice records"):
            stage_windows_notices(self.stage, self.runtime)
        self.put("share/licenses/library/LICENSE", b"")
        with self.assertRaisesRegex(ValueError, "Empty Windows runtime notice"):
            stage_windows_notices(self.stage, self.runtime)

    def test_referenced_common_text_must_exist(self):
        self.put("share/licenses/library/LICENSE", b"See /usr/share/licenses/common/LGPL2.1\n")
        with self.assertRaisesRegex(ValueError, "Missing referenced Windows common license"):
            stage_windows_notices(self.stage, self.runtime)
        self.put("../usr/share/licenses/common/LGPL2.1")
        stage_windows_notices(self.stage, self.runtime)
        self.assertTrue((self.stage / "licenses/windows-runtime/msys/share/licenses/common/LGPL2.1").is_file())

    def test_notice_symlink_cannot_escape_record_tree(self):
        target = self.put("private-record")
        link = self.runtime / "share/licenses/library/LICENSE"
        link.parent.mkdir(parents=True)
        try:
            link.symlink_to(target)
        except OSError:
            self.skipTest("Host cannot create symlinks")
        with self.assertRaisesRegex(ValueError, "Invalid Windows runtime notice"):
            stage_windows_notices(self.stage, self.runtime)

    def test_runtime_closure_includes_notices(self):
        binary = self.base / "batbox.exe"
        binary.write_bytes(b"Fixture executable")
        self.put("bin/fixture.dll", b"Fixture runtime")
        self.put("share/licenses/library/LICENSE")
        self.put("etc/ssl/certs/ca-bundle.crt", b"-----BEGIN CERTIFICATE-----\nfixture\n")
        imports = {"batbox.exe": ["fixture.dll"], "fixture.dll": ["kernel32.dll"]}
        with patch("package_windows._objdump_imports", side_effect=lambda path, _: imports[path.name]):
            files = stage_windows_runtime(self.stage, binary, self.runtime, "fixture-objdump")
        self.assertEqual([item["file"] for item in files], ["bin/fixture.dll"])
        self.assertTrue((self.stage / "licenses/windows-runtime/INVENTORY.json").is_file())
        self.assertEqual((self.stage / "bin/fixture.dll").read_bytes(), b"Fixture runtime")

    def test_windows_zip_contains_runtime_notice_inventory(self):
        binary = self.base / "batbox.exe"
        header = bytearray(70)
        header[:2] = b"MZ"
        header[0x3c:0x40] = (64).to_bytes(4, "little")
        header[64:70] = b"PE\0\0\x64\x86"
        binary.write_bytes(header)
        self.put("bin/fixture.dll", b"Fixture runtime")
        self.put("share/licenses/library/LICENSE")
        self.put("etc/ssl/certs/ca-bundle.crt", b"-----BEGIN CERTIFICATE-----\nfixture\n")
        source = self.base / "source"
        for relative in ("LICENSE", "THIRD_PARTY_NOTICES.md", "third_party/ftxui/LICENSE",
                         "docs/HALLOWEEN-PREVIEW.md", "docs/BUILDING.md",
                         "gadgets/install-windows.ps1"):
            path = source / relative
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_text("Fixture project record\n")
        output = self.base / "batbox.zip"
        arguments = ["package_indigenous.py", "--binary", str(binary), "--source", str(source),
                     "--runtime-root", str(self.runtime), "--output", str(output),
                     "--objdump", "fixture-objdump"]
        imports = {"batbox.exe": ["fixture.dll"], "fixture.dll": ["kernel32.dll"]}
        with patch.object(sys, "argv", arguments), contextlib.redirect_stdout(io.StringIO()), \
                patch("package_windows._objdump_imports", side_effect=lambda path, _: imports[path.name]):
            package_main()
        with zipfile.ZipFile(output) as archive:
            root = "batbox-windows-x86_64-halloween-preview/"
            inventory = json.loads(archive.read(root + "licenses/windows-runtime/INVENTORY.json"))
            self.assertEqual(len(inventory["records"]), 1)
            self.assertEqual(archive.read(root + "licenses/windows-runtime/" + inventory["records"][0]["file"]),
                             b"Fixture notice\n")
            self.assertEqual(archive.read(root + "LICENSE"), b"Fixture project record\n")
            self.assertIn(root + "gadgets/install-windows.ps1", archive.namelist())


if __name__ == "__main__":
    unittest.main()
