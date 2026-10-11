#!/usr/bin/env python3
"""Run portable host-side protocol, wake, poll-value and OTA guard tests."""
import argparse, os, subprocess, tempfile
from pathlib import Path
root=Path(__file__).resolve().parents[1]
p=argparse.ArgumentParser();p.add_argument('--cc',default=os.environ.get('CC','cc'));p.add_argument('--sanitize',action='store_true');a=p.parse_args()
with tempfile.TemporaryDirectory(prefix='smartwings-tests-') as d:
    for name in ('test_motor','test_motor_wake','test_poll_config','test_ota_guard'):
        exe=Path(d)/(name+('.exe' if os.name=='nt' else ''))
        cmd=[a.cc,'-std=c11','-Wall','-Wextra','-Werror','-I',str(root/'app'),'-I',str(root/'app/motor')]
        if a.sanitize:cmd+=['-fsanitize=address,undefined']
        if name in ('test_motor','test_motor_wake'):cmd+=[str(root/'app/motor/smartwings_motor.c')]
        subprocess.run(cmd+[str(root/'tests'/f'{name}.c'),'-o',str(exe)],check=True)
        subprocess.run([str(exe)],check=True)
        print(name+': PASS',flush=True)
