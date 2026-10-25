from pathlib import Path
import subprocess,time,json
r=Path(__file__).resolve().parents[2];out=r/'build/raw174-sanitize';out.mkdir(exist_ok=False)
base='/mnt/c/Users/HEQ/Projects/qupathxx';source=base+'/build/ci/scientific-grid-159';binary=base+'/build/ci/raw174-sanitized-reader'
commands=[('build',['wsl.exe','-d','Ubuntu','--','g++','-std=c++17','-O1','-g','-fsanitize=address,undefined','-fno-omit-frame-pointer','-Wall','-Wextra','-Werror','-pedantic','-I',source+'/cpp/include',source+'/cpp/scientific_raw_sampling.cpp',source+'/cpp/tests/scientific_raw_sampling_tests.cpp',source+'/cpp/tests/raw_scientific_numeric_tests.cpp',base+'/build/raw174-sanitize-driver.cpp',base+'/build/ci/raw174-linux-native/libqupathxx_core.a','-pthread','-o',binary]),('tests',['wsl.exe','-d','Ubuntu','--',binary])]
results=[]
for name,cmd in commands:
 print('Raw reader sanitizer',name,flush=True);start=time.perf_counter()
 with (out/(name+'.log')).open('wb') as log:p=subprocess.run(cmd,stdout=log,stderr=subprocess.STDOUT,timeout=180)
 results.append(dict(name=name,command=cmd,exit=p.returncode,elapsed_seconds=time.perf_counter()-start));(out/'commands.json').write_bytes((json.dumps(results,indent=2)+'\n').encode('utf-8'))
 print(name,'exit',p.returncode,flush=True)
 if p.returncode:print((out/(name+'.log')).read_text(encoding='utf-8',errors='replace')[-12000:]);raise SystemExit(p.returncode)
