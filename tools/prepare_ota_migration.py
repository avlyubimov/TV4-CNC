#!/usr/bin/env python3
"""Prepare/verify an UNPROVISIONED local USB bundle; no ports or device tools."""
import argparse
import hashlib
import json
from pathlib import Path
import subprocess
import tempfile
import check_idf_build
from build_candidate import encoded,regular_read,strict_json,source,git,inventory
from esp_app_image import inspect_app,require

WRITES=(('bootloader.bin',0,0x8000),('partition-table.bin',0x8000,0x1000),
        ('factory-recovery.bin',0x10000,0x200000),('otadata-erased.bin',0x210000,0x2000))
README=('PREPARED ONLY. Separate exact-bundle owner authorization and verified 16MiB backup/recovery copy required.\n'
        'Initial USB migration replaces bootloader/partition table/factory app and resets otadata; NVS/PHY are not written. Power loss may require USB recovery.\n'
        'Factory boots first. Check HMI/touch/Wi-Fi and settings on an isolated board after separately authorized migration.\n'
        'No erase-all, eFuse, signing, publishing, STM32 or motion. OTA is unconfigured; no key or production defaults.\n')
FILES={'plan.json','SHA256SUMS','README.txt',*(x[0] for x in WRITES)}

def verify(directory):
    p=Path(directory)
    require({x.name for x in p.iterdir()}==FILES,'unexpected bundle inventory')
    plan=strict_json(regular_read(p/'plan.json',65536))
    expected={'schema','kind','source_commit','source_tree','sdk','profile','layout','flash_bytes','writes',
              'preserves_nvs','initial_boot','erases_all_flash','device_access','hardware_migration_authorized',
              'release_authorized','provisioned','app','elf_sha256'}
    require(set(plan)==expected,'unknown migration fields')
    require(plan['schema']==1 and plan['kind']=='tv4-usb-migration-plan' and plan['sdk'] in ('5.3.2','6.1.1') and
            plan['profile']=='rev12-ota' and plan['layout']=='tv4-16m-ab-v1' and plan['flash_bytes']==16777216,'wrong migration profile')
    for flag in ('device_access','hardware_migration_authorized','release_authorized','erases_all_flash','provisioned'):
        require(plan[flag] is False,'unauthorized migration claim')
    require(plan['preserves_nvs'] is True and plan['initial_boot']=='factory','factory recovery required')
    import re
    require(all(isinstance(plan[k],str) and re.fullmatch('[0-9a-f]{40}',plan[k]) for k in ('source_commit','source_tree')),'invalid source identity')
    require(len(plan['writes'])==4,'unexpected writes')
    payload={}
    for entry,(name,offset,limit) in zip(plan['writes'],WRITES):
        require(set(entry)=={'file','offset','bytes','sha256'} and entry['file']==name and type(entry['offset']) is int and entry['offset']==offset,'unsafe write address')
        data=regular_read(p/name,limit);payload[name]=data
        require(type(entry['bytes']) is int and len(data)==entry['bytes'] and hashlib.sha256(data).hexdigest()==entry['sha256'],'binary checksum mismatch')
    require(payload['otadata-erased.bin']==b'\xff'*8192,'initial otadata must be erased, not a custom boot selector')
    require(regular_read(p/'README.txt',4096)==README.encode(),'migration notice changed')
    app=inspect_app(payload['factory-recovery.bin'],plan['sdk'])
    require(app==plan['app'] and app['elf_sha256']==plan['elf_sha256'],'image/ELF identity mismatch')
    # Revalidate the actual table and boot header with the same strict profile
    # inspector; the copied inputs are never trusted because a JSON says PASS.
    import struct
    table=payload['partition-table.bin'];actual=[]
    require(len(table)%32==0,'truncated partition table')
    for pos in range(0,len(table),32):
        if struct.unpack_from('<H',table,pos)[0]==0x50aa:
            _,kind,sub,address,size,label,flags=struct.unpack_from('<HBBII16sI',table,pos)
            require(not flags,'encrypted/unknown partition flags')
            actual.append((kind,sub,address,size,label.rstrip(b'\0').decode()))
    require(actual==check_idf_build.OTA_PARTITIONS,'OTA layout mismatch')
    boot=payload['bootloader.bin'];require(len(boot)>=24 and boot[0]==0xe9 and boot[3]>>4==4 and struct.unpack_from('<H',boot,12)[0]==9,'invalid bootloader')
    hashed={'plan.json':regular_read(p/'plan.json',65536),'README.txt':README.encode(),**payload}
    require(regular_read(p/'SHA256SUMS',4096)==inventory(hashed),'bundle inventory checksum mismatch')
    return plan

def prepare(build,sdk,output,repo=None):
    b=Path(build).resolve();out=Path(output).resolve();root=Path(repo or Path(__file__).resolve().parents[1])
    require(not out.exists() and not out.is_symlink(),'output must be new; no overwrite')
    sha=git(root,'rev-parse','HEAD');tree,build_version=source(root,sha)
    check_idf_build.verify(b,'rev12-ota',sdk,write_report=False)
    context=json.loads(regular_read(b/'ota-local/ota_local_context.json',4096))
    require(context.get('provisioned') is False and context.get('private_key_present') is False,'this review migration tool requires unprovisioned build')
    app=inspect_app(regular_read(b/'tv4_hmi.bin',0x200000),sdk)
    description=json.loads(regular_read(b/'project_description.json',1024*1024))
    require(app['project']==description.get('project_name')=='tv4_hmi' and app['version']==description.get('project_version')==build_version,'stale app/project/source version')
    elf_sha=hashlib.sha256(regular_read(b/'tv4_hmi.elf',32*1024*1024)).hexdigest()
    require(elf_sha==app['elf_sha256'],'app/ELF digest mismatch')
    payload={}
    for original,name,limit in [('bootloader/bootloader.bin','bootloader.bin',0x8000),('partition_table/partition-table.bin','partition-table.bin',0x1000),('tv4_hmi.bin','factory-recovery.bin',0x200000)]:
        payload[name]=regular_read(b/original,limit)
    payload['otadata-erased.bin']=b'\xff'*8192
    writes=[dict(file=name,offset=offset,bytes=len(payload[name]),sha256=hashlib.sha256(payload[name]).hexdigest()) for name,offset,_ in WRITES]
    plan=dict(schema=1,kind='tv4-usb-migration-plan',source_commit=sha,source_tree=tree,sdk=sdk,profile='rev12-ota',layout='tv4-16m-ab-v1',flash_bytes=16777216,writes=writes,preserves_nvs=True,initial_boot='factory',erases_all_flash=False,device_access=False,hardware_migration_authorized=False,release_authorized=False,provisioned=False,app=app,elf_sha256=elf_sha)
    require(source(root,sha)==(tree,build_version),'source changed during packaging')
    out.parent.mkdir(parents=True,exist_ok=True)
    with tempfile.TemporaryDirectory(prefix='.tv4-migration-',dir=out.parent) as tmp:
        stage=Path(tmp)/'bundle';stage.mkdir()
        payload.update({'plan.json':encoded(plan),'README.txt':README.encode()})
        for name,data in payload.items():(stage/name).write_bytes(data)
        (stage/'SHA256SUMS').write_bytes(inventory(payload))
        verify(stage);stage.rename(out)
    return plan

if __name__=='__main__':
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('--build');p.add_argument('--sdk',choices=['5.3.2','6.1.1']);p.add_argument('--output');p.add_argument('--verify');a=p.parse_args()
    try:
        if a.verify:
            require(not (a.build or a.sdk or a.output),'verify is read-only and takes only a bundle path');result=verify(a.verify)
        else:
            require(a.build and a.sdk and a.output,'build/sdk/output required');result=prepare(a.build,a.sdk,a.output)
        print(json.dumps(result,indent=2))
    except (OSError,ValueError,KeyError,TypeError,subprocess.CalledProcessError) as e:raise SystemExit('Migration blocked: '+str(e))
