#!/usr/bin/env python3
"""Write SHA256SUMS for a directory without replacing an existing manifest."""
import argparse, hashlib
from pathlib import Path
p=argparse.ArgumentParser();p.add_argument('directory',type=Path);a=p.parse_args()
root=a.directory.resolve();out=root/'SHA256SUMS'
if not root.is_dir():raise SystemExit('Directory does not exist')
if out.exists():raise SystemExit('Refusing to replace existing SHA256SUMS')
lines=[]
for f in sorted(root.rglob('*')):
    if f.is_file():lines.append(hashlib.sha256(f.read_bytes()).hexdigest()+'  '+f.relative_to(root).as_posix())
out.write_text('\n'.join(lines)+'\n');print(out)
