#!/usr/bin/env python3
"""Prepare/verify a LOCAL signed HMI CHECK-bench bundle. Never publish or install.

The owner supplies a reviewed local plan and pinned public key; a downloaded
bundle cannot select trust, versions or compatibility policy. Signing uses the
existing OpenSSL ECDSA path and is intended for an owned quiescent workstation.
"""
import argparse
import base64
import hashlib
import json
import os
from pathlib import Path
import re
import stat
import subprocess
import tempfile

from build_candidate import encoded, git, inventory, regular_read, source
import check_idf_build
from esp_app_image import APP_MAX, inspect_app, require
import verify_ota_candidate as ota

PLAN_MAX = 16384
ELF_MAX = 32 * 1024 * 1024
SDKCONFIG_MAX = 256 * 1024
FIELDS = {'reviewed', 'purpose', 'source_commit', 'sdk', 'release_version',
          'installed_version', 'bootloader_version', 'protocol_version',
          'channel', 'key_id', 'public_key_sha256', 'release_notes'}
NOTICE = (b'LOCAL SIGNED CHECK BENCH ONLY. Not publication or installation approval.\n'
          b'Only the app image is included; no bootloader, partition table, NVS or keys.\n'
          b'Resource, installer, rollback, power-loss and interactive performance acceptance remain open.\n'
          b'Use a separately reviewed local plan and trusted public key to verify.\n')
META_FILES = {'manifest.json', 'manifest.sig', 'build-evidence.json',
              'release-notes.md', 'README.txt', 'LICENSE.txt', 'LICENSE-NOTICE.md'}
EVIDENCE_FIELDS = {'schema', 'purpose', 'source_commit', 'source_tree', 'source_display_version',
    'sdk', 'profile', 'application', 'public_key_sha256', 'candidate_provisioned',
    'source_checked_locally', 'device_access', 'install_enabled', 'release_published',
    'ci_identity_verified', 'production_resource_qualified', 'sdkconfig_sha256',
    'partitions_sha256', 'dependency_lock_sha256'}


def plan_read(path):
    p = ota.parse(regular_read(path, PLAN_MAX))
    require(isinstance(p, dict) and set(p) == FIELDS and p['reviewed'] is True,
            'explicit reviewed local release plan required')
    require(p['purpose'] == 'signed-check-bench' and p['channel'] == 'prerelease',
            'only explicit prerelease CHECK-bench scope is supported')
    require(p['sdk'] in ('5.3.2', '6.1.1'), 'unsupported SDK')
    require(isinstance(p['source_commit'], str) and
            re.fullmatch('[0-9a-f]{40}', p['source_commit']) and p['source_commit'] != '0'*40,
            'explicit nonzero full source SHA required')
    for name in ('release_version', 'installed_version', 'bootloader_version', 'protocol_version'):
        ota.version(p[name])
        require(len(p[name]) <= 31, 'version exceeds target bound')
    require('-' in p['release_version'] and
            ota.version(p['installed_version']) < ota.version(p['release_version']),
            'prerelease must advance the local installed version')
    require(isinstance(p['key_id'], str) and re.fullmatch(r'[A-Za-z0-9_.-]{1,64}', p['key_id']),
            'invalid owner key ID')
    require(isinstance(p['public_key_sha256'], str) and
            re.fullmatch('[0-9a-f]{64}', p['public_key_sha256']), 'explicit public DER SHA-256 required')
    require(isinstance(p['release_notes'], str) and
            0 < len(p['release_notes'].encode('utf-8')) <= 4096, 'bounded reviewed release notes required')
    return p


def public_read(path, plan):
    raw = regular_read(path, 91)
    require(len(raw) == 91 and raw.startswith(ota.P256_PREFIX) and
            hashlib.sha256(raw).hexdigest() == plan['public_key_sha256'],
            'local canonical P-256 public key pin mismatch')
    return raw


def request(plan, root, public):
    return argparse.Namespace(manifest=root/'manifest.json', signature=root/'manifest.sig',
        public_key=public, trusted_key_id=plan['key_id'], installed_version=plan['installed_version'],
        bootloader_version=plan['bootloader_version'], protocol_version=plan['protocol_version'],
        layout_id='tv4-16m-ab-v1', channel=plan['channel'], motion_version=None,
        image=root/('tv4-hmi-esp32s3-v'+plan['release_version']+'.bin'))


def verify(directory, plan_path, public_key):
    plan = plan_read(plan_path)
    public_read(public_key, plan)
    root = Path(directory)
    asset = 'tv4-hmi-esp32s3-v'+plan['release_version']+'.bin'
    require(not root.is_symlink() and root.is_dir(), 'expected bundle directory')
    require({p.name for p in root.iterdir()} == META_FILES | {asset, 'checksums.sha256'},
            'unexpected/missing bundle files; no keys or Flash dumps accepted')
    files = {name: regular_read(root/name, APP_MAX if name == asset else
             ota.MANIFEST_MAX if name == 'manifest.json' else PLAN_MAX) for name in META_FILES | {asset}}
    require(regular_read(root/'checksums.sha256', 4096) == inventory(files), 'bundle inventory mismatch')
    require(files['README.txt'] == NOTICE and
            files['release-notes.md'] == (plan['release_notes']+'\n').encode('utf-8'), 'scope/notes mismatch')
    require(b'Permission is hereby granted' in files['LICENSE.txt'] and
            b'TV4-CNC contributors' in files['LICENSE.txt'] and
            b'MIT license' in files['LICENSE-NOTICE.md'], 'application license/attribution missing')
    # Immutable captured bytes are checked; the local plan/key remain external.
    with tempfile.TemporaryDirectory(prefix='tv4-check-verify-') as tmp:
        captured = Path(tmp)
        for name, data in files.items(): (captured/name).write_bytes(data)
        pub = captured/'trusted.der'; pub.write_bytes(public_read(public_key, plan))
        signed, report = ota.verify_with_metadata(request(plan, captured, pub))
    app = inspect_app(files[asset], plan['sdk'])
    require((signed['source_commit'], signed['release'], signed['version'], app['version']) ==
            (plan['source_commit'], plan['release_version'], plan['release_version'], plan['release_version']),
            'signed source/version/actual app descriptor mismatch')
    evidence = ota.parse(files['build-evidence.json'])
    require(isinstance(evidence, dict) and set(evidence) == EVIDENCE_FIELDS and
            type(evidence.get('schema')) is int and evidence['schema'] == 1 and
            evidence.get('profile') == 'rev12-ota' and evidence.get('source_checked_locally') is True and
            type(evidence.get('candidate_provisioned')) is bool, 'unsupported build evidence')
    require(evidence.get('source_commit') == plan['source_commit'] and evidence.get('sdk') == plan['sdk'] and
            evidence.get('application') == app and evidence.get('public_key_sha256') == plan['public_key_sha256'] and
            evidence.get('purpose') == plan['purpose'], 'build evidence/image binding mismatch')
    require(isinstance(evidence['source_tree'], str) and re.fullmatch('[0-9a-f]{40}', evidence['source_tree']) and
            isinstance(evidence['source_display_version'], str) and
            re.fullmatch(r'[A-Za-z0-9_.-]{1,80}', evidence['source_display_version']) and
            all(isinstance(evidence[k], str) and re.fullmatch('[0-9a-f]{64}', evidence[k])
                for k in ('sdkconfig_sha256', 'partitions_sha256', 'dependency_lock_sha256')),
            'invalid build evidence digest/source fields')
    for flag in ('device_access', 'install_enabled', 'release_published', 'ci_identity_verified', 'production_resource_qualified'):
        require(evidence.get(flag) is False, 'bundle must not claim operational authority')
    return dict(status='signed-offline-check-bench-only', source_commit=plan['source_commit'],
        release=plan['release_version'], image_bytes=report['image_bytes'], sha256=report['sha256'],
        public_key_sha256=plan['public_key_sha256'], device_access=False, install_enabled=False,
        release_published=False, production_resource_qualified=False)


def prepare(build, output, plan_path, public_key, private_key, repo=None):
    root = Path(repo or Path(__file__).resolve().parents[1]).absolute()
    build, out, private = Path(build).absolute(), Path(output).absolute(), Path(private_key).absolute()
    plan = plan_read(plan_path)
    public = public_read(public_key, plan)
    require(not out.exists() and not out.is_symlink() and not out.is_relative_to(root),
            'new output outside source checkout required')
    require(not private.is_relative_to(root) and not private.is_relative_to(out) and
            not private.is_relative_to(build), 'signing key must remain outside source/build/output')
    require(not any(p.is_symlink() for p in (private, *private.parents)) and
            stat.S_ISREG(private.stat().st_mode) and private.stat().st_uid == os.getuid() and
            stat.S_IMODE(private.stat().st_mode) == 0o600 and 0 < private.stat().st_size <= 8192 and
            stat.S_IMODE(private.parent.stat().st_mode) == 0o700,
            'owner-only regular private key file0600/directory0700 required')
    tree, display = source(root, plan['source_commit'])
    validation = check_idf_build.verify(build, 'rev12-ota', plan['sdk'], write_report=False)
    require(ota.parse(regular_read(build/'validation.json', 65536)) == validation, 'stale build validation')
    app_raw = regular_read(build/'tv4_hmi.bin', APP_MAX)
    app = inspect_app(app_raw, plan['sdk'])
    require(validation['images']['tv4_hmi.bin'] == {'bytes': len(app_raw), 'sha256': app['sha256']},
            'image changed after actual build validation')
    description = ota.parse(regular_read(build/'project_description.json', 1024*1024))
    require(description.get('project_name') == 'tv4_hmi' and
            description.get('project_version') == app['version'] == plan['release_version'],
            'actual compiler app version must equal reviewed SemVer')
    elf = regular_read(build/'tv4_hmi.elf', ELF_MAX)
    require(hashlib.sha256(elf).hexdigest() == app['elf_sha256'], 'actual ELF/app descriptor digest mismatch')
    context = ota.parse(regular_read(build/'ota-local/ota_local_context.json', 4096))
    require(type(context.get('provisioned')) is bool and context.get('private_key_present') is False,
            'invalid public-only build context')
    if context['provisioned']:
        require(context.get('public_key_sha256') == plan['public_key_sha256'] and
                context.get('installed_version') == app['version'], 'configured candidate trust/version mismatch')
    derived = subprocess.run(['openssl', 'pkey', '-in', str(private), '-passin', 'pass:',
                              '-pubout', '-outform', 'DER'], capture_output=True, timeout=15)
    require(derived.returncode == 0 and derived.stdout == public, 'private key does not match pinned public key')
    asset = 'tv4-hmi-esp32s3-v'+plan['release_version']+'.bin'
    url = 'https://github.com/avlyubimov/TV4-CNC/releases/download/v'+plan['release_version']+'/'
    manifest = dict(schema_version=1, release_version=plan['release_version'], channel=plan['channel'],
        source_commit=plan['source_commit'], signing=dict(key_id=plan['key_id'], algorithm='ECDSA-P256-SHA256'),
        protocol=dict(min=plan['protocol_version'], max=plan['protocol_version']), components=[dict(
            name='hmi', version=plan['release_version'], hardware_id='waveshare-esp32s3-touch-lcd-7',
            hardware_revision='1.2', chip='esp32s3', profile='rev12', layout_id='tv4-16m-ab-v1',
            minimum_installed_version=plan['installed_version'], minimum_bootloader_version=plan['bootloader_version'],
            size_bytes=len(app_raw), sha256=app['sha256'], url=url+asset)], release_notes=plan['release_notes'])
    evidence = dict(schema=1, purpose=plan['purpose'], source_commit=plan['source_commit'], source_tree=tree,
        source_display_version=display, sdk=plan['sdk'], profile='rev12-ota', application=app,
        public_key_sha256=plan['public_key_sha256'], candidate_provisioned=context['provisioned'],
        source_checked_locally=True, device_access=False, install_enabled=False, release_published=False,
        ci_identity_verified=False, production_resource_qualified=False,
        sdkconfig_sha256=hashlib.sha256(regular_read(build/'sdkconfig', SDKCONFIG_MAX)).hexdigest(),
        partitions_sha256=hashlib.sha256(regular_read(build/'partition_table/partition-table.bin', 4096)).hexdigest(),
        dependency_lock_sha256=hashlib.sha256(regular_read(root/('firmware/esp32-hmi/locks/idf-'+plan['sdk']+'.lock'), 65536)).hexdigest())
    out.parent.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(prefix='.tv4-signed-check-', dir=out.parent) as tmp:
        stage = Path(tmp)/'bundle'; stage.mkdir(mode=0o700)
        files = {asset: app_raw, 'manifest.json': encoded(manifest), 'build-evidence.json': encoded(evidence),
                 'README.txt': NOTICE, 'release-notes.md': (plan['release_notes']+'\n').encode('utf-8'),
                 'LICENSE.txt': regular_read(root/'LICENSE', PLAN_MAX),
                 'LICENSE-NOTICE.md': regular_read(root/'LICENSE-NOTICE.md', PLAN_MAX)}
        for name, data in files.items(): (stage/name).write_bytes(data)
        result = subprocess.run(['openssl', 'dgst', '-sha256', '-sign', str(private), '-passin', 'pass:',
                                 str(stage/'manifest.json')], capture_output=True, timeout=15)
        require(result.returncode == 0, 'local signing failed')
        signature = base64.b64encode(result.stdout).decode('ascii')
        ota.decode_signature(signature)
        files['manifest.sig'] = encoded(dict(key_id=plan['key_id'], algorithm='ECDSA-P256-SHA256', signature=signature))
        (stage/'manifest.sig').write_bytes(files['manifest.sig'])
        (stage/'checksums.sha256').write_bytes(inventory(files))
        verified = verify(stage, plan_path, public_key)
        require(source(root, plan['source_commit']) == (tree, display), 'source changed while signing')
        require(plan_read(plan_path) == plan and public_read(public_key, plan) == public,
                'local policy/key changed while signing')
        for path in stage.iterdir():
            os.chmod(path, 0o600)
            with path.open('rb') as stream: os.fsync(stream.fileno())
        stage.rename(out)
    return verified


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--plan', required=True); p.add_argument('--public-key', required=True)
    p.add_argument('--verify'); p.add_argument('--build'); p.add_argument('--output'); p.add_argument('--private-key')
    a = p.parse_args()
    try:
        if a.verify:
            require(not (a.build or a.output or a.private_key), 'verification never accepts a private key or build')
            result = verify(a.verify, a.plan, a.public_key)
        else:
            require(a.build and a.output and a.private_key, 'explicit build/output/private-key inputs required')
            os.umask(0o077)
            result = prepare(a.build, a.output, a.plan, a.public_key, a.private_key)
        print(json.dumps(result, indent=2))
    except (ValueError, KeyError, TypeError, OSError, RecursionError, subprocess.SubprocessError) as error:
        raise SystemExit('Signed CHECK bundle rejected: '+str(error)) from None


if __name__ == '__main__':
    main()
