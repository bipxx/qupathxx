from pathlib import Path
import hashlib
import json
import shutil
import subprocess

ROOT=Path(__file__).resolve().parents[2];OUT=ROOT/'build/gui-fonts-282'
old=ROOT/'build/unicode-font-probe-277'
assert subprocess.check_output(['git','rev-parse','HEAD'],cwd=OUT/'inputs/imgui193',text=True).strip()=='b916186e66aa3c02b72c6ca17455516512ed8049'
assert not subprocess.check_output(['git','status','--porcelain'],cwd=OUT/'inputs/imgui193')
manifest=json.loads((old/'input-manifest.json').read_text())
font=old/'inputs/NotoSansCJKsc-Regular.otf'
assert hashlib.sha256(font.read_bytes()).hexdigest()==manifest['files'][0]['sha256']
target=OUT/'inputs/NotoSansCJKsc-Regular.otf';assert not target.exists();shutil.copyfile(font,target)
for name in ['imgui.h','imconfig.h','imgui-LICENSE.txt','LICENSE','README-third_party.md']:
    source=old/'inputs'/name
    if source.exists():
        dest=OUT/'inputs/legacy'/name;dest.parent.mkdir(exist_ok=True);shutil.copyfile(source,dest)
source=(old/'font_probe.cpp').read_text(encoding='utf-8')
source=source.replace('0xC5B4,0x1F52C','0xC5B4,0x2000B,0x1F52C,0x1F469')
(OUT/'legacy_probe.cpp').write_text(source,encoding='utf-8')
library=ROOT/'build/stream-header-integrity-279/final-products/Release/qupathxx_gui.lib'
retained=json.loads((ROOT/'build/stream-header-integrity-279/final-products.json').read_text())
entry=next(item for item in retained if item['path']=='Release/qupathxx_gui.lib')
assert hashlib.sha256(library.read_bytes()).hexdigest()==entry['sha256']
cmake=f'''cmake_minimum_required(VERSION 3.24)
project(qpx_dynamic_font_pilot LANGUAGES CXX)
add_executable(legacy_probe legacy_probe.cpp)
target_include_directories(legacy_probe PRIVATE "${{CMAKE_CURRENT_SOURCE_DIR}}/inputs/legacy")
target_link_libraries(legacy_probe PRIVATE "{library.as_posix()}")
add_executable(dynamic_probe dynamic_probe.cpp inputs/imgui193/imgui.cpp inputs/imgui193/imgui_draw.cpp inputs/imgui193/imgui_widgets.cpp inputs/imgui193/imgui_tables.cpp)
target_include_directories(dynamic_probe PRIVATE "${{CMAKE_CURRENT_SOURCE_DIR}}/inputs/imgui193")
foreach(target legacy_probe dynamic_probe)
  target_compile_features(${{target}} PRIVATE cxx_std_17)
  target_compile_definitions(${{target}} PRIVATE IMGUI_USE_WCHAR32)
  if(MSVC)
    target_compile_options(${{target}} PRIVATE /utf-8)
  endif()
endforeach()
'''
(OUT/'CMakeLists.txt').write_text(cmake,encoding='utf-8')
(OUT/'input-identity.json').write_text(json.dumps(dict(
    legacy_imgui_commit='4806a1924ff6181180bf5e4b8b79ab4394118875',legacy_library=str(library),legacy_library_sha256=entry['sha256'],
    legacy_native_source='3bd6591930baa64ae5826d8ba3e2efaee37e8376',legacy_wchar_bits=32,
    modern_imgui_tag='v1.93.0-docking',modern_annotated_tag_sha='4e02e84cbb85d48881388e7090c2dfa1708c02a1',modern_commit='b916186e66aa3c02b72c6ca17455516512ed8049',
    noto_source=manifest,application_changed=False,application_rebuilt=False,gl_context=False,gpu_memory_values_are_simulated_from_dimensions=True),indent=2)+'\n')
print('Prepared32-bit retained-library and pinned1.93 docking font pilots',flush=True)
