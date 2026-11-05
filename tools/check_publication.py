#!/usr/bin/env python3
"""Audit the entire Git index before commit; never prints matched secret values.
Use --tree to inspect publishable working-tree files before staging.
This is a local guard, not a replacement for reviewing a future push.
"""
from pathlib import Path, PurePosixPath
import argparse, hashlib, json, re, subprocess, sys

ROOT = Path(__file__).resolve().parents[1]
ROOT_FILES = {
    '.gitignore', '.gitattributes', 'CMakeLists.txt', 'LICENSE', 'MPL-2.0.txt',
    'README.md', 'NOTICE.md', 'PUBLICATION.md', 'VALIDATION.md', 'CHANGELOG.md',
    'SHA256SUMS', 'release.json', 'sdkconfig', 'sdkconfig.defaults',
    'requirements-dev.txt', 'partitions.csv', 'firmware.bin', 'application.bin',
}
REVIEWED_BINARY_ASSETS = {'firmware.bin', 'application.bin', 'assets/fonts/IBMPlexSans.ttf', 'previews/richat.png', 'previews/uyuni.png', 'previews/world-equal-earth.png'}
PUBLIC_DIRS = {'main', 'tools', 'tests', 'assets', 'previews', 'LICENSES', '.githooks'}
PRIVATE_PARTS = {'private', 'backups', 'backup', 'secrets', 'credentials', 'work', 'build', '.venv', 'venv', '__pycache__'}
BAD_SUFFIXES = {'.pem', '.key', '.p12', '.pfx', '.jks', '.nvs', '.dump', '.epaper', '.raw', '.log', '.elf', '.o', '.a', '.so', '.dylib', '.zip'}
PATTERNS = [
    ('private key', rb'-----BEGIN (?:[A-Z0-9 ]+ )?PRIVATE KEY-----(?:\r?\n|\\n)(?:[A-Za-z0-9+/=]{16,}(?:\r?\n|\\n)){2,}'),
    ('AWS access key', rb'\b(?:AKIA|ASIA)[A-Z0-9]{16}\b'),
    ('GitHub token', rb'\b(?:gh[pousr]_[A-Za-z0-9]{20,}|github_pat_[A-Za-z0-9_]{30,})\b'),
    ('API secret', rb'\bsk-(?:proj-|svcacct-)?[A-Za-z0-9_-]{32,}\b'),
    ('Slack token', rb'\bxox[baprs]-[A-Za-z0-9-]{15,}\b'),
    ('personal filesystem path', rb'/(?:Users|home)/[A-Za-z0-9_.-]+/'),
    ('literal JSON credential', rb'"(?:password|wifi_password|wifi_ssid|ssid|api_key|access_token|refresh_token)"\s*:\s*"[^"\r\n]+"'),
    ('hardcoded Wi-Fi credential', rb'(?im)^\s*(?:(?:const|static|char|String|str)\s+)*(?:WIFI_SSID|WIFI_PASSWORD|WIFI_PASS|wifi_ssid|wifi_password)\s*(?:\[[^\]]*\])?\s*=\s*[\x22\x27][^\x22\x27\r\n]+'),
]
RULES = [(name, re.compile(pattern)) for name, pattern in PATTERNS]

def git(*args, env=None):
    return subprocess.run(['git', '-C', str(ROOT), *args], check=True, capture_output=True, env=env).stdout

def forbidden_path(name):
    p = PurePosixPath(name)
    lower = [part.lower() for part in p.parts]
    if p.is_absolute() or '..' in p.parts or any(part in PRIVATE_PARTS or part.startswith('build-') for part in lower):
        return 'private/build directory'
    if any(part.startswith('.env') for part in lower):
        return 'environment file'
    if re.search(r'(?i)(?:^|[/_.-])(?:backup|original-badge|flash-dump|credentials|secrets|nvs)(?:$|[/_.-])', name):
        return 'backup or credential filename'
    if p.suffix.lower() in BAD_SUFFIXES:
        return 'private or generated file type'
    if p.suffix.lower() in {'.png', '.jpg', '.jpeg', '.heic', '.ttf', '.woff', '.woff2'} and name not in REVIEWED_BINARY_ASSETS:
        return 'unreviewed binary asset; review and explicitly allow it'
    if p.suffix.lower() == '.bin' and name not in {'firmware.bin', 'application.bin'}:
        return 'unapproved binary image'
    if len(p.parts) == 1 and name not in ROOT_FILES:
        return 'unexpected root file; review and explicitly allow it'
    if len(p.parts) > 1 and p.parts[0] not in PUBLIC_DIRS:
        return 'unexpected directory; review and explicitly allow it'
    return None

def index_files():
    blobs = {}
    for entry in git('ls-files', '--stage', '-z').split(b'\0'):
        if not entry:
            continue
        metadata, path = entry.split(b'\t', 1)
        mode, oid, stage = metadata.split()
        name = path.decode('utf-8')
        if mode == b'120000':
            raise ValueError('symlink is not publishable: ' + name)
        if stage != b'0':
            raise ValueError('unmerged index entry: ' + name)
        blobs[name] = git('cat-file', 'blob', oid.decode())
    if not blobs:
        raise ValueError('Git index is empty; stage reviewed public files first, or use --tree')
    return blobs

def tree_files():
    blobs = {}
    for path in ROOT.rglob('*'):
        rel = path.relative_to(ROOT).as_posix()
        if '.git' in path.relative_to(ROOT).parts or not path.is_file():
            continue
        # --tree audits the publication allowlist, not ignored local backups.
        if forbidden_path(rel):
            continue
        if path.is_symlink():
            raise ValueError('symlink is not publishable: ' + rel)
        blobs[rel] = path.read_bytes()
    return blobs

def audit(blobs):
    failures = []
    for name, data in blobs.items():
        reason = forbidden_path(name)
        if reason:
            failures.append(name + ': ' + reason)
            continue
        for label, pattern in RULES:
            match = pattern.search(data)
            if match:
                line = data[:match.start()].count(b'\n') + 1
                failures.append(f'{name}:{line}: {label} (value withheld)')
    binaries = {name for name in ('firmware.bin', 'application.bin') if name in blobs}
    if binaries:
        if len(binaries) != 2:
            failures.append('release images: firmware.bin and application.bin must be reviewed together')
        else:
            fw, app = blobs['firmware.bin'], blobs['application.bin']
            if len(fw) != 0x400000 or not app or app[0] != 0xe9 or len(app) > 0x300000:
                failures.append('release images: unexpected image size/header')
            else:
                if fw[0x9000:0x10000] != b'\xff' * 0x7000:
                    failures.append('firmware.bin: NVS/PHY settings region is not blank; possible device dump')
                if fw[0x10000:0x10000+len(app)] != app:
                    failures.append('firmware.bin: application does not match application.bin')
                if fw[0x10000+len(app):] != b'\xff' * (len(fw)-0x10000-len(app)):
                    failures.append('firmware.bin: trailing data region is not blank; possible saved device data')
            try:
                release = json.loads(blobs['release.json'])
                for name, key in [('firmware.bin', 'firmware_sha256'), ('application.bin', 'application_sha256')]:
                    if hashlib.sha256(blobs[name]).hexdigest() != release[key]:
                        failures.append(name + ': release.json hash mismatch')
            except (KeyError, ValueError):
                failures.append('release.json: missing/invalid release hash metadata')
    try:
        for line in blobs['SHA256SUMS'].decode().splitlines():
            digest, name = line.split('  ', 1)
            if name not in blobs or hashlib.sha256(blobs[name]).hexdigest() != digest:
                failures.append(name + ': SHA256SUMS mismatch or missing public file')
    except (KeyError, ValueError, UnicodeError):
        failures.append('SHA256SUMS: missing or invalid manifest')
    return failures

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--tree', action='store_true', help='audit public working-tree files instead of the Git index')
    args = parser.parse_args()
    try:
        blobs = tree_files() if args.tree else index_files()
        failures = audit(blobs)
    except (ValueError, OSError, subprocess.CalledProcessError) as exc:
        print('Publication check could not complete:', str(exc), file=sys.stderr)
        return 1
    if failures:
        print('Publication check FAILED:', file=sys.stderr)
        for failure in failures:
            print(' - ' + failure, file=sys.stderr)
        return 1
    print(f'Publication check passed: {len(blobs)} public files; release settings/data regions blank; no detected credentials.')
    return 0

if __name__ == '__main__':
    raise SystemExit(main())
