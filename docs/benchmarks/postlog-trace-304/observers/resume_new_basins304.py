from pathlib import Path
import importlib.util
import json
import subprocess
import numpy as np
root=Path(__file__).resolve().parents[2];work=root/'build/ci/scientific-grid-159';out=root/'build/postlog-trace-304';probe=out/'new-basins';build=probe/'build'
args=[str(root/'.venv/Lib/site-packages/cmake/data/bin/cmake.exe'),'--build',str(build),'--config','Release','--parallel','2']
with (out/'new-basins-build-repaired.log').open('x',encoding='utf-8') as stream:r=subprocess.run(args,stdout=stream,stderr=subprocess.STDOUT)
assert r.returncode==0
text=(root/'build/ci/check_new_basins304.py').read_text(encoding='utf-8');marker="spec=importlib.util.spec_from_file_location('new_basin_edges'";assert text.count(marker)==1
exec(compile(text[text.index(marker):],'new-basins-comparison-only','exec'),globals())
