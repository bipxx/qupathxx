from pathlib import Path
import xml.etree.ElementTree as ET
import subprocess, shutil
root=Path.cwd()
cache=Path('C:/Users/HEQ/Projects/qupathxx/build/ci/gridfrontends167-package')
out=root/'build/ui-language'
runtime=Path('C:/Users/HEQ/Projects/qupathxx/build/consolidation/development/qupathxx')
ns={'m':'http://schemas.microsoft.com/developer/msbuild/2003'}
def release_def(name, kind):
    tree=ET.parse(cache/name)
    for group in tree.findall('m:ItemDefinitionGroup',ns):
        if "'Release|x64'" in group.attrib.get('Condition',''):
            return group.find('m:'+kind,ns)
    raise RuntimeError(name)
gui=release_def('qupathxx_gui.vcxproj','ClCompile')
includes=gui.find('m:AdditionalIncludeDirectories',ns).text.split(';')
includes=[str(root/'cpp/include')]+[p for p in includes if '%(' not in p]
args=['/nologo','/c','/std:c++17','/MD','/O2','/EHsc','/bigobj','/utf-8','/DNDEBUG','/DWIN32','/D_WINDOWS','/DIMGUI_USE_WCHAR32']
args += ['/I'+p for p in includes]
args += [str(root/'cpp/gui.cpp'),'/Fo'+str(out/'gui.obj')]
(out/'compile.rsp').write_text(subprocess.list2cmdline(args),encoding='utf8')
link=release_def('qupathxx_app.vcxproj','Link')
deps=link.find('m:AdditionalDependencies',ns).text.split(';')
deps=[str(cache/p) if not Path(p).is_absolute() and ('\\' in p or '/' in p) else p for p in deps if '%(' not in p]
args=['/NOLOGO','/OUT:'+str(out/'qupathxx_app.exe'),str(out/'gui.obj'),str(cache/'qupathxx_app.dir/Release/main.obj')]+deps
(out/'link.rsp').write_text(subprocess.list2cmdline(args),encoding='utf8')
vcvars=Path('C:/Program Files/Microsoft Visual Studio/2022/Community/VC/Auxiliary/Build/vcvars64.bat')
native=release_def('_native.vcxproj','Link')
pdeps=native.find('m:AdditionalDependencies',ns).text.split(';')
pdeps=[str(cache/p) if not Path(p).is_absolute() and ('\\' in p or '/' in p) else p for p in pdeps if '%(' not in p]
package=out/'runtime/qupathxx'
package.mkdir(parents=True,exist_ok=True)
pargs=['/NOLOGO','/DLL','/EXPORT:PyInit__native','/OUT:'+str(package/'_native.cp312-win_amd64.pyd'),str(out/'gui.obj')]+[str(p) for p in (cache/'_native.dir/Release').glob('*.obj')]+pdeps
(out/'python-link.rsp').write_text(subprocess.list2cmdline(pargs),encoding='utf8')
script='@echo off\ncall "'+str(vcvars)+'" >nul\ncl @"'+str(out/'compile.rsp')+'"\nif errorlevel 1 exit /b %errorlevel%\nlink @"'+str(out/'link.rsp')+'"\n'
script+='if errorlevel 1 exit /b %errorlevel%\nlink @\"'+str(out/'python-link.rsp')+'\"\n'
(out/'build.cmd').write_text(script)
with (out/'build.log').open('w') as log:
    result=subprocess.run(['cmd','/c',str(out/'build.cmd')],stdout=log,stderr=subprocess.STDOUT)
print((out/'build.log').read_text()[-6500:])
if result.returncode: raise SystemExit(result.returncode)
for p in runtime.glob('*.dll'): shutil.copy2(p,out/p.name)
shutil.copytree(runtime/'fonts',out/'fonts',dirs_exist_ok=True)
print('GUI-only build ready:',out)

for p in runtime.iterdir():
    if p.suffix in ['.py','.pyi','.dll'] or p.name=='py.typed': shutil.copy2(p,package/p.name)
shutil.copytree(runtime/'fonts',package/'fonts',dirs_exist_ok=True)
shutil.copy2(out/'qupathxx_app.exe',package/'qupathxx_app.exe')
