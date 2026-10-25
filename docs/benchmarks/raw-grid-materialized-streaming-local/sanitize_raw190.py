from pathlib import Path
import json,subprocess,time,shlex
r=Path(__file__).resolve().parents[2];out=r/'build/raw190-sanitize';out.mkdir(exist_ok=False)
base='/mnt/c/Users/HEQ/Projects/qupathxx';source=base+'/build/ci/scientific-grid-159';build=base+'/build/ci/raw176-tiff-linux-native';binary=base+'/build/ci/raw190-sanitized-reader'
link=shlex.split((r/'build/ci/raw176-tiff-linux-native/CMakeFiles/qupathxx_resampling_tests.dir/link.txt').read_text(encoding='utf-8'));libraries=link[link.index('libqupathxx_core.a'):];libraries=[build+'/'+p if p.endswith('.a') and not p.startswith('/') else p for p in libraries]
units=['scientific_raw_sampling.cpp','raw_analysis.cpp','raw_analysis_records.cpp','raw_analysis_archive.cpp','raw_tables.cpp','scientific_raster.cpp','tests/scientific_raw_sampling_tests.cpp','tests/raw_scientific_numeric_tests.cpp','tests/scientific_grid_geometry_tests.cpp']
commands=[('build',['wsl.exe','-d','Ubuntu','--','g++','-std=c++17','-O1','-g','-fsanitize=address,undefined','-fno-omit-frame-pointer','-Wall','-Wextra','-Werror','-Wno-misleading-indentation','-pedantic','-DQUPATHXX_HAS_INTERCHANGE=1','-DQUPATHXX_HAS_PROJECT=1','-DQUPATHXX_HAS_TIFF=1','-I',source+'/cpp/include',*[source+'/cpp/'+p for p in units],base+'/build/raw190-sanitize-driver.cpp',*libraries,'-pthread','-o',binary]),('tests',['wsl.exe','-d','Ubuntu','--','env','ASAN_OPTIONS=detect_leaks=1:detect_stack_use_after_return=1','UBSAN_OPTIONS=halt_on_error=1',binary])]
results=[]
for name,cmd in commands:
 print('Selective raw/raster sanitizer',name,flush=True);start=time.perf_counter()
 with (out/(name+'.log')).open('wb') as log:p=subprocess.run(cmd,stdout=log,stderr=subprocess.STDOUT,timeout=300)
 results.append(dict(name=name,command=cmd,exit=p.returncode,elapsed_seconds=time.perf_counter()-start));(out/'commands.json').write_bytes((json.dumps(results,indent=2)+'\n').encode('utf-8'))
 print(name,'exit',p.returncode,flush=True)
 if p.returncode:print((out/(name+'.log')).read_text(encoding='utf-8',errors='replace')[-12000:]);raise SystemExit(p.returncode)
