"""Exact admitted OME provider and matching native overlay package, development only."""
import argparse
import hashlib
import json
from pathlib import Path
from stage_annotation_sources import Package, COMMON, ROOT
from stage_annotation_geometry import own_headers

sha = lambda data: hashlib.sha256(data).hexdigest()


def main():
    p=argparse.ArgumentParser(description=__doc__)
    for name in ["module","validation","native","cli","consumer","kit","expat","schema",
                 "base-package","base-admission","compiled-inputs","output"]:
        p.add_argument("--"+name,type=Path,required=True)
    p.add_argument("--write-inventory",action="store_true");a=p.parse_args()
    assert not a.output.exists(),"Preserve prior packages"
    inventory_path=ROOT/"tests/fixtures/manifest.json";inventory=json.loads(inventory_path.read_text(encoding="utf8"))
    pkg=Package(inventory,"OME2016-06 native vector ROI provider, ABI4 locked-state capability and matching native service/CLI/SDK; individual component licenses, exact sources/binaries, frozen FP04 SDK/core/runtime remains separate", "ome_roi_plugin")
    base_admission=json.loads(a.base_admission.read_text());base=json.loads((a.base_package/"SOURCE.json").read_text())
    assert base_admission["admission_exit"]==0
    for name in base_admission["members"]:
        if (name.startswith("source/third_party/") or name in ["licenses/qupathxx/LICENSE","licenses/nlohmann-json/LICENSE.MIT","licenses/sqlite/sqlite-LICENSE.txt",
               "lib/cmake/qpx_annotation_plugin/qpx_annotation_pluginConfig.cmake"]):
            pkg.borrow(name,a.base_package,base_admission)
    def own(name,path,kind="code",parents=("qupathxx-root-license",)):
        return pkg.add(name,path.read_bytes(),str(path.relative_to(ROOT)) if path.is_relative_to(ROOT) else str(path),parents,kind)
    def external(name,path,license,evidence,attribution,parents=()):
        data=path.read_bytes();ident="ome-roi-"+license.replace(" ","-")+"-"+sha(data)[:24]
        pkg.records.setdefault(ident,{"filename":path.name,"source":evidence,"bytes":len(data),"sha256":sha(data),
            "material":{"id":ident,"category":"admitted_open","kind":"code" if path.suffix in [".c",".h",".cmake",".in"] or path.name=="CMakeLists.txt" else "asset",
                        "license_id":license,"license_evidence":evidence,"attribution":attribution,"source_ids":list(parents)}})
        pkg._member(name,data,ident);return ident
    host=own_headers()
    compiled=json.loads(a.compiled_inputs.read_text())
    # Compiler input evidence is exact for native code/headers; test controllers
    # may have later fixture-only corrections with their own immutable receipts.
    for name in host: assert compiled["sources"][name]==sha((ROOT/name).read_bytes()),"Changed native source: "+name
    for name in host: own("source/"+name,ROOT/name)
    module_files=list(COMMON)+["plugins/format-common/annotation_xml.hpp","plugins/format-common/annotation_xml_expat.cmake"]+[x.relative_to(ROOT).as_posix() for x in (ROOT/"plugins/ome_roi").rglob("*") if x.is_file()]
    for name in sorted(module_files):
        if name in compiled["sources"]: assert compiled["sources"][name]==sha((ROOT/name).read_bytes()),name
        own("source/"+name,ROOT/name,"asset" if name.endswith((".md",".json")) else "code")
    for file in a.kit.rglob("*"):
        if file.is_file():own("source/format-plugin-kit/"+file.relative_to(a.kit).as_posix(),file)
    lock=json.loads((ROOT/"plugins/ome_roi/reference-lock.json").read_text())
    dependency_parents=[]
    for name,expected in lock["expat"]["members"].items():
        file=a.expat/name;data=file.read_bytes();assert sha(data)==expected["sha256"] and len(data)==expected["bytes"],name
        license="CC0-1.0" if name=="lib/siphash.h" else "MIT"
        dependency_parents.append(external("source/deps/expat/"+name,file,license,
            lock["expat"]["upstream"]+"/blob/"+lock["expat"]["commit"]+"/expat/"+name+"; COPYING and per-file notices",
            "Thai Open Source Software Center, Clark Cooper, Expat maintainers and per-file contributors"))
    external("licenses/expat/COPYING",a.expat/"COPYING","MIT","Expat exact upstream COPYING","Thai Open Source Software Center, Clark Cooper and Expat maintainers")
    external("licenses/expat/siphash-CC0-notice.h",a.expat/"lib/siphash.h","CC0-1.0","Exact SipHash header notices/SPDX","Per-file SipHash authors; notices retained")
    assert sha(a.schema.read_bytes())==lock["schema_reference"]["sha256"]
    schema_id=external("source/references/ome-2016-06.xsd",a.schema,"CC-BY-4.0",lock["schema_reference"]["url"]+"; "+lock["schema_reference"]["licensing_url"],lock["attribution"])
    extra=["examples/annotation_geometry_host/CMakeLists.txt","examples/annotation_plugin/CMakeLists.txt","examples/annotation_plugin/main.cpp",
           "cpp/tests/ome_roi_tests.cpp","cpp/tests/annotation_lock_test_module.cpp","cpp/tests/format_plugin_c_header_test.c",
           "tools/ci/check_ome_roi.py","tools/ci/check_imagej_roi.py","tools/ci/check_annotation_source_contract.py",
           "tools/ci/check_format_family_plugins.py","tools/ci/stage_ome_roi.py","tools/ci/stage_annotation_sources.py",
           "tools/ci/stage_annotation_geometry.py","tools/ci/check_material_staging.py","docs/benchmarks/ome-roi/README.md"]
    for name in extra:own("source/"+name,ROOT/name,"asset" if name.endswith(".md") else "code")
    own("source/LICENSE",ROOT/"LICENSE","notice")
    validation=json.loads(a.validation.read_text());assert validation["passed"] and validation["artifacts"][str(a.module.resolve())]==sha(a.module.read_bytes())
    for file in [a.cli]: assert validation["artifacts"][str(file.resolve())]==sha(file.read_bytes())
    descriptor=json.loads((a.module.parent/"format-plugins.json").read_text());entry=descriptor["plugins"][0]
    assert len(descriptor["plugins"])==1 and entry["abi"]==4 and entry["provider_id"]=="org.qupathxx.ome-roi" and entry["dependencies"]==[] and entry["module"]["sha256"]==sha(a.module.read_bytes())
    inputs={name:sha((ROOT/name).read_bytes()) for name in module_files}
    inputs.update({"format-plugin-kit/"+x.relative_to(a.kit).as_posix():sha(x.read_bytes()) for x in a.kit.rglob("*") if x.is_file()})
    inputs.update({"expat/"+name:row["sha256"] for name,row in lock["expat"]["members"].items()})
    module_parents=[pkg.members["source/"+n] for n in module_files]+dependency_parents+[schema_id]
    own(a.module.name,a.module,parents=module_parents)
    own("format-plugins.json",a.module.parent/"format-plugins.json","asset")
    host_parents=[pkg.members["source/"+n] for n in host]+[pkg.members[n] for n in pkg.members if n.startswith("source/third_party/")]
    own("native/annotation_native.lib",a.native,parents=host_parents)
    own("bin/qupathxx_format_import.exe",a.cli,parents=host_parents)
    own("bin/annotation_plugin_consumer.exe",a.consumer,parents=host_parents+[pkg.members["source/examples/annotation_plugin/main.cpp"]])
    notice='''OME ROI provider, ABI4 native service, CLI/SDK control and first-party tests/tools: qupathxx contributors, AGPL-3.0-only. Complete matching corresponding source/root license is included. AGPL obligations remain applicable.
Private unchanged Expat2.10.0: MIT, except lib/siphash.h CC0-1.0. All selected source/build inputs, full upstream notices and authors are retained. The module CMake sets /utf-8 and /MD; no upstream source modification. No test corpus/assets fetched.
This work is derived in part from the OME specification. Copyright (C) 2002-2016 Open Microscopy Environment
OME2016-06 core schema: CC BY 4.0, https://creativecommons.org/licenses/by/4.0/ . Exact unmodified schema included; this plugin implements a selected ROI profile and does not claim OME certification. Source/schema and primary terms: https://www.openmicroscopy.org/Schemas/OME/2016-06/ome.xsd and https://www.openmicroscopy.org/licensing/ . No OME screenshots/videos/images/models/data corpus included.
Native overlay interface dependencies retain separate terms: nlohmann-json3.12.0 MIT; SQLite3.54.1 public domain. Borrowed admitted headers/notices retain their exact material identities. They are not linked into the OME module.
The exact FP04 base SDK/core/runtime/source/notices remain a separate frozen product, not repackaged/requalified here. Windows compiler/CRT follow the recorded Windows x64 MSVC1944/compiler19.44.35225.0 Release /MD profile.
'''
    pkg.add("NOTICE.txt",notice.encode(),"Per-component source/distribution obligations",kind="notice")
    pkg.add("README.md",b'''# OME native annotation package

Read source/plugins/ome_roi/README.md for the selected ROI/plane/selection/lock
contract and source/docs/benchmarks/ome-roi/README.md for validation/scope.
Use this module with this matching ABI4 native service/header/CLI/SDK candidate.
Existing ABI1-3 providers keep their contracts; the FP04 base SDK stays separate.

Offline provider build: cmake -S source/plugins/ome_roi -B ome-build -A x64
-DQUPATHXX_FORMAT_PLUGIN_KIT_PREFIX=source/format-plugin-kit
-DQUPATHXX_OME_EXPAT_SOURCE_DIR=source/deps/expat . No network, Python or Java.
SDK consumer: cmake -S source/examples/annotation_plugin -B consumer -A x64
-DCMAKE_PREFIX_PATH="<this-package>;<frozen-FP04-SDK>" . Build Release /MD.
Native service source rebuild recipe: source/examples/annotation_geometry_host.
Exact per-file ownership/hashes are in PACKAGE.json. Uninstall removes only
unchanged owned members after checking active processes; retain user projects,
fixtures, evidence and frozen source/runtime products.
''',"Source-matched install/build/removal guidance",kind="asset")
    module_sha=sha(a.module.read_bytes());metadata={"schema":"qpx.ome-roi-delivery/1","providers":{"org.qupathxx.ome-roi":{"module_sha256":module_sha,"source_context_sha256":sha(json.dumps(inputs,sort_keys=True,separators=(",",":")).encode()),"source_inputs":inputs,"license_id":"AGPL-3.0-only","dependencies":{"expat2.10.0":"MIT AND CC0-1.0"},"schema_terms":"CC-BY-4.0","validation_sha256":sha(a.validation.read_bytes())}},
        "host_inputs":{n:sha((ROOT/n).read_bytes()) for n in host},"native_overlay_sha256":sha(a.native.read_bytes()),"cli_sha256":sha(a.cli.read_bytes()),"public_consumer_sha256":sha(a.consumer.read_bytes()),"base_sdk":base["base_sdk"],"host_changed":True,"scope":"New source/binary identity; previous ABI1-3 modules and all base SDK/runtime/wheel products stay frozen"}
    result=pkg.finish(a.output,"ome-roi-"+module_sha[:16],metadata,inventory_path,a.write_inventory)
    print(json.dumps({"files":result["files"],"admission_exit":result["admission_exit"],"output":str(a.output)}))


if __name__=="__main__": main()
