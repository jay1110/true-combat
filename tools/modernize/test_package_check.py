"""Package resource closure only; synthetic DLL carriers are never executed."""
import io
import json
from pathlib import Path
import subprocess
import struct
import sys
import tempfile
import unittest
from unittest.mock import patch
import zipfile

import package_check as check


def archive(files):
    output = io.BytesIO()
    with zipfile.ZipFile(output, "w", zipfile.ZIP_STORED) as z:
        for name, data in sorted(files.items()):
            z.writestr(name, data)
    return output.getvalue()


def fixture(variant):
    version = check.current_version()
    markers = {"cgame_mp_x86.dll": ("cg_omnibotdrawing", "ExportClientFunctionsFromDLL"),
               "ui_mp_x86.dll": ("omnibot_menu", "omnibot_goalproperty"),
               "qagame_mp_x86.dll": ("tce_navgen", "wp_add")}
    runtime = variant == "navstudio"
    if runtime:
        markers["omni-bot/omnibot_et.dll"] = ("waypoint_add", "waypoint_save", "goal_edit")
    files = {"tce2/" + name: name.encode() + b"\0" +
             (b"\0".join(word.encode() for word in words) + b"\0" if runtime else b"")
             for name, words in markers.items()}
    client = {name: path.read_bytes() for name, path in check.client_assets(variant).items()}
    client.update({name: files["tce2/" + name] for name in ("cgame_mp_x86.dll", "ui_mp_x86.dll")})
    client["tce2/" + version + "/client.manifest"] = version.encode()
    files["tce2/tce2-" + version + ".pk3"] = archive(client)
    if runtime:
        folder = check.ROOT / "navstudio/editor-ui"
        files["tce2/zz_tce2_navstudio_editor.pk3"] = archive({"ui/" + name: (folder / name).read_bytes()
            for name in (folder / "editor-files.txt").read_text().splitlines()})
    manifest = {"variant": variant, "architecture": "x86", "version": version,
                "configuration": "Release", "omnibot_runtime": runtime,
                "editor_code_checks": {name: {word: runtime for word in words} for name, words in markers.items()}}
    files["README.txt"] = b"Synthetic resource closure test; no executable binaries."
    refresh_manifest(files, manifest)
    return files, manifest


def refresh_manifest(files, manifest):
    manifest["files"] = {name: {"bytes": len(data), "sha256": check.sha(data)}
                         for name, data in files.items() if name.startswith("tce2/")}
    files["manifest.json"] = json.dumps(manifest).encode()


def fake_pe(data):
    exports = ["dllEntry", "vmMain"]
    if data.startswith(b"cgame") and b"ExportClientFunctionsFromDLL\0" in data:
        exports.append("ExportClientFunctionsFromDLL")
    return {"machine": "0x14c", "optional_magic": "0x10b", "exports": exports}


class PackageResources(unittest.TestCase):
    def assert_native_system_menu(self, data):
        menu = data.decode('utf-8')
        for control in ('s_volume', 'ui_s_khz', 'ui_r_mode', 'ui_r_fullscreen',
                        'ui_r_picmip', 'ui_r_texturemode', 'ui_r_dynamiclight',
                        'ui_rate', 'systemCvarsApply'):
            self.assertIn(control, menu)
        compact = ''.join(menu.upper().split())
        for retained in ('"SETRECOMMENDED"', 'OPENREC_RESTART'):
            self.assertIn(retained, compact)
        for removed in ('ENHANCEDGRAPHICS', 'OPTIONS_QUALITY', 'TCEGRAPHICS', 'TCE_GRAPHICS'):
            self.assertNotIn(removed, compact)

    def test_original_fireteam_artwork_and_shared_system_menus_are_required(self):
        required = {'gfx/tce2/fireteam/' + name for name in
                    ('assault.tga', 'recon.tga', 'sniper.tga', 'SOURCE.json')}
        required |= {'ui/options_system.menu'}
        for variant in ('game', 'navstudio'):
            self.assertTrue(required <= check.client_assets(variant).keys())
            self.assertNotIn('ui/options_quality.menu', check.client_assets(variant))
            self.assert_native_system_menu(check.client_assets(variant)['ui/options_system.menu'].read_bytes())
        folder = check.ROOT / 'src/assets/gfx/tce2/fireteam'
        source = json.loads((folder/'SOURCE.json').read_text())
        self.assertEqual({row['asset'] for row in source['files']},
                         {'assault.tga', 'recon.tga', 'sniper.tga'})
        for row in source['files']:
            data = (folder/row['asset']).read_bytes()
            self.assertEqual((data[2], struct.unpack_from('<HH', data, 12), data[16], data[17]),
                             (2, (64, 64), 32, 0x28))
            self.assertEqual(check.sha(data), row['sha256'])
        subprocess.run([sys.executable, str(check.ROOT/'tools/assets/generate_fireteam_icons.py'), '--check'],
                       check=True, capture_output=True, text=True)

    def test_original_nav_artwork_is_reproducible_rgba_with_transparency(self):
        folder = check.ROOT / "src/assets/gfx/tce2/nav"
        provenance = json.loads((folder / "SOURCE.json").read_text())
        self.assertEqual({r["asset"] for r in provenance["files"]}, set(check.NAV_ICONS) - {"SOURCE.json"})
        for row in provenance["files"]:
            data = (folder / row["asset"]).read_bytes()
            size = 128 if row["asset"].startswith("objective_") else 64
            self.assertEqual((data[2], struct.unpack_from("<HH", data, 12), data[16], data[17]), (2, (size, size), 32, 0x28))
            self.assertEqual((len(data), check.sha(data)), (row["bytes"], row["sha256"]))
            alpha = data[21:18 + size * size * 4:4]
            self.assertEqual(alpha[0], 0)
            self.assertEqual(alpha[size - 1], 0)
            self.assertIn(255, alpha)
            self.assertTrue(any(0 < a < 255 for a in alpha))
        subprocess.run([sys.executable, str(check.ROOT / "tools/assets/generate_nav_icons.py"), "--check"],
                       check=True, capture_output=True, text=True)

    def test_real_cmake_game_and_editor_asset_sets_match_validator(self):
        # Run the real packager with tiny non-executable module carriers. This
        # catches future divergence instead of copying its list into the test.
        with tempfile.TemporaryDirectory() as temporary:
            for variant in ("game", "navstudio"):
                with self.subTest(variant=variant):
                    folder = Path(temporary) / variant / "modules"
                    folder.mkdir(parents=True)
                    for role in ("cgame", "ui"):
                        (folder / (role + "_mp_x86.dll")).write_bytes(b"non-executable carrier")
                    subprocess.run(["cmake", "-DMODULE_DIRECTORY=" + str(folder), "-DBUILD_VARIANT=" + variant,
                                    "-P", str(check.ROOT / "src/cmake/package_modules.cmake")],
                                   check=True, capture_output=True, text=True)
                    files = check.archive_files((folder / ("tce2-" + check.current_version() + ".pk3")).read_bytes())
                    expected = set(check.client_assets(variant)) | {"cgame_mp_x86.dll", "ui_mp_x86.dll",
                        "tce2/" + check.current_version() + "/client.manifest"}
                    self.assertEqual(set(files), expected)
                    self.assertIn('ui/options_system.menu', files)
                    self.assertNotIn('ui/options_quality.menu', files)
                    self.assert_native_system_menu(files['ui/options_system.menu'])
                    for name, source in check.client_assets(variant).items():
                        self.assertEqual(files[name], source.read_bytes(), name)

    @patch.object(check, "pe_info", side_effect=fake_pe)
    def test_valid_current_resources_accept_both_variants(self, unused):
        for variant in ("game", "navstudio"):
            files, _ = fixture(variant)
            result = check.check_contents(files, "synthetic", check.current_version())
            self.assertTrue(result["passed"], result["errors"])

    @patch.object(check, "pe_info", side_effect=fake_pe)
    def test_missing_or_changed_assets_fail_even_with_honest_outer_inventory(self, unused):
        for variant in ("game", "navstudio"):
            files, manifest = fixture(variant)
            name = "tce2/tce2-" + check.current_version() + ".pk3"
            original = files[name]
            for resource in check.client_assets(variant):
                for missing in (False, True):
                    with self.subTest(variant=variant, resource=resource, missing=missing):
                        client = check.archive_files(original)
                        if missing:
                            del client[resource]
                        else:
                            client[resource] = b"changed current asset or provenance"
                        files[name] = archive(client)
                        refresh_manifest(files, manifest)
                        result = check.check_contents(files, "synthetic", check.current_version())
                        self.assertFalse(result["passed"])
                        self.assertTrue(any(resource in error for error in result["errors"]), result["errors"])

    @patch.object(check, "pe_info", side_effect=fake_pe)
    def test_removed_quality_menu_is_rejected_even_with_honest_outer_inventory(self, unused):
        for variant in ("game", "navstudio"):
            files, manifest = fixture(variant)
            name = "tce2/tce2-" + check.current_version() + ".pk3"
            client = check.archive_files(files[name])
            client["ui/options_quality.menu"] = b"removed graphics quality menu"
            files[name] = archive(client)
            refresh_manifest(files, manifest)
            result = check.check_contents(files, "synthetic", check.current_version())
            self.assertFalse(result["passed"])
            self.assertTrue(any("unexpected=['ui/options_quality.menu']" in error
                                for error in result["errors"]), result["errors"])

    @patch.object(check, "pe_info", side_effect=fake_pe)
    def test_unexpected_editor_menu_is_still_rejected_from_game_client(self, unused):
        files, manifest = fixture("game")
        name = "tce2/tce2-" + check.current_version() + ".pk3"
        client = check.archive_files(files[name]); client["ui/tce_waypoint_editor.menu"] = b"editor"
        files[name] = archive(client); refresh_manifest(files, manifest)
        result = check.check_contents(files, "synthetic", check.current_version())
        self.assertFalse(result["passed"])
        self.assertTrue(any("unexpected=['ui/tce_waypoint_editor.menu']" in error for error in result["errors"]))


if __name__ == "__main__":
    unittest.main()
