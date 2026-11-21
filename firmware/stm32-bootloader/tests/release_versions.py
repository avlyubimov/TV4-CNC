"""Bounded bootloader SemVer against the independently tested Python oracle.

The driver calls the actual bootloader policy helper. No signer, files, device
access, network or HMI library in the independent STM32 implementation.
"""
import argparse
import importlib.util
import itertools
from pathlib import Path
import subprocess

ROOT = Path(__file__).resolve().parents[3]
spec = importlib.util.spec_from_file_location('offline', ROOT / 'tools/verify_ota_candidate.py')
ota = importlib.util.module_from_spec(spec)
spec.loader.exec_module(ota)


def version(text):
    if not isinstance(text, str) or not 0 < len(text.encode('utf-8')) < 32:
        raise ValueError('bootloader subset bound')
    return ota.version(text)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--driver', required=True)
    args = parser.parse_args()
    ordered = ['1.0.0-alpha', '1.0.0-alpha.1', '1.0.0-alpha.beta', '1.0.0-beta',
               '1.0.0-beta.2', '1.0.0-beta.11', '1.0.0-rc.1', '1.0.0']
    values = set(ordered + ['0.0.0', '1.9.0', '1.10.0', '4294967295.0.0',
        '0.4294967295.0', '0.0.4294967295', '1.0.0-' + '9'*25, '1.0.0-' + 'a'*25])
    values.update(core+'-'+pre for core, pre in itertools.product(['0.0.0','1.0.0','2.1.3'],
        ['0','2','11','A','a','-','--','01a','0.1','a.0','a.b']))
    pairs = list(itertools.product(sorted(values), repeat=2))
    invalid = [None, '', '1','1.0','1.0.0.0','01.0.0','1.01.0','1.0.01','4294967296.0.0',
        '0.4294967296.0','0.0.4294967296','1.0.0-','1.0.0-01','1.0.0-a..b','1.0.0-a.',
        '1.0.0+a','1.0.0-a+b','1.0.0-'+'a'*26,'1.0.0-ё','1.0.0-１２',' 1.0.0','1.0.0\n']
    invalid += ['1.0.0-'+chr(c) for c in range(1,128)
                if not (chr(c).isascii() and (chr(c).isalnum() or chr(c)=='-'))]
    for bad, good in itertools.product(invalid, ['0.0.0','1.0.0-a']):
        pairs.extend([(bad,good),(good,bad)])
    def encode(text):
        return '-' if text is None else text.encode('utf-8').hex()
    data=''.join(encode(a)+' '+encode(b)+'\n' for a,b in pairs)
    result=subprocess.run([args.driver,'--versions'],input=data,text=True,capture_output=True,timeout=20)
    assert result.returncode==0 and not result.stderr, (result.returncode,result.stderr)
    answers=result.stdout.splitlines()
    assert len(answers)==len(pairs)
    for (a,b),got in zip(pairs,answers):
        try:
            av,bv=version(a),version(b)
            expected=(av>bv)-(av<bv)
        except ValueError:
            expected=9
        assert int(got)==expected, (a,b,got,expected)
    for i,a in enumerate(ordered):
        for j,b in enumerate(ordered):
            assert (version(a)>version(b))-(version(a)<version(b))==(i>j)-(i<j)
    print(f'PASS {len(pairs)} independent bounded SemVer comparisons; 31-byte subset, numeric prerelease not truncated to uint32')


if __name__=='__main__':
    main()
