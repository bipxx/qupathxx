from pathlib import Path
import argparse,os,subprocess
ap=argparse.ArgumentParser();ap.add_argument('--output',default='scientific160-first-wheels');ap.add_argument('--log',default='scientific160-first-build.log');a=ap.parse_args()
root=Path(r'C:/Users/HEQ/Projects/qupathxx');source=root/'build/ci/scientific-grid-159'
env=os.environ.copy();env['CMAKE_GENERATOR']='Visual Studio 17 2022';env['CMAKE_BUILD_PARALLEL_LEVEL']='2';env['PATH']=str(root/'.venv/Scripts')+os.pathsep+env['PATH']
args=[r'C:/Users/HEQ/.local/bin/uv.exe','build',str(source),'--wheel','--no-build-isolation','--python',str(root/'.venv/Scripts/python.exe'),'--out-dir',str(root/'build/ci'/a.output),'-C','build-dir='+str(root/'build/ci/grid159-native')]
for name,value in {'QUPATHXX_BUILD_PYTHON':'ON','QUPATHXX_BUILD_GUI':'OFF','QUPATHXX_BUILD_TESTS':'ON','QUPATHXX_BUILD_PROJECT':'OFF','QUPATHXX_BUILD_INTERCHANGE':'OFF','QUPATHXX_BUILD_GEOS':'OFF','QUPATHXX_BUILD_OPENSLIDE':'OFF','QUPATHXX_BUILD_TIFF':'OFF','QUPATHXX_BUILD_CLASSIFIER_TRAINING':'OFF'}.items():args+=['-C','cmake.define.'+name+'='+value]
with (root/'build'/a.log).open('wb') as log:result=subprocess.run(args,env=env,stdout=log,stderr=subprocess.STDOUT)
print('scientific160 wheel build exit',result.returncode,flush=True);raise SystemExit(result.returncode)
