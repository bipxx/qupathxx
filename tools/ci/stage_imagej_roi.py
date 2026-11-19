"""Stage exact admitted ImageJ module/source and a separate ABI3 native overlay.

Development packaging only; no product format or workflow authority in Python.
"""
import argparse
import hashlib
import json
from pathlib import Path
import re
import subprocess
import sys
import tarfile
import zipfile
from stage_annotation_geometry import own_headers, update_group, BASE_CORE, JSON_ARCHIVE, SQLITE_ARCHIVE

ROOT=Path(__file__).resolve().parents[2]
HOST=("cpp/format_plugin_loader.cpp","cpp/format_plugin_runtime.cpp","cpp/format_import.cpp",
      "cpp/format_import_main.cpp","cpp/annotation_geometry_value.cpp")
PRODUCT=("plugins/imagej_roi/CMakeLists.txt","plugins/imagej_roi/plugin.cpp","plugins/imagej_roi/roi.cpp",
         "plugins/imagej_roi/roi.hpp","plugins/imagej_roi/zip.cpp","plugins/format-common/binary_input.hpp","plugins/format-common/annotation_source.hpp",
         "plugins/format-common/plugin_build.cmake","plugins/format-common/format_plugin_profile.hpp.in",
         "plugins/format-common/write_manifest.cmake","plugins/imagej_roi/zlib-source-lock.json")
EXTRA=("plugins/imagej_roi/README.md","plugins/imagej_roi/reference-lock.json",
       "cpp/tests/imagej_roi_tests.cpp","cpp/tests/annotation_source_tests.cpp","cpp/tests/annotation_source_test_module.cpp",
       "cpp/tests/format_plugin_c_header_test.c","tools/ci/check_imagej_roi.py","tools/ci/check_annotation_source_contract.py",
       "tools/ci/check_format_family_plugins.py","tools/ci/stage_imagej_roi.py","tools/ci/stage_annotation_geometry.py",
       "tools/ci/check_material_staging.py","examples/annotation_plugin/CMakeLists.txt","examples/annotation_plugin/main.cpp",
       "examples/annotation_geometry_host/CMakeLists.txt","docs/benchmarks/imagej-roi/README.md")
sha=lambda b:hashlib.sha256(b).hexdigest()


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    for name in ["module","overlay","cli","validation","compiled-inputs","kit","sdk","zlib","output"]:
        parser.add_argument("--"+name,type=Path,required=True)
    parser.add_argument("--write-inventory",action="store_true")
    args=parser.parse_args()
    validation=json.loads(args.validation.read_text(encoding="utf8"));assert validation["passed"]
    for path in [args.module,args.cli]:assert validation["artifacts"][str(path.resolve())]==sha(path.read_bytes()),"Unqualified artifact"
    compiled=json.loads(args.compiled_inputs.read_text(encoding="utf8"))
    for path,digest in compiled["sources"].items():assert sha((ROOT/path).read_bytes())==digest,"Changed compiled input: "+path
    module=args.module.read_bytes();overlay=args.overlay.read_bytes();assert compiled["artifacts"]["overlay"]==sha(overlay)
    assert compiled["artifacts"]["module"]==sha(module) and compiled["artifacts"]["cli"]==sha(args.cli.read_bytes())
    assert sha((args.sdk/"lib/qupathxx/qupathxx_core.lib").read_bytes())==BASE_CORE
    base=json.loads((ROOT/"build/format-family-plugins/results.json").read_text(encoding="utf8"))
    assert sha(Path(base["native_source_snapshot"]["archive"]).read_bytes())==base["native_source_snapshot"]["archive_sha256"]
    descriptor_path=args.module.parent/"format-plugins.json";descriptor=descriptor_path.read_bytes();item=json.loads(descriptor)["plugins"][0]
    assert item["provider_id"]=="org.qupathxx.imagej-roi" and item["abi"]==3 and item["module"]["sha256"]==sha(module) and item["dependencies"]==[]
    output=args.output.resolve();assert not output.exists(),"Preserve prior staged evidence; use a fresh destination"
    inventory_path=ROOT/"tests/fixtures/manifest.json";inventory=json.loads(inventory_path.read_text(encoding="utf8"))
    group=inventory.get("imagej_roi_plugin",{"scope":"Exact local optional Windows ImageJ ROI/ZIP module/ABI3 source kit and matching separate native overlay; no base SDK/runtime repack, GUI, bindings, wheel or publication", "members":[],"deliveries":{}})
    records={row["material"]["id"]:row for row in group["members"]};files={};members={};identities={}
    def add(name,data,origin,parents=("qupathxx-root-license",),kind="code",license_id="AGPL-3.0-only",evidence="LICENSE; repository author declaration",authors="qupathxx contributors"):
        ident="imagej-roi-"+license_id+"-"+sha(data)[:24]
        record={"filename":Path(name).name,"source":origin,"bytes":len(data),"sha256":sha(data),"material":{"id":ident,"category":"admitted_open","kind":kind,"license_id":license_id,"license_evidence":evidence,"attribution":authors,"source_ids":list(dict.fromkeys(parents))}}
        records.setdefault(ident,record);files[name]=data;members[name]=ident;identities[name]={"bytes":len(data),"sha256":sha(data),"material_id":ident};return ident
    headers=own_headers(HOST);host_hashes={path:sha((ROOT/path).read_bytes()) for path in headers};product_hashes={path:sha((ROOT/path).read_bytes()) for path in PRODUCT}
    for path in sorted(set(PRODUCT+EXTRA+tuple(headers))):add("source/"+path,(ROOT/path).read_bytes(),path,kind="asset" if path.endswith((".md",".json")) else "code")
    for name in ["source/LICENSE","licenses/qupathxx/LICENSE"]:add(name,(ROOT/"LICENSE").read_bytes(),"LICENSE",kind="notice")
    imagej_license=(ROOT/"plugins/imagej_roi/licenses/ImageJ-LICENSE.txt").read_bytes()
    assert sha(imagej_license)=="618725ff649e43605de0fec0a778b4aad24b28c6be9b3cde16295764022f668c"
    for name in ["licenses/imagej/ImageJ-LICENSE.txt","source/plugins/imagej_roi/licenses/ImageJ-LICENSE.txt"]:
        add(name,imagej_license,"Pinned official ImageJ root LICENSE at a376e43fad41a7337f7f01433c9673ed376b40e6",(),"notice","LicenseRef-ImageJ-Public-Domain","Exact NIH federal-duty public-domain author declaration","National Institutes of Health")
    kit_ids=[]
    for path in ["include/qupathxx/reader_plugin.h","include/qupathxx/format_plugin.h","share/qupathxx/format-plugin/profile.cmake","share/qupathxx/format-plugin/LICENSE"]:
        data=(args.kit/path).read_bytes();kit_ids.append(add("source/format-plugin-kit/"+path,data,"Matching ABI3 kit "+path,kind="notice" if path.endswith("LICENSE") else "code"))
    assert (args.kit/"include/qupathxx/format_plugin.h").read_bytes()==(ROOT/"cpp/include/qupathxx/format_plugin.h").read_bytes()
    codec_ids=[];lock=json.loads((ROOT/"plugins/imagej_roi/zlib-source-lock.json").read_text())
    license_path="licenses/zlib/LICENSE"
    notice=add(license_path,(args.zlib/"LICENSE").read_bytes(),"Unmodified admitted zlib1.3.2 LICENSE",(),"notice","Zlib",license_path,"Jean-loup Gailly and Mark Adler")
    for member,record in lock["members"].items():
        data=(args.zlib/member).read_bytes();assert sha(data)==record["sha256"] and len(data)==record["bytes"]
        codec_ids.append(add("source/deps/zlib/"+member,data,"Pinned zlib1.3.2 source archive "+lock["upstream_archive_sha256"],(notice,),"notice" if member=="LICENSE" else "code","Zlib",license_path,"Jean-loup Gailly and Mark Adler; zlib contributors"))
    dependency_ids=[];json_archive=ROOT/"build/deps/json-3.12.0.tar.xz";sqlite_archive=ROOT/"build/deps/sqlite-amalgamation-3540100.zip"
    assert sha(json_archive.read_bytes())==JSON_ARCHIVE and sha(sqlite_archive.read_bytes())==SQLITE_ARCHIVE
    with tarfile.open(json_archive) as archive:
        notice=add("licenses/nlohmann-json/LICENSE.MIT",archive.extractfile("json/LICENSE.MIT").read(),"Pinned MIT json3.12.0 archive",(),"notice","MIT","licenses/nlohmann-json/LICENSE.MIT","Niels Lohmann")
        for member in archive.getmembers():
            if member.isfile() and member.name.startswith("json/include/nlohmann/"):
                rel=member.name[len("json/include/"):];data=archive.extractfile(member).read();assert data==(ROOT/"build/ci/gridfrontends167-package/_deps/json-src/include"/rel).read_bytes();assert b"SPDX-License-Identifier: MIT" in data[:4096]
                authors="; ".join(re.findall(r"SPDX-FileCopyrightText: ([^\r\n]+)",data.decode("utf8")))
                dependency_ids.append(add("source/third_party/native-overlay/json/include/"+rel,data,"Exact pinned MIT native JSON header",(notice,),"code","MIT","licenses/nlohmann-json/LICENSE.MIT; header declarations",authors))
    with zipfile.ZipFile(sqlite_archive) as archive:
        data=archive.read("sqlite-amalgamation-3540100/sqlite3.h");assert data==(ROOT/"build/deps/sqlite-amalgamation-3540100/sqlite3.h").read_bytes()
        notice=add("licenses/sqlite/sqlite-LICENSE.txt",(args.sdk/"share/qupathxx/licenses/sqlite/sqlite-LICENSE.txt").read_bytes(),"Exact retained SQLite3.54.1 author declaration",(),"notice","blessing","licenses/sqlite/sqlite-LICENSE.txt; sqlite3.h","SQLite authors")
        dependency_ids.append(add("source/third_party/native-overlay/sqlite/sqlite3.h",data,"Pinned SQLite3.54.1 public interface",(notice,),"code","blessing","licenses/sqlite/sqlite-LICENSE.txt; sqlite3.h","SQLite authors"))
    module_id=add(args.module.name,module,"Qualified independent ImageJ ABI3 provider with private prefixed zlib",[members["source/"+path] for path in PRODUCT]+kit_ids+codec_ids)
    add("format-plugins.json",descriptor,"Exact qualified provider manifest",(module_id,),"asset")
    overlay_id=add("native/annotation_native.lib",overlay,"Source-matched native services over the separate frozen SDK",[members["source/"+path] for path in headers]+dependency_ids)
    cli_id=add("bin/qupathxx_format_import.exe",args.cli.read_bytes(),"Qualified thin native CLI; runtime supplied by the separate frozen SDK",(overlay_id,))
    config='''include(CMakeFindDependencyMacro)
find_dependency(qupathxx 0.1.0 CONFIG COMPONENTS core)
if(NOT WIN32 OR NOT CMAKE_SIZEOF_VOID_P EQUAL 8 OR NOT MSVC_VERSION STREQUAL "1944" OR NOT CMAKE_CXX_COMPILER_VERSION STREQUAL "19.44.35225.0")
 message(FATAL_ERROR "Use the recorded Windows x64 MSVC1944 compiler19.44.35225.0")
endif()
if(CMAKE_CONFIGURATION_TYPES)
 set(CMAKE_CONFIGURATION_TYPES Release CACHE STRING "Supported annotation configuration" FORCE)
elseif(NOT CMAKE_BUILD_TYPE STREQUAL "Release")
 message(FATAL_ERROR "The annotation overlay requires Release /MD")
endif()
get_target_property(base_core qupathxx::core IMPORTED_LOCATION_RELEASE)
file(SHA256 "${base_core}" base_sha)
if(NOT base_sha STREQUAL "'''+BASE_CORE+'''")
 message(FATAL_ERROR "Use the exact frozen FP04 SDK/core")
endif()
get_filename_component(package "${CMAKE_CURRENT_LIST_DIR}/../../.." ABSOLUTE)
if(NOT TARGET qpx_annotation_plugin::native)
 add_library(qpx_annotation_plugin::native STATIC IMPORTED)
 set_target_properties(qpx_annotation_plugin::native PROPERTIES IMPORTED_LOCATION "${package}/native/annotation_native.lib" INTERFACE_INCLUDE_DIRECTORIES "${package}/source/cpp/include" INTERFACE_LINK_LIBRARIES "qupathxx::core" INTERFACE_COMPILE_FEATURES cxx_std_17)
endif()
'''
    add("lib/cmake/qpx_annotation_plugin/qpx_annotation_pluginConfig.cmake",config.encode(),"Generated exact-base ABI3 overlay config",(overlay_id,))
    guide="""# Native ImageJ annotation package

See source/plugins/imagej_roi/README.md for the representable ROI/ZIP contract.
Build that optional provider with its included source/format-plugin-kit and
source/deps/zlib; no downloads, Java, Python or native SDK are needed by the DLL.
Matching source/cpp/include and native/annotation_native.lib form a separate
ABI3 native SDK overlay. Configure source/examples/annotation_plugin with this
package and the exact retained FP04 SDK on CMAKE_PREFIX_PATH. Its config verifies
the frozen core identity. Build source/examples/annotation_geometry_host for a
source rebuild of these native services/CLI; exact JSON/SQLite interface source
and notices are under source/third_party/native-overlay. The base SDK/core,
runtime DLLs, corresponding source archive and notices remain separate retained
products. Do not substitute only a header in an older SDK.

bin/qupathxx_format_import.exe is the qualified thin CLI; provide its separate
frozen SDK runtime DLL directory on PATH. Keep DLL/manifest together and use
--format-plugins ABSOLUTE_MANIFEST. All scientific/application interpretation,
source guards, planes, preview/apply/persistence and undo live in shared C++.

This is a local Windows x64 engineering delivery, not a publication, a general
ImageJ/QuPath equivalence claim or a full catalogue closure. Close active module
users before removing only manifest-owned package files. Preserve project/image/
result/note files and the separately installed SDK. Applicable AGPL source and
notice obligations accompany the actual code; plugin separation does not waive
any dependency's terms. Exact source/binary identities are in SOURCE.json.
"""
    add("README.md",guide.encode(),"Package-local use/source/removal guide",kind="asset")
    notice="""qupathxx provider/ABI3/native services/tests/tools/examples: AGPL-3.0-only, qupathxx contributors; exact corresponding source under source/ and full license under licenses/qupathxx/.
ImageJ layout references: official NIH federal-duty public-domain declaration in licenses/imagej/; pinned reference-lock.json. No Java runtime/code linked.
Private unmodified prefixed static zlib1.3.2: Zlib, Jean-loup Gailly/Mark Adler/contributors; exact source and license included; preserve notices and identify any modifications accurately.
Native overlay JSON3.12.0 headers: MIT, Niels Lohmann and per-header contributors; exact sources/notices included.
Native overlay SQLite3.54.1 public interface: public domain author declaration/blessing; exact source/notice included.
PKWARE ZIP6.3.10 documentation is referenced, not redistributed; no encryption/proprietary codec is included.
Separate frozen FP04 SDK/core/runtime retain their own complete source/dependency/notice receipt; no base SDK/runtime is repackaged or requalified here.
"""
    add("NOTICE.txt",notice.encode(),"Component-specific source/notice obligations",(module_id,overlay_id),"notice")
    product_context=sha(json.dumps(product_hashes,sort_keys=True,separators=(",",":")).encode());host_context=sha(json.dumps(host_hashes,sort_keys=True,separators=(",",":")).encode())
    provenance={"schema":"qpx.imagej-roi-package/1","module_sha256":sha(module),"overlay_sha256":sha(overlay),"cli_sha256":sha(args.cli.read_bytes()),"product_source_sha256":product_context,"product_inputs":product_hashes,"host_source_sha256":host_context,"host_inputs":host_hashes,"validation_sha256":sha(args.validation.read_bytes()),"base_sdk":{"core_sha256":BASE_CORE,"source_archive":Path(base["native_source_snapshot"]["archive"]).name,"source_archive_sha256":base["native_source_snapshot"]["archive_sha256"],"scope":"Separate frozen product, source and notices; not repackaged/requalified"},"components":{"module":"AGPL-3.0-only","zlib":"Zlib","native_overlay":"AGPL-3.0-only","native_json":"MIT","native_sqlite":"blessing","schema_reference":"LicenseRef-ImageJ-Public-Domain"}}
    add("SOURCE.json",(json.dumps(provenance,indent=2)+"\n").encode(),"Exact component source/binary identities",(module_id,overlay_id,cli_id),"asset")
    ownership={"schema":"qpx.annotation-package-members/1","owned_files":sorted([*files,"PACKAGE.json"]),"members":dict(identities),"self":"PACKAGE.json (self hash is in the external admitted delivery inventory)"}
    add("PACKAGE.json",(json.dumps(ownership,indent=2)+"\n").encode(),"Exact package ownership and member digest table",(module_id,overlay_id),"asset")
    group["members"]=list(records.values());group["deliveries"][sha(module)]={"product_source_sha256":product_context,"host_source_sha256":host_context,"members":members}
    if args.write_inventory:update_group(inventory_path,group,"imagej_roi_plugin")
    else:assert all(i in {r["material"]["id"] for r in inventory.get("imagej_roi_plugin",{}).get("members",[])} for i in members.values()),"New exact materials need a scoped inventory update"
    for name,data in files.items():
        path=output/name;path.parent.mkdir(parents=True,exist_ok=True);path.write_bytes(data)
    cmd=[sys.executable,str(ROOT/"tools/ci/check_material_staging.py"),"--stage",str(output),"--inventory",str(inventory_path)];r=subprocess.run(cmd,capture_output=True)
    record=dict(provenance,members=identities,files=len(files),admission_exit=r.returncode,admission_output=(r.stdout+r.stderr).decode("utf8",errors="replace"))
    (output.parent/(output.name+"-admission.json")).write_text(json.dumps(record,indent=2)+"\n",encoding="utf8");assert r.returncode==0,record["admission_output"]
    print(json.dumps({"admitted":True,"files":len(files),"module_sha256":sha(module),"overlay_sha256":sha(overlay)}))


if __name__=="__main__":main()
