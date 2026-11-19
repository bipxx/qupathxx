"""Exact admitted annotation source packages; development packaging only.

Provider format interpretation and application behavior remain in C++. This
writer reuses admitted frozen native/kit/dependency members without rebuilding
or relabeling their licenses, then adds the exact independently qualified DLLs.
"""
import argparse
import hashlib
import json
from pathlib import Path
import subprocess
import sys
from stage_annotation_geometry import update_group
from check_material_staging import records

ROOT=Path(__file__).resolve().parents[2]
COMMON=("plugins/format-common/annotation_source.hpp","plugins/format-common/binary_input.hpp",
        "plugins/format-common/reader_input.hpp",
        "plugins/format-common/plugin_build.cmake","plugins/format-common/format_plugin_profile.hpp.in",
        "plugins/format-common/write_manifest.cmake")
PRODUCTS={"org.qupathxx.wkt-wkb":("plugins/wkt_wkb/CMakeLists.txt","plugins/wkt_wkb/wkt_wkb.cpp",
            "plugins/format-common/wkb_geometry.hpp"),
          "org.qupathxx.imagej-roi":("plugins/imagej_roi/CMakeLists.txt","plugins/imagej_roi/plugin.cpp",
            "plugins/imagej_roi/roi.hpp","plugins/imagej_roi/roi.cpp","plugins/imagej_roi/zip.cpp",
            "plugins/imagej_roi/zlib-source-lock.json")}
EXTRA=("plugins/wkt_wkb/README.md","plugins/wkt_wkb/reference-lock.json","plugins/imagej_roi/README.md",
       "plugins/imagej_roi/reference-lock.json","cpp/tests/wkt_wkb_tests.cpp","tools/ci/check_wkt_wkb.py",
       "tools/ci/check_imagej_roi.py","tools/ci/check_annotation_source_contract.py",
       "tools/ci/check_format_family_plugins.py","tools/ci/stage_annotation_sources.py",
       "tools/ci/stage_annotation_geometry.py","tools/ci/check_material_staging.py",
       "docs/benchmarks/wkt-wkb/README.md")
sha=lambda data:hashlib.sha256(data).hexdigest()


class Package:
    def __init__(self,inventory,scope,group_name="annotation_source_plugins"):
        self.inventory=inventory
        self.group_name=group_name
        self.group=inventory.get(group_name,{"scope":scope,"members":[],"deliveries":{}})
        self.records={r["material"]["id"]:r for r in self.group["members"]}
        self.known_records={r["material"]["id"]:r for r in records(inventory)}
        self.files={};self.members={};self.identities={}
    def add(self,name,data,origin,parents=("qupathxx-root-license",),kind="code"):
        ident="annotation-source-AGPL-3.0-only-"+sha(data)[:24]
        if ident in self.known_records:
            existing=self.known_records[ident]
            assert existing["sha256"]==sha(data) and existing["bytes"]==len(data) and existing["material"]["license_id"]=="AGPL-3.0-only"
        else:self.records.setdefault(ident,{"filename":Path(name).name,"source":origin,"bytes":len(data),"sha256":sha(data),
            "material":{"id":ident,"category":"admitted_open","kind":kind,"license_id":"AGPL-3.0-only",
            "license_evidence":"LICENSE; repository author declaration","attribution":"qupathxx contributors","source_ids":list(dict.fromkeys(parents))}})
        self._member(name,data,ident);return ident
    def _member(self,name,data,ident):
        assert name not in self.files,"Duplicate stage ownership: "+name
        self.files[name]=data;self.members[name]=ident
        self.identities[name]={"bytes":len(data),"sha256":sha(data),"material_id":ident}
    def borrow(self,name,parent,admission):
        record=admission["members"][name];data=(parent/name).read_bytes()
        assert len(data)==record["bytes"] and sha(data)==record["sha256"],"Changed frozen admitted member: "+name
        self._member(name,data,record["material_id"])
    def finish(self,output,delivery_id,metadata,inventory_path,write):
        self.add("SOURCE.json",(json.dumps(metadata,indent=2)+"\n").encode(),"Exact per-provider and reused-native identities",kind="asset")
        ownership={"schema":"qpx.annotation-package-members/1","owned_files":sorted([*self.files,"PACKAGE.json"]),
                   "members":dict(self.identities),"self":"PACKAGE.json (self hash in external admitted inventory)"}
        self.add("PACKAGE.json",(json.dumps(ownership,indent=2)+"\n").encode(),"Exact owned paths and member digests",kind="asset")
        self.group["members"]=list(self.records.values());self.group["deliveries"][delivery_id]={"providers":metadata["providers"],"members":self.members}
        if write:update_group(inventory_path,self.group,self.group_name)
        else:
            known={r["material"]["id"] for r in self.inventory.get(self.group_name,{}).get("members",[])}
            assert all(i in known or i not in self.records for i in self.members.values()),"New exact materials require inventory update"
        for name,data in self.files.items():
            path=output/name;path.parent.mkdir(parents=True,exist_ok=True);path.write_bytes(data)
        cmd=[sys.executable,str(ROOT/"tools/ci/check_material_staging.py"),"--stage",str(output),"--inventory",str(inventory_path)]
        x=subprocess.run(cmd,capture_output=True)
        result=dict(metadata,files=len(self.files),members=self.identities,admission_exit=x.returncode,admission_output=(x.stdout+x.stderr).decode("utf8",errors="replace"))
        (output.parent/(output.name+"-admission.json")).write_text(json.dumps(result,indent=2)+"\n",encoding="utf8")
        assert x.returncode==0,result["admission_output"]
        return result


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    for name in ["wkt-module","imagej-module","wkt-validation","imagej-validation","compiled-inputs",
                 "base-package","base-admission","sdk","consumer","output"]:
        parser.add_argument("--"+name,type=Path,required=True)
    parser.add_argument("--write-inventory",action="store_true");args=parser.parse_args()
    output=args.output.resolve();assert not output.exists(),"Use a fresh stage; preserve prior evidence"
    compiled=json.loads(args.compiled_inputs.read_text(encoding="utf8"))
    for name,digest in compiled["sources"].items():assert sha((ROOT/name).read_bytes())==digest,"Changed compiled source: "+name
    base=json.loads((args.base_package/"SOURCE.json").read_text(encoding="utf8"));admission=json.loads(args.base_admission.read_text(encoding="utf8"))
    assert admission["admission_exit"]==0
    for name,digest in base["host_inputs"].items():assert sha((ROOT/name).read_bytes())==digest,"Changed frozen native source: "+name
    assert sha((args.sdk/"lib/qupathxx/qupathxx_core.lib").read_bytes())==base["base_sdk"]["core_sha256"]
    assert sha((args.base_package/"native/annotation_native.lib").read_bytes())==compiled["native_overlay_sha256"]==base["overlay_sha256"]
    assert sha((args.base_package/"bin/qupathxx_format_import.exe").read_bytes())==compiled["frozen_cli_sha256"]==base["cli_sha256"]
    inventory_path=ROOT/"tests/fixtures/manifest.json";inventory=json.loads(inventory_path.read_text(encoding="utf8"))
    package=Package(inventory,"Source-matched optional native WKT/WKB and shared-producer ImageJ candidates, separate per-provider source/licenses; exact unchanged native SDK overlay/CLI reused, frozen base SDK/runtime/source/notices remain separate; no GUI/binding/wheel/publication")
    # Reuse exact admitted IDs/terms for native code, API kit and dependencies.
    # No new extraction, rebuild or MIT/SQLite/zlib/NIH relabeling is performed.
    def borrowed(name):
        return (name.startswith(("native/","bin/","lib/cmake/","licenses/","source/cpp/","source/third_party/",
                "source/format-plugin-kit/","source/deps/zlib/","source/examples/")) or name=="source/LICENSE" or
                name=="source/plugins/imagej_roi/licenses/ImageJ-LICENSE.txt")
    for name in admission["members"]:
        if borrowed(name):package.borrow(name,args.base_package,admission)
    for path in sorted(set(COMMON+EXTRA+sum(PRODUCTS.values(),()))):
        package.add("source/"+path,(ROOT/path).read_bytes(),path,kind="asset" if path.endswith((".md",".json")) else "code")
    providers={};descriptors=[]
    for module_path,validation_path,ident in [(args.wkt_module,args.wkt_validation,"org.qupathxx.wkt-wkb"),
                                              (args.imagej_module,args.imagej_validation,"org.qupathxx.imagej-roi")]:
        data=module_path.read_bytes();validation=json.loads(validation_path.read_text(encoding="utf8"));assert validation["passed"]
        assert validation["artifacts"][str(module_path.resolve())]==sha(data),"Unqualified module"
        descriptor=json.loads((module_path.parent/"format-plugins.json").read_text(encoding="utf8"))
        assert len(descriptor["plugins"])==1;entry=descriptor["plugins"][0]
        assert entry["provider_id"]==ident and entry["abi"]==3 and entry["domain"]=="annotation" and entry["dependencies"]==[] and entry["module"]["sha256"]==sha(data)
        assert entry["module"]["path"]==module_path.name
        paths=COMMON+PRODUCTS[ident];inputs={name:sha((ROOT/name).read_bytes()) for name in paths}
        for name,digest in inputs.items():assert compiled["sources"][name]==digest,name
        inputs.update({name[len("source/"):]:sha(package.files[name]) for name in package.files if name.startswith("source/format-plugin-kit/")})
        parents=[package.members["source/"+name] for name in paths]+[package.members[n] for n in package.members if n.startswith("source/format-plugin-kit/")]
        if ident.endswith("imagej-roi"):parents.extend(package.members[n] for n in package.members if n.startswith("source/deps/zlib/"))
        package.add(module_path.name,data,"Qualified independent provider "+ident,parents)
        context=sha(json.dumps(inputs,sort_keys=True,separators=(",",":")).encode())
        providers[ident]={"module_sha256":sha(data),"source_context_sha256":context,"source_inputs":inputs,
            "license_id":"AGPL-3.0-only","dependencies":{"private_prefixed_zlib1.3.2":"Zlib"} if ident.endswith("imagej-roi") else {},
            "validation_sha256":sha(validation_path.read_bytes()),"scope":"New source/binary identity; previous ImageJ products remain frozen"}
        descriptors.append(entry)
    package.add("format-plugins.json",(json.dumps({"schema":"qpx.format-plugins/1","plugins":sorted(descriptors,key=lambda e:e["provider_id"])},indent=2)+"\n").encode(),"Exact coexisting qualified annotation provider manifest",kind="asset")
    # Consumer is reused only with its source/header/config/native archive exact.
    assert (ROOT/"examples/annotation_plugin/main.cpp").read_bytes()==(args.base_package/"source/examples/annotation_plugin/main.cpp").read_bytes()
    consumer_evidence=json.loads((args.base_package.parent/"offline-runtime.json").read_text(encoding="utf8"))
    assert consumer_evidence["passed"] and consumer_evidence["public_sdk_consumer_sha256"]==sha(args.consumer.read_bytes()),"Changed qualified SDK consumer"
    package.add("bin/annotation_plugin_consumer.exe",args.consumer.read_bytes(),"Previously qualified exact-source public SDK consumer over unchanged native overlay",
                [package.members["source/examples/annotation_plugin/main.cpp"],package.members["native/annotation_native.lib"]])
    notice="""Independent WKT/WKB parser, shared ABI3 producer/value encoder, ImageJ parser, tests/tools: qupathxx contributors, AGPL-3.0-only. Exact corresponding new source and full root license are included; AGPL obligations remain applicable.
WKT/WKB: selected OGC06-103r4/v1.2.1 geometry encodings; schema/implementation terms referenced in source/plugins/wkt_wkb/reference-lock.json. OGC document is not redistributed. No GEOS reader, external codec, reference code/image/dataset, Java or Python is linked by this DLL. No OGC certification/patent grant is claimed.
ImageJ: its pinned public NIH author declaration is under licenses/imagej/. No Java runtime/code linked. Private prefixed unchanged zlib1.3.2: Zlib; exact source/license retained. PKWARE specification is referenced, not redistributed.
Reused exact native overlay/CLI: AGPL-3.0-only with the frozen matching sources; MIT nlohmann-json3.12.0 and public-domain SQLite3.54.1 interface source/notices remain separate component terms. All borrowed members keep their exact existing admitted material IDs.
Frozen FP04 base SDK/core/runtime and complete source archive/notices remain separate retained products under their actual licenses. This local source/module package does not rebuild/requalify or relicense that base. Windows compiler/CRT follow the exact recorded profile.
"""
    package.add("NOTICE.txt",notice.encode(),"Per-provider and retained-component obligations",kind="notice")
    guide="""# Native image-pixel annotation sources

WKT/WKB: source/plugins/wkt_wkb/README.md. Selecting this provider declares literal
2D image-pixel XY coordinates; geographic/physical conversion, dimensions/SRID
and empty shapes are outside the selected profile. Native frame/plane selection
and all geometry/application validation remain shared C++ authorities.
ImageJ: source/plugins/imagej_roi/README.md. Its current source uses the shared
producer/value support; new hashes/evidence are in SOURCE.json, separate from
the earlier frozen ImageJ packet. See source/docs/benchmarks/wkt-wkb/README.md.

Build either independent provider against source/format-plugin-kit. WKT/WKB
needs no other dependency. ImageJ additionally uses source/deps/zlib. These module
builds need no core/SDK/Python/Java or network downloads. Preserve the recorded
Windows x64 MSVC1944 compiler19.44.35225.0 Release /MD profile.

native/annotation_native.lib, its matching headers/config and bin CLI are exact
reused native products; use them together with the separately retained FP04 SDK.
The included config checks its frozen core SHA. Source rebuild recipe is under
source/examples/annotation_geometry_host and its native JSON/SQLite interface
sources/notices are under source/third_party/native-overlay. Those are not DLL
dependencies. Native host sources/archives were unchanged by this packet.

The public-only source/examples/annotation_plugin consumer and its reused
bin/annotation_plugin_consumer.exe use the same native APIs. Select either
provider in the root format-plugins.json. Supply the separate SDK runtime DLL
directory on PATH for included executables. The prior runtime/SDK/full source
archive/notices remain available separately at their exact recorded identity.

PACKAGE.json records owned paths/digests. Close active package users and verify
ownership before removing only those files; preserve user projects, images,
annotation inputs, results, reports and notes. No GUI/binding/wheel, release,
general geographic/QuPath equivalence or full catalogue closure is claimed.
"""
    package.add("README.md",guide.encode(),"Package-local native module/SDK/runtime/source/removal guide",kind="asset")
    metadata={"schema":"qpx.annotation-sources-package/1","providers":providers,"native_overlay_sha256":base["overlay_sha256"],"cli_sha256":base["cli_sha256"],
              "public_consumer_sha256":sha(args.consumer.read_bytes()),"frozen_host_inputs":base["host_inputs"],"frozen_host_source_sha256":base["host_source_sha256"],
              "base_sdk":base["base_sdk"],"retained_native_delivery_source_sha256":sha((args.base_package/"SOURCE.json").read_bytes()),"host_changed":False}
    delivery_id=sha(json.dumps({key:value["module_sha256"] for key,value in providers.items()},sort_keys=True).encode())
    result=package.finish(output,delivery_id,metadata,inventory_path,args.write_inventory)
    print(json.dumps({"admitted":True,"files":result["files"],"providers":{key:value["module_sha256"] for key,value in providers.items()},"host_changed":False}))


if __name__=="__main__":main()
