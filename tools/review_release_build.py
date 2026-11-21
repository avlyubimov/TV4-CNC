#!/usr/bin/env python3
"""Offline build-to-signed-cohort audit; valid reports are BLOCKED, exit 3.

External candidate pin and local file/tool paths are explicit operator inputs.
Unsigned build claims are never CI/source authentication or install authority.
Owned quiescent filesystem/toolchain required; no hostile local actor sandbox.
"""
import copy
import json
from pathlib import Path
import subprocess
import tempfile

import build_candidate as build
import review_release_cohort as cohort

ELF_MAX = 32 * 1024 * 1024  # Host digest ceiling, not MCU memory capacity.
BLOCKED_EXIT = 3


def audit(args):
    cohort.require(build.sha(args.hmi_candidate_sha256) and args.hmi_candidate_sha256 != '0' * 64,
                   'explicit nonzero lowercase candidate SHA-256 required')
    cohort.require(args.hmi_build_profile in build.PROFILES, 'explicit build profile required')
    directory = Path(args.hmi_build_candidate)
    cohort.require(not directory.is_symlink() and directory.is_dir(), 'expected candidate directory')
    cohort.require({p.name for p in directory.iterdir()} == build.FILES, 'unexpected/missing review files')
    # Only fixed local names. Never interpret a path/URL/command from metadata.
    inputs = {name: build.regular_read(directory / name, build.META_MAX) for name in build.FILES}
    cohort.require(cohort.sha(inputs['candidate.json']) == args.hmi_candidate_sha256,
                   'candidate SHA-256 pin mismatch')
    image = build.regular_read(args.hmi_image, build.APP_MAX)
    elf = build.regular_read(args.hmi_elf, ELF_MAX)
    with tempfile.TemporaryDirectory(prefix='tv4-release-build-') as temp:
        root = Path(temp)
        candidate_dir = root / 'candidate'
        candidate_dir.mkdir()
        for name, raw in inputs.items():
            (candidate_dir / name).write_bytes(raw)
        image_path = root / 'hmi-image'
        image_path.write_bytes(image)
        request = copy.copy(args)
        request.hmi_image = image_path
        checked, captured = build.verify_with_metadata(candidate_dir, args.source_commit, args.hmi_sdk,
            args.hmi_build_profile, image_path, args.hmi_candidate_sha256)
        candidate = captured['candidate']
        cohort.require(build.digest(elf) == candidate['elf'], 'local ELF digest/length mismatch')
        signed_cohort, details = cohort.review_with_details(request)
        # Both checkers inspected the exact same private image snapshot; binding
        # fields below come from their captured verified metadata, not the paths.
        cohort.require(checked['image_checked'] is True and
                       candidate['application'] == details['hmi_application'],
                       'build candidate and signed HMI application disagree')
        signed = details['hmi_signed']
        cohort.require((candidate['source_commit'], candidate['sdk'], candidate['application']['version']) ==
                       (signed_cohort['source_commit'], args.hmi_sdk, signed['version']),
                       'build/signed source, SDK or component version mismatch')
        cohort.require(captured['candidate_sha256'] == args.hmi_candidate_sha256,
                       'captured candidate does not match external pin')
    profile_match = candidate['profile'] == signed['profile']
    layout_match = candidate['layout']['id'] == signed['layout_id']
    blockers = ['unsigned-build-provenance', 'production-authority-gates-open']
    if not profile_match:
        blockers.append('hmi-build-profile-mismatch')
    if not layout_match:
        blockers.append('hmi-build-layout-mismatch')
    if signed_cohort['motion_present']:
        blockers.append('motion-build-evidence-unavailable')
    # No success/ready path. Even matching bytes and an external metadata pin do
    # not establish CI identity, signer custody, physical state or provisioning.
    return dict(schema='tv4-release-build-review-draft-1', status='blocked-build-review-only',
        candidate_pin_checked=True, image_checked=True, elf_digest_checked=True,
        elf_format_checked=False, ci_identity_verified=False, build_provenance_verified=False,
        profile_match=profile_match, layout_match=layout_match, blockers=blockers,
        hmi_build=dict(source_commit=candidate['source_commit'], source_tree=candidate['source_tree'],
            sdk=candidate['sdk'], profile=candidate['profile'], signed_profile=signed['profile'],
            candidate_sha256=captured['candidate_sha256'], inventory_sha256=captured['inventory_sha256'],
            application=candidate['application'], elf=candidate['elf'], sdkconfig=candidate['sdkconfig'],
            partition_table=candidate['partition_table'], layout=candidate['layout'],
            signed_layout_id=signed['layout_id'], supporting_files=candidate['files']),
        signed_cohort=signed_cohort, gates={name: False for name in cohort.CLOSED_GATES},
        device_access=False, install_enabled=False, release_published=False)


def parser():
    p = cohort.parser()
    p.description = __doc__
    for name in ('hmi-build-candidate', 'hmi-candidate-sha256', 'hmi-elf'):
        p.add_argument('--' + name, required=True)
    p.add_argument('--hmi-build-profile', choices=build.PROFILES, required=True)
    return p


def main():
    try:
        report = audit(parser().parse_args())
    except (ValueError, TypeError, KeyError, AttributeError, RecursionError, OSError,
            subprocess.SubprocessError) as error:
        raise SystemExit('Release build audit rejected: ' + str(error)) from None
    print(json.dumps(report, indent=2, sort_keys=True))
    raise SystemExit(BLOCKED_EXIT)


if __name__ == '__main__':
    main()
