"""Linux SDK bundling contract: real relocated ELF plus invalid-input rejection."""
import os
from pathlib import Path
import shutil
import subprocess
import sys

import pytest


pytestmark=pytest.mark.skipif(sys.platform!='linux' or not os.environ.get('QUPATHXX_GEOS_SDK'),reason='requires opt-in Linux GEOS SDK')


def configure(tmp_path,*,license=True,reader=None,static=False):
    sdk=Path(os.environ['QUPATHXX_GEOS_SDK']).resolve()
    helper=Path(__file__).resolve().parents[1]/'cmake/GeosRuntime.cmake'
    project=tmp_path/'source';project.mkdir()
    (project/'probe.cpp').write_text('#include <geos_c.h>\nint main() { return GEOSversion() ? 0 : 1; }\n',encoding='utf-8')
    targets=('add_library(GEOS::geos_c STATIC IMPORTED)\nadd_library(GEOS::geos STATIC IMPORTED)' if static else f'find_package(GEOS 3.12 CONFIG REQUIRED PATHS "{sdk}" NO_DEFAULT_PATH)')
    text=f'''cmake_minimum_required(VERSION 3.24)
project(BundleProbe LANGUAGES CXX)
{targets}
set(GEOS_LICENSE_FILE "{sdk/'share/licenses/geos/COPYING' if license else tmp_path/'missing-COPYING'}")
'''
    if reader:text+=f'set(CMAKE_READELF "{reader}")\n'
    text+=f'''include("{helper}")
add_executable(probe probe.cpp)
target_link_libraries(probe PRIVATE GEOS::geos_c)
set_target_properties(probe PROPERTIES INSTALL_RPATH "$ORIGIN")
install(TARGETS probe RUNTIME DESTINATION qupathxx)
'''
    (project/'CMakeLists.txt').write_text(text,encoding='utf-8')
    build=tmp_path/'build';prefix=tmp_path/'relocated package'
    done=subprocess.run(['cmake','-S',str(project),'-B',str(build),'-DCMAKE_BUILD_TYPE=Release',f'-DCMAKE_INSTALL_PREFIX={prefix}'],capture_output=True,text=True,timeout=60)
    return done,build,prefix,sdk


def test_bundled_geos_c_api_resolves_its_cpp_runtime_after_relocation(tmp_path):
    done,build,prefix,sdk=configure(tmp_path)
    assert done.returncode==0,done.stdout+done.stderr
    subprocess.run(['cmake','--build',str(build),'--parallel','2'],check=True,capture_output=True,timeout=120)
    subprocess.run(['cmake','--install',str(build)],check=True,capture_output=True,timeout=30)
    payload=prefix/'qupathxx';libraries=list(payload.glob('libgeos*.so*'))
    assert len(libraries)==2 and (payload/'libgeos_c.so.1').is_file()
    for library in libraries:assert library.read_bytes()==(sdk/'lib'/library.name).read_bytes()
    relocated=tmp_path/'another location';shutil.move(str(prefix),relocated)
    env=os.environ.copy();env.pop('LD_LIBRARY_PATH',None)
    ran=subprocess.run([str(relocated/'qupathxx/probe')],env=env,capture_output=True,timeout=30)
    assert ran.returncode==0,ran.stderr


def test_bundle_rejects_c_api_without_origin_runtime_path(tmp_path):
    reader=tmp_path/'empty-readelf';reader.write_text('#!/bin/sh\nexit 0\n',encoding='utf-8');reader.chmod(0o700)
    done,_,_,_=configure(tmp_path,reader=reader)
    assert done.returncode!=0 and 'requires an exact $ORIGIN' in done.stdout+done.stderr


def test_bundle_requires_matching_sdk_license(tmp_path):
    done,_,_,_=configure(tmp_path,license=False)
    assert done.returncode!=0 and 'requires GEOS_LICENSE_FILE' in done.stdout+done.stderr


def test_bundle_rejects_static_targets(tmp_path):
    done,_,_,_=configure(tmp_path,static=True)
    assert done.returncode!=0 and 'requires shared libraries' in done.stdout+done.stderr
