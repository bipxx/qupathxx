from pathlib import Path
import os,subprocess
root=Path(r'C:/Users/HEQ/Projects/qupathxx');source=root/'build/ci/scientific-grid-159'
env=os.environ.copy();env['CMAKE_GENERATOR']='Visual Studio 17 2022';env['CMAKE_BUILD_PARALLEL_LEVEL']='2';env['PATH']=str(root/'.venv/Scripts')+os.pathsep+env['PATH']
args=[r'C:/Users/HEQ/.local/bin/uv.exe','build',str(source),'--wheel','--no-build-isolation','--python',str(root/'.venv/Scripts/python.exe'),'--out-dir',str(root/'build/ci/gridfrontends167-first-wheels'),'-C','build-dir='+str(root/'build/ci/gridfrontends167-package')]
defines={
'QUPATHXX_BUILD_PYTHON':'ON','QUPATHXX_BUILD_GUI':'ON','QUPATHXX_BUILD_TESTS':'ON','QUPATHXX_BUILD_PROJECT':'ON','QUPATHXX_BUILD_INTERCHANGE':'OFF','QUPATHXX_BUILD_GEOS':'ON','QUPATHXX_BUILD_OPENSLIDE':'OFF','QUPATHXX_BUILD_TIFF':'OFF','QUPATHXX_BUILD_CLASSIFIER_TRAINING':'OFF',
'GEOS_ROOT':str(root/'build/deps/geos-sdk'),'GEOS_LICENSE_FILE':str(root/'build/deps/geos-sdk/share/licenses/geos/COPYING'),
'FETCHCONTENT_SOURCE_DIR_SQLITE':str(root/'build/deps/sqlite-amalgamation-3540100'),
'FETCHCONTENT_SOURCE_DIR_GLFW':str(root/'build/cp312-cp312-win_amd64/_deps/glfw-src'),
'FETCHCONTENT_SOURCE_DIR_IMGUI':str(root/'build/cp312-cp312-win_amd64/_deps/imgui-src')}
for key,value in defines.items():args+=['-C','cmake.define.'+key+'='+value]
log=root/'build/gridfrontends167-first-package.log'
with log.open('wb') as f: result=subprocess.run(args,env=env,stdout=f,stderr=subprocess.STDOUT)
print('gridfrontends167 first package exit',result.returncode,flush=True);raise SystemExit(result.returncode)
