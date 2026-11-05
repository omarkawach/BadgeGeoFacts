"""Regression tests for the local publication guard; uses synthetic data only."""
import hashlib
import importlib.util
import json
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location('publication', ROOT / 'tools/check_publication.py')
publication = importlib.util.module_from_spec(spec)
spec.loader.exec_module(publication)

class PublicationChecks(unittest.TestCase):
    def release(self, nvs=False, trailing=False):
        app = bytes([0xe9]) + bytes(127)
        fw = bytearray(b'\xff' * 0x400000)
        fw[0] = 0xe9
        fw[0x10000:0x10000+len(app)] = app
        if nvs:
            fw[0x9000] = 0x12
        if trailing:
            fw[-1] = 0x12
        fw = bytes(fw)
        hashes = {name: hashlib.sha256(data).hexdigest() for name, data in [('firmware.bin', fw), ('application.bin', app)]}
        return {
            'firmware.bin': fw, 'application.bin': app,
            'release.json': json.dumps(dict(firmware_sha256=hashes['firmware.bin'], application_sha256=hashes['application.bin'])).encode(),
            'SHA256SUMS': ''.join(f'{digest}  {name}\n' for name, digest in hashes.items()).encode(),
        }

    def test_custom_partition_table_is_publishable(self):
        self.assertIsNone(publication.forbidden_path('partitions.csv'))

    def test_factory_release_is_accepted(self):
        self.assertEqual(publication.audit(self.release()), [])

    def test_device_nvs_rejected_even_with_matching_hash(self):
        result = publication.audit(self.release(nvs=True))
        self.assertTrue(any('NVS/PHY' in x for x in result))

    def test_trailing_device_data_rejected(self):
        self.assertTrue(any('trailing data' in x for x in publication.audit(self.release(trailing=True))))

    def test_force_added_private_path_rejected(self):
        files = {'SHA256SUMS': b'', 'private/backups/original-badge.bin': b'synthetic fixture'}
        self.assertTrue(any('private' in x for x in publication.audit(files)))

    def test_unapproved_binary_and_environment_paths_rejected(self):
        for name in ['backup.bin', 'device-dump.bin', '.env', 'main/.env.local', 'wifi.json', 'work/config.json', 'unknown.txt', 'previews/personal-photo.png']:
            self.assertIsNotNone(publication.forbidden_path(name), name)

    def test_credential_report_does_not_print_value(self):
        secret = b'synthetic-fixture-not-a-password'
        data = b'WIFI_' + b'PASSWORD = "' + secret + b'"\n'
        result = publication.audit({'SHA256SUMS': b'', 'main/example.c': data})
        self.assertTrue(any('hardcoded Wi-Fi' in x for x in result))
        self.assertNotIn(secret.decode(), '\n'.join(result))

    def test_json_secret_and_personal_path_rejected(self):
        data = json.dumps(dict(password='synthetic')).encode()
        self.assertTrue(any('JSON credential' in x for x in publication.audit({'SHA256SUMS': b'', 'tests/example.json': data})))
        path = b'/Us' + b'ers/' + b'someone/project'
        self.assertTrue(any('filesystem path' in x for x in publication.audit({'SHA256SUMS': b'', 'README.md': path})))

    def test_real_key_body_rejected_but_parser_marker_allowed(self):
        marker = b'-----BEGIN ' + b'PRIVATE KEY-----'
        harmless = publication.audit({'SHA256SUMS': b'', 'main/parser.c': marker})
        self.assertEqual(harmless, [])
        material = marker + bytes([10]) + b'A' * 64 + bytes([10]) + b'B' * 64 + bytes([10])
        failures = publication.audit({'SHA256SUMS': b'', 'main/example.c': material})
        self.assertTrue(any('private key' in x for x in failures))

    def test_runtime_credential_code_is_allowed(self):
        data = b'char ssid[33]={0}; nvs_get_str(store, "password", pass, &size);\n'
        self.assertEqual(publication.audit({'SHA256SUMS': b'', 'main/example.c': data}), [])

if __name__ == '__main__':
    unittest.main()
