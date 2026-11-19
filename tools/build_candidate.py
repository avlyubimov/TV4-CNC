#!/usr/bin/env python3
"""Create/verify metadata-only build review bundles. No signing/publish/device I/O."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import stat
import subprocess
import tempfile

import check_idf_build
from esp_app_image import APP_MAX, inspect_app, require

PROFILES = ('headless', 'rev12', 'rev12-memory-bench', 'rev12-tls-bench')
SDK_IMAGES = {
    '5.3.2': 'espressif/idf@sha256:5b8f58d9d9caa45b689a31a0b6a0cc53b84594e0ba5fdd8d28eeef7d6123242a',
    '6.1.1': 'espressif/idf@sha256:2b4f966c37eaedc19542b0637a5f850b2eff9478d8e858192aa0073d92c2dc91',
}
FILES = frozenset(('candidate.json', 'validation.json', 'dependencies.lock',
                   'LICENSE-NOTICE.md', 'checksums.sha256'))
META_MAX = 65536
DESCRIPTION_MAX = 1024 * 1024  # IDF 6 inventories source paths, ~350 KiB today.
GATES = ['owner-license-selection', 'release-version-context', 'trusted-key-provisioning',
         'migration-recovery', 'runtime-resource-and-fault-tests', 'exact-release-owner-approval']
POLICY = dict(metadata_only=True, manifest_signed=False, release_published=False,
              install_enabled=False, device_access=False, authenticity_verified=False)
LAYOUT = dict(id='factory-single-2m', app_offset=0x10000, app_bytes=APP_MAX, ota_slots=0)


def encoded(value):
    return (json.dumps(value, sort_keys=True, indent=2, ensure_ascii=True) + '\n').encode('ascii')


def regular_read(path, maximum):
    path = Path(path)
    require(not any(p.is_symlink() for p in (path, *path.parents)), 'symlink input rejected')
    require(stat.S_ISREG(path.stat().st_mode), 'expected regular file')
    # O_NONBLOCK prevents a concurrent replacement by a FIFO from blocking the
    # reviewer. O_NOFOLLOW and fstat check the actual descriptor too.
    with os.fdopen(os.open(path, os.O_RDONLY | os.O_NONBLOCK | os.O_NOFOLLOW), 'rb') as stream:
        require(stat.S_ISREG(os.fstat(stream.fileno()).st_mode), 'expected regular file')
        data = stream.read(maximum + 1)
    require(0 < len(data) <= maximum, 'empty or oversized input')
    return data


def strict_json(raw):
    def pairs(items):
        result = {}
        for key, value in items:
            require(key not in result, 'duplicate JSON key')
            result[key] = value
        return result
    def constant(_):
        raise ValueError('non-finite JSON value')
    value = json.loads(raw.decode('utf-8'), object_pairs_hook=pairs, parse_constant=constant)
    # Bundles are generated canonically. This also rejects BOM, odd surrogate/
    # alternate encodings, and noncanonical checksum inventory representations.
    require(encoded(value) == raw, 'noncanonical review metadata')
    return value


def digest(raw):
    return dict(bytes=len(raw), sha256=hashlib.sha256(raw).hexdigest())


def sha(value):
    return isinstance(value, str) and re.fullmatch('[0-9a-f]{64}', value) is not None


def commit(value):
    require(isinstance(value, str) and re.fullmatch('[0-9a-f]{40}', value), 'expected full source SHA')


def git(repo, *args):
    return subprocess.check_output(['git', '-C', str(repo), *args], text=True,
                                   stderr=subprocess.PIPE, timeout=15).strip()


def source(repo, expected):
    commit(expected)
    require(git(repo, 'rev-parse', 'HEAD') == expected, 'source commit mismatch')
    require(not git(repo, 'status', '--porcelain=v1', '--untracked-files=normal'),
            'source checkout must be clean, including untracked files')
    return git(repo, 'rev-parse', 'HEAD^{tree}'), git(repo, 'describe', '--always', '--dirty')


def inventory(files):
    return ''.join(hashlib.sha256(raw).hexdigest() + '  ' + name + '\n'
                   for name, raw in sorted(files.items())).encode('ascii')


def check_validation(v, sdk, profile):
    require(isinstance(v, dict) and set(v) == {
        'profile', 'sdk', 'target', 'flash_bytes', 'mbedtls_allocator', 'mbedtls_certificate_dates',
        'tls_record_in', 'tls_record_out', 'memory_bench', 'tls_bench', 'psram', 'images',
        'partitions', 'device_access', 'physical_performance'}, 'unknown validation fields')
    require(v.get('sdk') == sdk and v.get('profile') == profile,
            'validation profile/SDK mismatch')
    require(v.get('target') == 'esp32s3' and v.get('flash_bytes') == 16777216 and
            type(v['flash_bytes']) is int and v.get('device_access') is False and
            v['physical_performance'] == 'not measured', 'invalid validation target/scope')
    require(v.get('memory_bench') is (profile == 'rev12-memory-bench') and
            v.get('tls_bench') is (profile == 'rev12-tls-bench'), 'bench mislabelled')
    require(v.get('psram') == ('disabled' if profile == 'headless' else 'octal 80MHz'),
            'wrong PSRAM profile')
    require(v.get('mbedtls_allocator') == ('spiram-only' if profile == 'rev12-tls-bench' else 'internal-only'),
            'wrong allocator profile')
    require(type(v['mbedtls_certificate_dates']) is bool and
            all(v[k] is None or (type(v[k]) is int and 0 < v[k] <= 65536)
                for k in ('tls_record_in', 'tls_record_out')), 'invalid TLS fields')
    if profile == 'rev12-tls-bench':
        require(v['mbedtls_certificate_dates'] and v['tls_record_in'] == 16384 and
                v['tls_record_out'] == 4096, 'TLS bench date/record gates missing')
    expected = [(1, 2, 0x9000, 0x6000, 'nvs'), (1, 1, 0xf000, 0x1000, 'phy_init'),
                (0, 0, 0x10000, APP_MAX, 'factory')]
    parts = v.get('partitions')
    require(isinstance(parts, list) and len(parts) == 3, 'unexpected active layout')
    require(all(isinstance(p, dict) for p in parts), 'invalid partition entry')
    require(all(set(p) == {'kind', 'subtype', 'address', 'size', 'label'} and
                all(type(p[k]) is int for k in ('kind', 'subtype', 'address', 'size'))
                for p in parts), 'unknown/invalid partition fields')
    require([(p.get('kind'), p.get('subtype'), p.get('address'), p.get('size'), p.get('label'))
             for p in parts] == expected, 'only current factory layout is supported')
    require(set(v['images']) == {'tv4_hmi.bin', 'bootloader/bootloader.bin'}, 'unknown image inventory')
    for name, maximum in (('tv4_hmi.bin', APP_MAX), ('bootloader/bootloader.bin', 0x8000)):
        item = v['images'][name]
        require(isinstance(item, dict) and set(item) == {'bytes', 'sha256'} and
                type(item['bytes']) is int and 0 < item['bytes'] <= maximum and sha(item['sha256']),
                'invalid validation image digest')


def create(repo, build, output, sdk, profile, expected):
    repo, build, output = Path(repo), Path(build), Path(output)
    require(sdk in SDK_IMAGES and profile in PROFILES, 'unsupported SDK/profile')
    require(not output.exists() and not output.is_symlink(), 'output already exists')
    tree, build_version = source(repo, expected)
    for name, maximum in (('config/sdkconfig.json', META_MAX * 4),
                          ('project_description.json', DESCRIPTION_MAX), ('tv4_hmi.bin', APP_MAX),
                          ('bootloader/bootloader.bin', 0x8000),
                          ('partition_table/partition-table.bin', 4096)):
        regular_read(build / name, maximum)
    # Revalidate actual compiler inputs instead of trusting an old PASS JSON.
    validation = check_idf_build.verify(build, profile, sdk, write_report=False)
    require(json.loads(regular_read(build / 'validation.json', META_MAX)) == validation,
            'stale build validation')
    check_validation(validation, sdk, profile)
    description = json.loads(regular_read(build / 'project_description.json', DESCRIPTION_MAX))
    app = inspect_app(regular_read(build / 'tv4_hmi.bin', APP_MAX), sdk)
    require(description.get('project_name') == app['project'] and
            description.get('project_version') == app['version'] == build_version,
            'stale app/project version does not match clean source')
    elf = regular_read(build / 'tv4_hmi.elf', 32 * 1024 * 1024)
    require(hashlib.sha256(elf).hexdigest() == app['elf_sha256'], 'app/ELF digest mismatch')
    require(validation['images']['tv4_hmi.bin'] == {k: app[k] for k in ('bytes', 'sha256')},
            'app changed during validation')
    files = {'validation.json': encoded(validation),
             'dependencies.lock': regular_read(repo / 'firmware/esp32-hmi/locks' / ('idf-' + sdk + '.lock'), META_MAX),
             'LICENSE-NOTICE.md': regular_read(repo / 'LICENSE-NOTICE.md', META_MAX)}
    candidate = dict(schema_version=1, kind='tv4-unsigned-build-review', source_commit=expected,
                     source_tree=tree, sdk=sdk, reference_sdk_image=SDK_IMAGES[sdk], profile=profile,
                     application=app, elf=digest(elf), layout=LAYOUT, policy=POLICY,
                     release_gates=GATES, files={name: digest(raw) for name, raw in files.items()},
                     sdkconfig=digest(regular_read(build / 'sdkconfig', META_MAX * 4)),
                     partition_table=digest(regular_read(build / 'partition_table/partition-table.bin', 4096)))
    files['candidate.json'] = encoded(candidate)
    files['checksums.sha256'] = inventory(files)
    # Recheck the source at the end. Packaging assumes a quiescent owned build
    # directory; it cannot attest how arbitrary external binaries were built.
    require(source(repo, expected) == (tree, build_version), 'source changed during packaging')
    output.parent.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(prefix='.tv4-review-', dir=output.parent) as temp:
        stage = Path(temp) / 'bundle'
        stage.mkdir()
        for name, raw in files.items():
            (stage / name).write_bytes(raw)
        verify(stage, expected, sdk, profile)
        require(not output.exists(), 'output already exists')
        stage.rename(output)
    return dict(status='created-metadata-only', source_commit=expected, sdk=sdk, profile=profile,
                app_sha256=app['sha256'], binary_uploaded=False, **POLICY)


def verify(directory, expected, sdk, profile, image=None):
    commit(expected)
    require(sdk in SDK_IMAGES and profile in PROFILES, 'unsupported SDK/profile')
    directory = Path(directory)
    require(not directory.is_symlink() and directory.is_dir(), 'expected bundle directory')
    require({p.name for p in directory.iterdir()} == FILES, 'unexpected/missing review files')
    files = {name: regular_read(directory / name, META_MAX) for name in FILES}
    checksum = files.pop('checksums.sha256')
    require(checksum == inventory(files), 'review inventory checksum mismatch')
    c = strict_json(files['candidate.json'])
    require(set(c) == {'schema_version', 'kind', 'source_commit', 'source_tree', 'sdk', 'reference_sdk_image',
                       'profile', 'application', 'elf', 'layout', 'policy', 'release_gates', 'files',
                       'sdkconfig', 'partition_table'}, 'unknown candidate fields')
    require(type(c['schema_version']) is int and c['schema_version'] == 1 and
            c['kind'] == 'tv4-unsigned-build-review', 'unsupported review schema')
    require(c['source_commit'] == expected and c['sdk'] == sdk and c['profile'] == profile,
            'unexpected candidate source/SDK/profile')
    commit(c['source_tree'])
    require(c['reference_sdk_image'] == SDK_IMAGES[sdk] and encoded(c['policy']) == encoded(POLICY) and
            encoded(c['layout']) == encoded(LAYOUT) and c['release_gates'] == GATES,
            'invalid review scope/gates')
    expected_files = {name: digest(raw) for name, raw in files.items() if name != 'candidate.json'}
    require(c['files'] == expected_files, 'payload inventory mismatch')
    validation = strict_json(files['validation.json'])
    check_validation(validation, sdk, profile)
    a = c['application']
    require(set(a) == {'bytes', 'sha256', 'project', 'version', 'idf_version', 'elf_sha256',
                       'segments', 'chip_revision_min', 'chip_revision_max', 'flash_bytes',
                       'flash_mode', 'flash_frequency_mhz'}, 'invalid app fields')
    require(type(a['bytes']) is int and 336 <= a['bytes'] <= APP_MAX and sha(a['sha256']) and
            a['project'] == 'tv4_hmi' and a['idf_version'] == 'v' + sdk and
            isinstance(a['version'], str) and 0 < len(a['version']) <= 31 and
            all(32 <= ord(x) <= 126 for x in a['version']) and 'dirty' not in a['version'],
            'invalid app identity')
    require(type(a['segments']) is int and 1 <= a['segments'] <= 16 and
            type(a['chip_revision_min']) is int and type(a['chip_revision_max']) is int and
            0 <= a['chip_revision_min'] <= a['chip_revision_max'] <= 65535 and
            a['flash_bytes'] == 16777216 and a['flash_mode'] == 'dio' and
            a['flash_frequency_mhz'] == 80, 'invalid app format')
    require(validation['images']['tv4_hmi.bin'] == {k: a[k] for k in ('bytes', 'sha256')},
            'app/validation mismatch')
    for field, bound in (('elf', 32 * 1024 * 1024), ('sdkconfig', META_MAX * 4), ('partition_table', 4096)):
        require(set(c[field]) == {'bytes', 'sha256'} and type(c[field]['bytes']) is int and
                0 < c[field]['bytes'] <= bound and sha(c[field]['sha256']), 'invalid ' + field + ' digest')
    require(sha(a['elf_sha256']) and a['elf_sha256'] == c['elf']['sha256'], 'ELF identity mismatch')
    if image is not None:
        require(inspect_app(regular_read(image, APP_MAX), sdk) == a, 'local app does not match review')
    return dict(status='verified-metadata-only', source_commit=expected, sdk=sdk, profile=profile,
                image_checked=image is not None, **POLICY)


def main():
    p = argparse.ArgumentParser(description=__doc__)
    sub = p.add_subparsers(dest='action', required=True)
    for action in ('create', 'verify'):
        q = sub.add_parser(action)
        q.add_argument('--source-commit', required=True)
        q.add_argument('--sdk', required=True, choices=tuple(SDK_IMAGES))
        q.add_argument('--profile', required=True, choices=PROFILES)
        if action == 'create':
            q.add_argument('--repo', required=True, type=Path)
            q.add_argument('--build', required=True, type=Path)
            q.add_argument('--output', required=True, type=Path)
        else:
            q.add_argument('--candidate', required=True, type=Path)
            q.add_argument('--image', type=Path)
    a = p.parse_args()
    try:
        if a.action == 'create':
            result = create(a.repo, a.build, a.output, a.sdk, a.profile, a.source_commit)
        else:
            result = verify(a.candidate, a.source_commit, a.sdk, a.profile, a.image)
    except (ValueError, TypeError, KeyError, AttributeError, RecursionError, OSError, subprocess.SubprocessError) as error:
        raise SystemExit('Build candidate rejected: ' + str(error)) from None
    print(json.dumps(result, sort_keys=True, indent=2))


if __name__ == '__main__':
    main()
