#!/usr/bin/env python3
"""Fetch the pinned upstream SDK and apply the local patch without overwriting edits."""
import hashlib
import json
from pathlib import Path
import subprocess

ROOT = Path(__file__).resolve().parents[1]
SDK = ROOT / 'bdk_rtt'
SPEC = json.loads((ROOT / 'patches/sdk.json').read_text())
PATCH = ROOT / 'patches/opencam14-sdk.patch'

def git(*args, check=True):
    return subprocess.run(['git', '-C', str(SDK), *args], check=check,
                          stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)

def prepare():
    if hashlib.sha256(PATCH.read_bytes()).hexdigest() != SPEC['patch_sha256']:
        raise RuntimeError('SDK patch checksum mismatch')
    if not (SDK / '.git').exists():
        subprocess.run(['git', '-C', str(ROOT), 'submodule', 'update', '--init', '--', 'bdk_rtt'], check=True)
    if git('rev-parse', 'HEAD').stdout.strip() != SPEC['base_commit']:
        raise RuntimeError('Unexpected SDK HEAD. Expected ' + SPEC['base_commit'] + '; preserve your edits before changing it.')
    tracked_changes = git('diff', '--name-only', 'HEAD').stdout.splitlines()
    untracked = git('ls-files', '--others', '--exclude-standard').stdout.splitlines()
    changed = set(tracked_changes + untracked)
    expected = set(SPEC['source_sha256'])
    if not changed:
        git('apply', '--check', str(PATCH))
        git('apply', str(PATCH))
        print('Applied OpenCam14 SDK patch.')
    else:
        if changed != expected:
            raise RuntimeError('SDK contains unexpected or incomplete changes; nothing was overwritten.')
        for name, digest in SPEC['source_sha256'].items():
            file = SDK / name
            if not file.is_file() or hashlib.sha256(file.read_bytes()).hexdigest() != digest:
                raise RuntimeError('SDK file differs from the release patch: ' + name + '; nothing was overwritten.')
        git('apply', '--reverse', '--check', str(PATCH))
        print('OpenCam14 SDK patch is already applied.')
    for name, digest in SPEC['source_sha256'].items():
        if hashlib.sha256((SDK / name).read_bytes()).hexdigest() != digest:
            raise RuntimeError('Patched SDK checksum mismatch: ' + name)
    print('Verified SDK base and all patched source files.')

if __name__ == '__main__':
    try:
        prepare()
    except (RuntimeError, subprocess.CalledProcessError) as exc:
        detail = getattr(exc, 'stderr', '')
        raise SystemExit(str(exc) + ('\n' + detail if detail else ''))
