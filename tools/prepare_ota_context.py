#!/usr/bin/env python3
"""Compile LOCAL public trust/settings only. No keys generated, devices or network."""
import argparse,hashlib,json,re,subprocess
from pathlib import Path

def unique(pairs):
    result={}
    for k,v in pairs:
        if k in result:raise ValueError('duplicate local context field')
        result[k]=v
    return result

def version(s):
    if not isinstance(s,str) or len(s)>31 or not re.fullmatch(r'(0|[1-9][0-9]*)\.(0|[1-9][0-9]*)\.(0|[1-9][0-9]*)(?:-[0-9A-Za-z]+(?:[.-][0-9A-Za-z]+)*)?',s):
        raise ValueError('explicit strict SemVer <=31 characters required')
    if any(int(x)>0xffffffff for x in s.split('-')[0].split('.')):raise ValueError('SemVer overflow')
    if '-' in s and any(x.isdigit() and len(x)>1 and x[0]=='0' for x in s.split('-',1)[1].split('.')):raise ValueError('SemVer leading zero')
    return s

def prepare(context,output):
    p=Path(output);p.mkdir(parents=True,exist_ok=True)
    config=None;key=b''
    if context:
        source=Path(context);raw=source.read_bytes()
        if len(raw)>8192:raise ValueError('local context too large')
        c=json.loads(raw,object_pairs_hook=unique,parse_constant=lambda _: (_ for _ in ()).throw(ValueError("invalid JSON constant")))
        fields={'reviewed','key_id','public_key','installed_version','bootloader_version','protocol_version','manifest_url','prerelease','resources','time'}
        if set(c)!=fields or c['reviewed'] is not True or type(c['prerelease']) is not bool:raise ValueError('local reviewed context fields required')
        if not re.fullmatch(r'[A-Za-z0-9_.-]{1,64}',c['key_id']):raise ValueError('invalid key ID')
        for name in ['installed_version','bootloader_version','protocol_version']:version(c[name])
        if not re.fullmatch(r'https://github.com/avlyubimov/TV4-CNC/releases/download/v[0-9A-Za-z.-]{1,64}/manifest.json',c['manifest_url']):raise ValueError('exact TV4 release manifest URL required')
        key_path=source.parent/c['public_key'];key_input=key_path.read_bytes()
        if len(key_input)>4096 or b'PRIVATE' in key_input or not key_input.startswith(b'-----BEGIN PUBLIC KEY-----'):raise ValueError('PEM PUBLIC KEY only; never private keys')
        key=subprocess.check_output(['openssl','pkey','-pubin','-in',str(key_path),'-pubout','-outform','DER'],stderr=subprocess.DEVNULL)
        if len(key)!=91 or not key.startswith(bytes.fromhex('3059301306072a8648ce3d020106082a8648ce3d03010703420004')):raise ValueError('P-256 public key required')
        if set(c['resources'])!={'internal_free','internal_largest','psram_free','psram_largest','stack'}:raise ValueError('explicit measured resource floors required')
        for value in c['resources'].values():
            if type(value) is not int or not 1<=value<=16*1024*1024:raise ValueError('positive bounded resource floors required')
        if c['resources']['internal_largest']>c['resources']['internal_free'] or c['resources']['psram_largest']>c['resources']['psram_free']:raise ValueError('inconsistent resource floors')
        t=c['time']
        if set(t)!={'allow_unauthenticated_sntp','earliest_epoch','latest_epoch','maximum_age_ms','maximum_skew_ms'} or t['allow_unauthenticated_sntp'] is not True:raise ValueError('explicit SNTP source decision required')
        for k in ['earliest_epoch','latest_epoch','maximum_age_ms','maximum_skew_ms']:
            if type(t[k]) is not int or not 1<=t[k]<=0xffffffff:raise ValueError('time bound invalid')
        if t['earliest_epoch']>=t['latest_epoch'] or t['maximum_age_ms']>86400000 or t['maximum_skew_ms']>60000:raise ValueError('time policy invalid')
        config=c
    c=config or dict(key_id='',installed_version='',bootloader_version='',protocol_version='',manifest_url='',prerelease=False,resources=dict.fromkeys(['internal_free','internal_largest','psram_free','psram_largest','stack'],0),time=dict(earliest_epoch=0,latest_epoch=0,maximum_age_ms=0,maximum_skew_ms=0))
    q=json.dumps
    header=['/* Generated local PUBLIC trust/settings. No downloaded key or production default. */','#pragma once','#include <stdint.h>',f'#define TV4_OTA_PROVISIONED {int(config is not None)}',
        'static const uint8_t ota_public_key[] = {'+','.join(str(b) for b in (key or b'\0'))+'};']
    for k in ['key_id','installed_version','bootloader_version','protocol_version','manifest_url']:header.append(f'#define OTA_{k.upper()} {q(c[k])}')
    header += [f'#define OTA_PRERELEASE {int(c["prerelease"])}']
    for k,v in c['resources'].items():header.append(f'#define OTA_FLOOR_{k.upper()} {v}u')
    for k in ['earliest_epoch','latest_epoch','maximum_age_ms','maximum_skew_ms']:header.append(f'#define OTA_TIME_{k.upper()} {c["time"][k]}u')
    result=dict(provisioned=config is not None,installed_version=c['installed_version'],public_key_sha256=hashlib.sha256(key).hexdigest() if key else None,private_key_present=False)
    for name,data in [('ota_local_context.h','\n'.join(header)+'\n'),('ota_local_context.json',json.dumps(result,indent=2)+'\n')]:
        dest=p/name
        if not dest.exists() or dest.read_text()!=data:dest.write_text(data)
    return result
if __name__=='__main__':
    ap=argparse.ArgumentParser(description=__doc__);ap.add_argument('--context');ap.add_argument('--output',required=True);a=ap.parse_args()
    try:print(json.dumps(prepare(a.context,a.output)))
    except (ValueError,KeyError,TypeError,OSError,subprocess.CalledProcessError) as e:raise SystemExit('Local OTA configuration rejected: '+str(e))
