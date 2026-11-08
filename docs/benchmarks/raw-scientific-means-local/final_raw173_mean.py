from pathlib import Path
import subprocess,json,time,hashlib
r=Path(__file__).resolve().parents[2];out=r/'build/raw173-mean-final';out.mkdir(exist_ok=False)
source=r/'build/ci/scientific-grid-159';cmake=r/'.venv/Lib/site-packages/cmake/data/bin/cmake.exe';build=r/'build/ci/raw173-mean-windows'
linux='/mnt/c/Users/HEQ/Projects/qupathxx';cpp=linux+'/build/ci/scientific-grid-159/cpp/tests/raw_scientific_numeric_tests.cpp'
locked={p:hashlib.sha256((source/p).read_bytes()).hexdigest() for p in ['cpp/exact_weighted_mean.hpp','cpp/tests/raw_scientific_numeric_tests.cpp','cpp/tests/raw_scientific_mean_reference.hpp']}
commands=[('msvc-build',[str(cmake),'--build',str(build),'--config','Release','--parallel','2']),
('msvc-test',[str(cmake.with_name('ctest.exe')),'--test-dir',str(build),'-C','Release','--output-on-failure','--no-tests=error','--output-junit',str(out/'msvc-tests.xml')]),
('gcc-build',['wsl.exe','-d','Ubuntu','--','g++','-std=c++17','-O2','-Wall','-Wextra','-Werror','-pedantic','-DQUPATHXX_EXACT_MEAN_STANDALONE',cpp,'-o',linux+'/build/ci/raw173-mean-final']),
('gcc-test',['wsl.exe','-d','Ubuntu','--',linux+'/build/ci/raw173-mean-final']),
('sanitize-build',['wsl.exe','-d','Ubuntu','--','g++','-std=c++17','-O1','-g','-fsanitize=address,undefined','-fno-omit-frame-pointer','-Wall','-Wextra','-Werror','-pedantic','-DQUPATHXX_EXACT_MEAN_STANDALONE',cpp,'-o',linux+'/build/ci/raw173-mean-sanitize']),
('sanitize-test',['wsl.exe','-d','Ubuntu','--',linux+'/build/ci/raw173-mean-sanitize']),
('oracle',[str(r/'.venv/Scripts/python.exe'),'-I','-m','pytest',str(source/'tests/test_raw_mean_reference.py'),'-q','--override-ini','addopts=','--junitxml='+str(out/'oracle-tests.xml')])]
results=[]
for name,cmd in commands:
 assert all(hashlib.sha256((source/p).read_bytes()).hexdigest()==v for p,v in locked.items())
 print(name,flush=True);start=time.perf_counter()
 with (out/(name+'.log')).open('wb') as log:p=subprocess.run(cmd,stdout=log,stderr=subprocess.STDOUT,timeout=180)
 results.append(dict(name=name,command=cmd,exit=p.returncode,elapsed_seconds=time.perf_counter()-start))
 (out/'commands.json').write_bytes((json.dumps(dict(compiler_input_hashes=locked,commands=results),indent=2)+'\n').encode('utf-8'))
 print(name,'exit',p.returncode,flush=True)
 if p.returncode:
  print((out/(name+'.log')).read_text(encoding='utf-8',errors='replace')[-10000:]);raise SystemExit(p.returncode)
assert all(hashlib.sha256((source/p).read_bytes()).hexdigest()==v for p,v in locked.items())
