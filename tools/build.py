#!/usr/bin/env python3
"""Generate/build Q17 offline; never flash or overwrite tested release images."""
import argparse, hashlib, json, os, re, shutil, subprocess
from pathlib import Path
root=Path(__file__).resolve().parents[1]
p=argparse.ArgumentParser();p.add_argument('--config',type=Path,default=root/'tools/toolchain.local.json');a=p.parse_args();cfg=json.loads(a.config.read_text())
build=root/'build'
if build.exists():raise SystemExit('Refusing to overwrite build/: move it aside first')
build.mkdir();art=build/'artifacts';art.mkdir();env=os.environ.copy();env.update(ARM_GCC_DIR=cfg['gcc'],NINJA_EXE_PATH=cfg['ninja'],POST_BUILD_EXE=cfg['commander'])
def run(cmd,log,cwd=None):
    print('Running '+log,flush=True)
    with (build/log).open('w') as f:subprocess.run([str(x) for x in cmd],cwd=cwd,env=env,stdout=f,stderr=subprocess.STDOUT,check=True)
for folder,name in [('bootloader','smartwings_bootloader'),('app','smartwings_app')]:
    project=build/folder;shutil.copytree(root/folder,project)
    if folder=='app':
        zp=project/'config/zcl/zcl_config.zap';z=json.loads(zp.read_text())
        for package in z['package']:
            if package['path'].startswith('${SDK_ROOT}/'):
                package['path']=str(Path(cfg['sdk'])/package['path'][len('${SDK_ROOT}/'):])
        zp.write_text(json.dumps(z,indent=2)+'\n')
    run(cfg['slc']+['generate','--sdk-package-path='+cfg['sdk'],'-p',project/(name+'.slcp'),'--tool-path='+cfg['zap'],'-d',project,'-o','cmake','-tlcn','gcc','--no-daemon','--generator-timeout=120'],folder+'-generate.log')
    run([cfg['cmake'],'--workflow','--preset','project'],folder+'-build.log',project/'cmake_gcc')
    base=project/'cmake_gcc/build/base'
    readelf=Path(cfg['gcc'])/'bin'/('arm-none-eabi-readelf.exe' if os.name=='nt' else 'arm-none-eabi-readelf')
    loads=subprocess.check_output([str(readelf),'-lW',str(base/(name+'.out'))],text=True)
    lo,hi=(0,0x4000) if folder=='bootloader' else (0x4000,0x5c000)
    found=False
    for ln in loads.splitlines():
        fields=ln.split()
        if fields and fields[0]=='LOAD':
            addr,size=int(fields[3],16),int(fields[4],16)
            if size and addr<0xc0000:assert lo<=addr and addr+size<=hi,(folder,hex(addr),size);found=True
    assert found
    for ext in ('out','map','bin','hex','s37'):shutil.copy2(base/(name+'.'+ext),art/(folder+'.'+('elf' if ext=='out' else ext)))
if not re.search(r'NVM3KEY_SW_IDLE_POLL_INTERVAL_MS_1 \(NVM3KEY_DOMAIN_ZIGBEE \| 0xB000\)',(build/'app/autogen/zap-tokens.h').read_text()):raise SystemExit('Poll token changed')
if not re.search(r'NVM3KEY_SW_SAVED_COVERING_MODE_1 \(NVM3KEY_DOMAIN_ZIGBEE \| 0xB001\)',(build/'app/autogen/zap-tokens.h').read_text()):raise SystemExit('Mode token changed')
run([cfg['commander'],'gbl','create',art/'app.gbl','--app',art/'app.elf','--device','EFR32MG21A020F768IM32'],'gbl-create.log')
run([cfg['commander'],'ota','create','--upgrade-image',art/'app.gbl','--firmware-version','17','--manufacturer-id','0xFFFE','--image-type','0x6AAD','--min-hw','1','--max-hw','1','--string','TubesZBs Smartwings Q','--outfile',art/'app.ota'],'ota-create.log')
result={}
for folder,file in [('app','smartwings-Q17-app.bin'),('bootloader','smartwings-bootloader.bin')]:
    actual=hashlib.sha256((art/(folder+'.bin')).read_bytes()).hexdigest();expected=hashlib.sha256((root/'firmware/Q17'/file).read_bytes()).hexdigest()
    result[folder]={'sha256':actual,'matches_tested_release':actual==expected}
(art/'reproduction.json').write_text(json.dumps(result,indent=2)+'\n');print(json.dumps(result,indent=2))
