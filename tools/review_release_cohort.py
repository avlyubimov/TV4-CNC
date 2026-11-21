#!/usr/bin/env python3
"""Offline signed component cohort review. No network, signing or installation.

Local context/selectors and the host verifier executable are trusted explicit
inputs, never fields/paths/commands from a manifest. Owned quiescent filesystem
required; this is not a sandbox for a hostile local executable/filesystem actor.
"""
import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import subprocess
import tempfile

from build_candidate import regular_read
from esp_app_image import APP_MAX, inspect_app
import verify_ota_candidate as ota

CONTEXT_MAX = 16384
MOTION_IMAGE_MAX = 16 * 1024 * 1024  # Host review cap, not a board memory profile.
DRIVER_MAX = 64 * 1024 * 1024
HMI_FIELDS = ('key_id', 'installed_version', 'bootloader_version', 'protocol_version', 'motion_version', 'layout_id')
MOTION_TEXT = ('key_id', 'hardware_id', 'hardware_revision', 'memory_profile',
               'installed_component_version', 'minimum_component_version')
MOTION_NUMBERS = ('installed_image_counter', 'minimum_image_counter', 'bootloader_counter',
                  'protocol', 'max_image_bytes', 'timeout_ms')
MOTION_INPUTS = ('motion_metadata', 'motion_signature', 'motion_public_key', 'motion_image', 'motion_driver')
CLOSED_GATES = ('install', 'publish', 'license_selection', 'exact_release_owner_approval', 'build_provenance', 'actual_layout', 'actual_installed_context',
                'key_provisioning', 'protocol_mapping', 'motion_vectors', 'live_source',
                'physical_quiescent', 'recovery', 'runtime_resource_budget')


def require(ok, reason):
    ota.require(ok, reason)


def sha(raw):
    return hashlib.sha256(raw).hexdigest()


def identifier(value, maximum=31):
    return isinstance(value, str) and re.fullmatch(r'[A-Za-z0-9._-]{1,' + str(maximum) + '}', value) is not None


def context_read(raw, motion):
    c = ota.parse(raw)
    ota.keys(c, ('schema', 'hmi'), ('motion',))
    require(c['schema'] == 'tv4-cohort-local-context-draft-1', 'unsupported local context schema')
    require(('motion' in c) == motion, 'local Motion context and explicit inputs must agree')
    h = c['hmi']
    ota.keys(h, HMI_FIELDS)
    require(identifier(h['key_id'], 64), 'invalid local HMI key id')
    for field in ('installed_version', 'bootloader_version', 'protocol_version'):
        ota.version(h[field])
    if h['motion_version'] is not None:
        ota.version(h['motion_version'])
    require(isinstance(h['layout_id'], str) and 0 < len(h['layout_id']) <= 31, 'unknown local HMI layout')
    if motion:
        m = c['motion']
        ota.keys(m, (*MOTION_TEXT, *MOTION_NUMBERS))
        for field in MOTION_TEXT[:4]:
            require(identifier(m[field]), 'invalid local Motion identifier')
        for field in MOTION_TEXT[4:]:
            ota.version(m[field])
            require(len(m[field]) <= 31, 'Motion version exceeds native bound')
        for field in MOTION_NUMBERS:
            require(type(m[field]) is int and 0 < m[field] <= 0xffffffff, 'invalid local Motion counter/bound')
        require(m['timeout_ms'] <= 10000 and m['max_image_bytes'] <= MOTION_IMAGE_MAX, 'host Motion review bound exceeded')
        require(h['motion_version'] == m['installed_component_version'], 'local installed Motion contexts disagree')
    return c


def motion_arguments(m, paths, selection):
    # Fixed trusted program, explicit local arguments; no shell or downloaded
    # executable/command/paths. Numeric protocol remains a separate local input.
    return [str(paths[name]) for name in ('metadata', 'signature', 'public', 'image')] + [
        m[field] for field in MOTION_TEXT[:4]] + [str(m[field]) for field in MOTION_NUMBERS] + [
        m['installed_component_version'], m['minimum_component_version'],
        selection.release_version, selection.source_commit, selection.channel]


def motion_verify(driver, m, paths, selection, raw):
    result = subprocess.run([str(driver), *motion_arguments(m, paths, selection)],
                            capture_output=True, timeout=15)
    require(len(result.stdout) <= 4096 and len(result.stderr) <= 16384, 'host Motion reviewer output bound exceeded')
    require(result.returncode == 0 and not result.stderr, 'independent Motion verifier rejected candidate')
    v = ota.parse(result.stdout)
    ota.keys(v, ('kind', 'status', 'result', 'release_version', 'component_version', 'source_commit',
                 'channel', 'key_id', 'image_bytes', 'image_counter', 'sha256', 'metadata_sha256',
                 'workspace_zero', 'device_access', 'install_enabled'))
    require(v['kind'] == 'tv4-motion-host-review-1' and v['status'] == 'verified-offline-only' and
            type(v['result']) is int and v['result'] == 0 and v['workspace_zero'] is True and
            v['device_access'] is False and v['install_enabled'] is False, 'invalid native Motion review result')
    require((v['release_version'], v['source_commit'], v['channel'], v['key_id']) ==
            (selection.release_version, selection.source_commit, selection.channel, m['key_id']), 'native Motion selection mismatch')
    require(type(v['image_bytes']) is int and 0 < v['image_bytes'] <= m['max_image_bytes'] and
            type(v['image_counter']) is int and v['image_counter'] > m['installed_image_counter'], 'invalid native Motion image binding')
    ota.version(v['component_version'])
    require(len(v['component_version']) <= 31 and all(isinstance(v[k], str) and re.fullmatch('[0-9a-f]{64}', v[k])
            for k in ('sha256', 'metadata_sha256')), 'invalid native Motion digest/version')
    require(v['metadata_sha256'] == sha(raw['metadata']) and v['sha256'] == sha(raw['image']) and
            v['image_bytes'] == len(raw['image']), 'native Motion report/snapshot mismatch')
    return v


def review_with_details(args):
    """Internal captured fields for subsequent read-only audits; not authority."""
    ota.version(args.release_version)
    require(isinstance(args.source_commit, str) and re.fullmatch('[0-9a-f]{40}', args.source_commit) and
            args.source_commit != '0' * 40, 'explicit nonzero reviewed source SHA required')
    require(args.channel in ('stable', 'prerelease') and args.hmi_sdk in ('5.3.2', '6.1.1'), 'explicit channel/SDK required')
    supplied = [getattr(args, name) is not None for name in MOTION_INPUTS]
    require(not any(supplied) or all(supplied), 'all Motion inputs/driver must be explicit together')
    motion = all(supplied)
    raw_context = regular_read(args.context, CONTEXT_MAX)
    local = context_read(raw_context, motion)
    # Snapshot every bounded regular artifact before crypto. No input document
    # is reopened to obtain fields after it was authenticated.
    hraw = dict(manifest=regular_read(args.hmi_manifest, ota.MANIFEST_MAX),
                signature=regular_read(args.hmi_signature, ota.ENVELOPE_MAX),
                public=regular_read(args.hmi_public_key, 91), image=regular_read(args.hmi_image, APP_MAX))
    mraw = None
    driver = None
    driver_digest = None
    if motion:
        mraw = dict(metadata=regular_read(args.motion_metadata, 1024),
                    signature=regular_read(args.motion_signature, 72),
                    public=regular_read(args.motion_public_key, 91),
                    image=regular_read(args.motion_image, local['motion']['max_image_bytes']))
        driver = Path(args.motion_driver).absolute()
        driver_digest = sha(regular_read(driver, DRIVER_MAX))
        require(os.access(driver, os.X_OK), 'local host Motion reviewer is not executable')
    with tempfile.TemporaryDirectory(prefix='tv4-cohort-review-') as temp:
        root = Path(temp)
        def snapshot(role, values):
            paths = {}
            for name, raw in values.items():
                paths[name] = root / (role + '-' + name)
                paths[name].write_bytes(raw)
            return paths
        hp = snapshot('hmi', hraw)
        h = local['hmi']
        request = argparse.Namespace(manifest=hp['manifest'], signature=hp['signature'], public_key=hp['public'],
            image=hp['image'], trusted_key_id=h['key_id'], installed_version=h['installed_version'],
            bootloader_version=h['bootloader_version'], protocol_version=h['protocol_version'],
            motion_version=h['motion_version'], layout_id=h['layout_id'], channel=args.channel)
        signed, image_check = ota.verify_with_metadata(request)
        require((signed['release'], signed['source_commit'], signed['channel']) ==
                (args.release_version, args.source_commit, args.channel), 'HMI signed release/source/channel mismatch')
        app = inspect_app(hraw['image'], args.hmi_sdk)
        require(app['version'] == signed['version'] and app['sha256'] == image_check['sha256'],
                'HMI app descriptor does not match signed component version/image')
        components = [dict(name='hmi', component_version=signed['version'], key_id=h['key_id'],
            key_sha256=signed['key_sha256'], metadata_sha256=signed['metadata_sha256'],
            signature_sha256=sha(hraw['signature']), image_bytes=app['bytes'], sha256=app['sha256'],
            sdk=args.hmi_sdk, app_descriptor_checked=True)]
        if motion:
            mp = snapshot('motion', mraw)
            verified = motion_verify(driver, local['motion'], mp, args, mraw)
            peer = signed['compatible_peer_versions']
            require(peer is not None, 'signed HMI peer range required for paired review')
            require(ota.version(peer['motion_min']) <= ota.version(verified['component_version']) <=
                    ota.version(peer['motion_max']), 'prospective Motion component is outside signed HMI peer range')
            components.append(dict(name='motion', component_version=verified['component_version'],
                key_id=verified['key_id'], key_sha256=sha(mraw['public']),
                metadata_sha256=verified['metadata_sha256'], signature_sha256=sha(mraw['signature']),
                image_bytes=verified['image_bytes'], sha256=verified['sha256'], image_counter=verified['image_counter'],
                native_workspace_zero=True))
    report = dict(schema='tv4-release-cohort-review-draft-1', status='consistent-offline-review-only',
        release_version=args.release_version, source_commit=args.source_commit, channel=args.channel,
        local_context_sha256=sha(raw_context), components=components, motion_present=motion,
        prospective_peer_checked=motion, gates={name: False for name in CLOSED_GATES},
        device_access=False, install_enabled=False, release_published=False,
        protocol_semver=local['hmi']['protocol_version'])
    if motion:
        report.update(protocol_counter=local['motion']['protocol'], host_motion_driver_sha256=driver_digest)
    return report, dict(hmi_signed=signed, hmi_application=app)


def review(args):
    # Existing cohort CLI/report remains unchanged.
    return review_with_details(args)[0]


def parser():
    p = argparse.ArgumentParser(description=__doc__)
    for name in ('context', 'release-version', 'source-commit', 'hmi-manifest', 'hmi-signature', 'hmi-public-key', 'hmi-image'):
        p.add_argument('--' + name, required=True)
    p.add_argument('--channel', choices=('stable', 'prerelease'), required=True)
    p.add_argument('--hmi-sdk', choices=('5.3.2', '6.1.1'), required=True)
    for name in MOTION_INPUTS:
        p.add_argument('--' + name.replace('_', '-'))
    return p


def main():
    try:
        report = review(parser().parse_args())
    except (ValueError, TypeError, KeyError, RecursionError, OSError, subprocess.SubprocessError) as error:
        raise SystemExit('Release cohort review rejected: ' + str(error)) from None
    print(json.dumps(report, indent=2, sort_keys=True))


if __name__ == '__main__':
    main()
