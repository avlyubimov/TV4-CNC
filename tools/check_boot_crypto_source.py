#!/usr/bin/env python3
"""Verify the pinned source subset even when an extracted build cache exists."""
import argparse
import hashlib
import json
from pathlib import Path


def verify(source, lock_path):
    source = Path(source).resolve(strict=True)
    lock = json.loads(Path(lock_path).read_text())
    names = lock['crypto_sources']
    if not names or any(not name.replace('_', '').isalnum() for name in names):
        raise ValueError('invalid source list')
    files = sorted({*source.glob('include/**/*.h'), *source.glob('library/*.h'),
                    *(source / 'library' / f'{name}.c' for name in names), source / 'LICENSE'})
    digest = hashlib.sha256()
    for path in files:
        if path.is_symlink() or not path.is_file() or path.stat().st_size > 4 * 1024 * 1024:
            raise ValueError('invalid crypto source file')
        relative = path.relative_to(source).as_posix()
        digest.update(relative.encode() + b'\0' + hashlib.sha256(path.read_bytes()).hexdigest().encode() + b'\n')
    if len(files) != lock['tree_file_count'] or digest.hexdigest() != lock['tree_sha256']:
        raise ValueError('Mbed TLS source integrity mismatch')
    return {'version': lock['version'], 'files': len(files), 'tree_sha256': digest.hexdigest()}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--source', required=True)
    parser.add_argument('--lock', required=True)
    args = parser.parse_args()
    try:
        result = verify(args.source, args.lock)
    except (ValueError, OSError, KeyError) as exc:
        parser.exit(1, f'Crypto source rejected: {exc}\n')
    print('PASS pinned Mbed TLS source: ' + json.dumps(result, sort_keys=True))


if __name__ == '__main__':
    main()
