"""Independent authored WKT/WKB fixtures and native interoperability checks.

Development oracles/control only. Python never decodes user geometry or owns
application plane, state, identity, measurement or publication behavior.
"""
from pathlib import Path
import argparse
import ctypes
import hashlib
import json
import os
import shutil
import struct
import subprocess
from check_format_family_plugins import quantitative_tiff
from check_annotation_source_contract import volume_tiff
from check_imagej_roi import qro

NAMES={1:"POINT",2:"LINESTRING",3:"POLYGON",4:"MULTIPOINT",5:"MULTILINESTRING",6:"MULTIPOLYGON",7:"GEOMETRYCOLLECTION"}


def wkt(node):
    kind,data=node
    def point(p):return " ".join(format(v,".17g") for v in p)
    def line(p):return "("+",".join(map(point,p))+")"
    def polygon(p):return "("+",".join(map(line,p))+")"
    if kind==1:body="("+point(data)+")"
    elif kind==2:body=line(data)
    elif kind==3:body=polygon(data)
    elif kind==4:body="("+",".join("("+point(p)+")" for p in data)+")"
    elif kind==5:body="("+",".join(map(line,data))+")"
    elif kind==6:body="("+",".join(map(polygon,data))+")"
    else:body="("+",".join(map(wkt,data))+")"
    return NAMES[kind]+body


def wkb(node,little=True,mixed=False):
    index=0
    def encode(node):
        nonlocal index
        order=little if not mixed else index%2==0;index+=1
        endian="<" if order else ">";kind,data=node
        integer=lambda n:struct.pack(endian+"I",n)
        point=lambda p:struct.pack(endian+"dd",*p)
        line=lambda p:integer(len(p))+b"".join(map(point,p))
        polygon=lambda p:integer(len(p))+b"".join(map(line,p))
        if kind==1:body=point(data)
        elif kind==2:body=line(data)
        elif kind==3:body=polygon(data)
        else:
            members=data if kind==7 else [(kind-3,p) for p in data]
            body=integer(len(members))+b"".join(map(encode,members))
        return bytes([int(order)])+integer(kind)+body
    return encode(node)


class GeosOracle:
    """Admitted existing native GEOS reader, used only as a development oracle."""
    def __init__(self,directory):
        self.directory=os.add_dll_directory(str(directory))
        self.lib=ctypes.CDLL(str(directory/"geos_c.dll"))
        def function(name,args,returns):
            f=getattr(self.lib,name);f.argtypes=args;f.restype=returns;return f
        pointer=ctypes.c_void_p
        self.start=function("GEOS_init_r",[],pointer);self.finish=function("GEOS_finish_r",[pointer],None)
        self.context=self.start();assert self.context
        self.new_text=function("GEOSWKTReader_create_r",[pointer],pointer)
        self.read_text=function("GEOSWKTReader_read_r",[pointer,pointer,ctypes.c_char_p],pointer)
        self.free_text=function("GEOSWKTReader_destroy_r",[pointer,pointer],None)
        self.new_binary=function("GEOSWKBReader_create_r",[pointer],pointer)
        self.read_binary=function("GEOSWKBReader_read_r",[pointer,pointer,pointer,ctypes.c_size_t],pointer)
        self.free_binary=function("GEOSWKBReader_destroy_r",[pointer,pointer],None)
        self.destroy=function("GEOSGeom_destroy_r",[pointer,pointer],None)
        self.kind=function("GEOSGeomTypeId_r",[pointer,pointer],ctypes.c_int)
        self.version=function("GEOSversion",[],ctypes.c_char_p)().decode()
        self.checked=0
    def check(self,payload,text,kind):
        reader=self.new_text(self.context) if text else self.new_binary(self.context)
        try:
            buffer=ctypes.create_string_buffer(payload)
            geom=self.read_text(self.context,reader,payload) if text else self.read_binary(self.context,reader,buffer,len(payload))
            assert geom,"Independent GEOS reader refused authored positive geometry"
            try:assert self.kind(self.context,geom)=={1:0,2:1,3:3,4:4,5:5,6:6,7:7}[kind]
            finally:self.destroy(self.context,geom)
        finally:(self.free_text if text else self.free_binary)(self.context,reader)
        self.checked+=1
    def close(self):self.finish(self.context);self.directory.close()


def fixtures(root,geos):
    root.mkdir(parents=True,exist_ok=False)
    suite={"schema":"qpx.wkt-wkb-fixtures/1","positive":[],"negative":[]}
    oracle=GeosOracle(geos) if geos else None
    def row(kind,geometry,path="",ancestors=None,components=1):
        return {"id":"geometry"+path,"geometry":geometry,"class":"","c":-1,"z":0,"t":0,"color":None,
                "facts":{"geometry.type":NAMES[kind],"geometry.path":path or "/","geometry.components":str(components),**(ancestors or {})}}
    def positive(file,data,rows,kind,encoding,oracle_payload=None,bom=False,prefix="none"):
        (root/file).write_bytes(data)
        expected=[]
        for original in rows:
            e=json.loads(json.dumps(original));e["name"]=Path(file).stem
            e["facts"].update({"geometry.source_encoding":encoding,"geometry.root_type":NAMES[kind],"geometry.dimensions":"2","geometry.coordinates":"literal-image-pixel-xy","geometry.text_bom":"utf8" if bom else "none","geometry.hex_prefix":prefix});expected.append(e)
        suite["positive"].append({"file":file,"rows":expected})
        if oracle:oracle.check(oracle_payload if oracle_payload is not None else data,encoding=="wkt",kind)
    def negative(name,data):
        (root/name).write_bytes(data);suite["negative"].append(name)
    outer=[(1.,1.),(9.,1.),(9.,9.),(1.,9.),(1.,1.)]
    hole=[(2.,2.),(2.,4.),(4.,4.),(4.,2.),(2.,2.)]
    second=[(11.,1.),(15.,1.),(15.,5.),(11.,5.),(11.,1.)]
    point=(1.25,2.5);line=[(1.,2.),(8.,2.),(8.,8.)];line2=[(2.,11.),(5.,12.),(9.,11.)]
    points=[point,(2.5,3.75),(2.5,3.75)]
    polygon=[outer,hole];parts=[polygon,[second]]
    gpoint=qro(5,[point]);gline=qro(4,line);gline2=qro(4,line2)
    gpoly=qro(2,[(outer[:-1],[hole[:-1]])]);gparts=qro(3,[(outer[:-1],[hole[:-1]]),(second[:-1],[])])
    parent={"geometry.container.0.type":"MULTILINESTRING","geometry.container.0.path":"/","geometry.container.0.children":"2"}
    collection=(7,[(1,point),(5,[line,line2]),(6,parts),(7,[(4,points),(2,line)])])
    collection_parent={"geometry.container.0.type":"GEOMETRYCOLLECTION","geometry.container.0.path":"/","geometry.container.0.children":"4"}
    nested_line={**collection_parent,"geometry.container.1.type":"MULTILINESTRING","geometry.container.1.path":"/1","geometry.container.1.children":"2"}
    nested_group={**collection_parent,"geometry.container.1.type":"GEOMETRYCOLLECTION","geometry.container.1.path":"/3","geometry.container.1.children":"2"}
    cases=[("point",(1,point),[row(1,gpoint)]),("line",(2,line),[row(2,gline,components=3)]),
           ("polygon",(3,polygon),[row(3,gpoly)]),("multipoint",(4,points),[row(4,qro(5,points),components=3)]),
           ("multiline",(5,[line,line2]),[row(2,gline,"/0",parent,3),row(2,gline2,"/1",parent,3)]),
           ("multipolygon",(6,parts),[row(6,gparts,components=2)]),
           ("collection",collection,[row(1,gpoint,"/0",collection_parent),row(2,gline,"/1/0",nested_line,3),row(2,gline2,"/1/1",nested_line,3),row(6,gparts,"/2",collection_parent,2),row(4,qro(5,points),"/3/0",nested_group,3),row(2,gline,"/3/1",nested_group,3)])]
    for name,node,rows in cases:
        positive(name+".wkt",wkt(node).encode(),rows,node[0],"wkt")
        for suffix,little,mixed in [("le",True,False),("be",False,False),("mixed",True,True)]:
            positive(name+"-"+suffix+".wkb",wkb(node,little,mixed),rows,node[0],"wkb")
        binary=wkb(node);positive(name+".hex",binary.hex().encode(),rows,node[0],"hex-wkb",binary)
    positive("singleton.wkt",wkt((6,[polygon])).encode(),[row(6,gpoly)],6,"wkt")
    positive("singleton.wkb",wkb((6,[polygon])),[row(6,gpoly)],6,"wkb")
    positive("flat-multipoint.wkt",b"mUlTiPoInT (1.25 2.5, 2.5 3.75,2.5 3.75)",[row(4,qro(5,points),components=3)],4,"wkt")
    positive("decimal.wkt",b" \tPoInT (+.125 2.5E+0)\r\n",[row(1,qro(5,[(.125,2.5)]))],1,"wkt")
    positive("signed-zero.wkt",b"POINT (-0 +0)",[row(1,qro(5,[(-0.,0.)]))],1,"wkt")
    tiny=struct.unpack("<d",struct.pack("<Q",1))[0]
    positive("subnormal.wkb",wkb((1,(tiny,1e-300))),[row(1,qro(5,[(tiny,1e-300)]))],1,"wkb")
    positive("subnormal.wkt",b"POINT (4.9406564584124654e-324 1e-300)",[row(1,qro(5,[(tiny,1e-300)]))],1,"wkt")
    text=wkt((1,point)).encode();positive("unicode-μ.wkt",b"\xef\xbb\xbf"+text,[row(1,gpoint)],1,"wkt",text,bom=True)
    for name,prefix,bom in [("hex-0x.wkb","0x",False),("hex-bytea.wkb","\\x",False),("hex-bom.wkb","none",True)]:
        binary=wkb(collection,False,True);hexadecimal=" \t".join(binary.hex()[i:i+2] for i in range(0,len(binary)*2,2))
        data=(b"\xef\xbb\xbf" if bom else b"")+("\n"+(prefix if prefix!="none" else "")+hexadecimal+"\r\n").encode()
        positive(name,data,cases[-1][2],7,"hex-wkb",binary,bom,prefix)
    # Deep but admitted nesting, with every container path/count retained.
    deep=(1,point);facts={}
    for _ in range(31):deep=(7,[deep])
    for i in range(31):facts.update({f"geometry.container.{i}.type":"GEOMETRYCOLLECTION",f"geometry.container.{i}.path":"/0"*i or "/",f"geometry.container.{i}.children":"1"})
    for suffix,binary in [("wkt",False),("wkb",True)]:positive("depth32."+suffix,wkb(deep) if binary else wkt(deep).encode(),[row(1,gpoint,"/0"*31,facts)],7,"wkb" if binary else "wkt")
    for name,data in {
        "empty.wkt":b"POINT EMPTY","empty-collection.wkt":b"GEOMETRYCOLLECTION EMPTY",
        "child-empty.wkt":b"GEOMETRYCOLLECTION (POINT(1 2), LINESTRING EMPTY)",
        "z.wkt":b"POINT Z(1 2 3)","m.wkt":b"POINT M(1 2 3)","zm.wkt":b"POINT ZM(1 2 3 4)",
        "implicit-z.wkt":b"POINT(1 2 3)","srid.wkt":b"SRID=0;POINT(1 2)","curve.wkt":b"CIRCULARSTRING(1 2,3 4,5 6)",
        "unclosed.wkt":b"POLYGON((1 1,9 1,9 9,1 9))","short-ring.wkt":b"POLYGON((1 1,9 1,1 1))",
        "one-line.wkt":b"LINESTRING(1 2)","no-separator.wkt":b"POINT(1+2)","double-sign.wkt":b"POINT(+-1 2)",
        "mixed-multipoint.wkt":b"MULTIPOINT((1 2),3 4)","multiple.wkt":b"POINT(1 2) POINT(3 4)","nul.wkt":b"POINT(1 2)\0",
        "nan.wkt":b"POINT(NaN 2)","infinite.wkt":b"POINT(1e309 2)","underflow.wkt":b"POINT(1e-400 2)",
        "negative.wkt":b"POINT(-1 2)","decimal-hex.wkt":b"POINT(0x1 2)","bad-exponent.wkt":b"POINT(1e 2)",
        "token-limit.wkt":b"POINT("+b"1"*257+b" 2)","unicode-space.wkt":"POINT(1\u00a02)".encode(),
        "trailing.wkt":b"POINT(1 2);","parse-arena-limit.wkt":b" "*(1024*1024),
        "hex-odd.wkb":b"010","hex-nonhex.wkb":b"0x010000Q0","hex-trailing.wkb":wkb((1,point)).hex().encode()+b"00",
        "hex-empty.wkb":b"0x","hex-double-prefix.wkb":b"0x0x0101"}.items():negative(name,data)
    for name,value in [("nan",float("nan")),("infinite",float("inf")),("negative",-1.)]:negative(name+".wkb",wkb((1,(value,2.))))
    for name,kind in [("ewkb-z",0x80000001),("ewkb-m",0x40000001),("ewkb-srid",0x20000001),("iso-z",1001),("iso-m",2001),("iso-zm",3001),("curve",8),("unknown",0xffffffff)]:negative(name+".wkb",b"\x01"+struct.pack("<I",kind)+bytes(40))
    negative("byte-order.wkb",b"\x02"+wkb((1,point))[1:]);negative("wkb-trailing.wkb",wkb((1,point))+b"x")
    negative("multipart-type.wkb",b"\x01"+struct.pack("<II",4,1)+wkb((2,line)))
    for kind in range(2,8):negative("huge-count-"+str(kind)+".wkb",b"\x01"+struct.pack("<II",kind,0xffffffff))
    negative("empty-count.wkb",b"\x01"+struct.pack("<II",7,0))
    negative("unclosed.wkb",wkb((3,[outer[:-1]])))
    negative("invalid-topology.wkt",b"POLYGON((1 1,9 9,9 1,1 9,1 1))")
    negative("invalid-hole.wkb",wkb((3,[outer,second])))
    too_deep=(7,[deep]);negative("depth33.wkt",wkt(too_deep).encode());negative("depth33.wkb",wkb(too_deep))
    for at in range(len(wkb((1,point)))):negative("truncated-"+str(at)+".wkb",wkb((1,point))[:at])
    # Native bounds are checked at atomic append, not by silently clipping preview.
    (root/"outside.wkt").write_bytes(b"POINT(20 20)")
    (root/"image.tif").write_bytes(quantitative_tiff());(root/"volume.tif").write_bytes(volume_tiff())
    suite["sha256"]={p.name:hashlib.sha256(p.read_bytes()).hexdigest() for p in root.iterdir() if p.is_file()}
    if oracle:
        suite["independent_oracle"]={"scope":"Development-only existing GEOS readers validate authored positive source syntax/type; no new dependency or product parser fallback","version":oracle.version,"checked":oracle.checked,"libraries":{name:hashlib.sha256((geos/name).read_bytes()).hexdigest() for name in ["geos.dll","geos_c.dll"]}};oracle.close()
    (root/"fixtures.json").write_text(json.dumps(suite,ensure_ascii=False,indent=2)+"\n",encoding="utf8");return suite


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    for name in ["fixtures","test","manifest","cli","output","geos-oracle","runtime-dir","reuse-native"]:parser.add_argument("--"+name,type=Path,required=name=="fixtures")
    args=parser.parse_args()
    if not args.test:
        suite=fixtures(args.fixtures.resolve(),args.geos_oracle.resolve() if args.geos_oracle else None)
        print(json.dumps({"positive":len(suite["positive"]),"negative":len(suite["negative"]),"oracle":suite.get("independent_oracle")}));return
    assert args.manifest and args.cli and args.output;args.output.mkdir(parents=True,exist_ok=False)
    sha=lambda path:hashlib.sha256(path.read_bytes()).hexdigest();module=args.manifest.parent/json.loads(args.manifest.read_text())["plugins"][0]["module"]["path"]
    result={"schema":"qpx.wkt-wkb-validation/1","passed":False,"artifacts":{str(p.resolve()):sha(p) for p in [args.test,args.cli,args.manifest,module]},"commands":[]}
    result["fixtures_sha256"]=sha(args.fixtures/"fixtures.json")
    env={k:v for k,v in os.environ.items() if not k.upper().startswith("PYTHON")};env["PATH"]=os.pathsep.join([os.environ["SystemRoot"]+"/System32",os.environ["SystemRoot"]])
    if args.runtime_dir:
        env["PATH"]=str(args.runtime_dir.resolve())+os.pathsep+env["PATH"]
        result["runtime_libraries"]={p.name:sha(p) for p in args.runtime_dir.glob("*.dll")}
    reusable=None
    if args.reuse_native:
        previous=json.loads(args.reuse_native.read_text(encoding="utf8"))
        assert previous["artifacts"]==result["artifacts"],"Native evidence requires identical test/CLI/module/manifest bytes"
        assert previous.get("fixtures_sha256")==result["fixtures_sha256"],"Native reuse requires a pre-recorded exact fixture identity"
        row=next(r for r in previous["commands"] if r["name"]=="native")
        assert row["returncode"]==row["expected"]==0
        log=args.reuse_native.parent/row["log"];assert sha(log)==row["log_sha256"]
        reusable=(previous,row,log)
    def run(name,cmd,expected=0,report=False):
        cmd=list(map(str,cmd));x=subprocess.run(cmd,capture_output=True,env=env);log=args.output/(name+".log");log.write_bytes(x.stdout+x.stderr)
        result["commands"].append({"name":name,"argv":cmd,"returncode":x.returncode,"expected":expected,"log":log.name,"log_sha256":sha(log)});(args.output/"validation.json").write_text(json.dumps(result,indent=2)+"\n")
        assert x.returncode==expected,log.read_text(encoding="utf8",errors="replace")
        return json.loads(x.stdout) if report else None
    native=args.output/"native";native.mkdir()
    if reusable:
        previous,row,log=reusable
        source=args.reuse_native.parent/"native";shutil.copytree(source,native,dirs_exist_ok=True)
        assert {p.relative_to(source).as_posix():sha(p) for p in source.rglob("*") if p.is_file()}=={p.relative_to(native).as_posix():sha(p) for p in native.rglob("*") if p.is_file()}
        result["commands"].append(dict(row,log=str(log.resolve()),reused=True));report=json.loads(log.read_text(encoding="utf8"));result["reused_native_validation"]={"path":str(args.reuse_native.resolve()),"sha256":sha(args.reuse_native),"scope":"Identical native artifacts/fixtures; repair only packaged executable runtime discovery"}
    else:report=run("native",[args.test,"validate",args.manifest.resolve(),args.fixtures.resolve(),native.resolve()],report=True)
    result["native_report"]=report
    for file in ["point.wkt","collection.wkt","polygon-be.wkb","hex-bytea.wkb"]:
        output=args.output/(file+".qpproj")
        cli=run("cli-"+file,[args.cli,"--format-plugins",args.manifest.resolve(),"annotations-import","org.qupathxx.wkt-wkb",native/"base.qpproj",1,args.fixtures.resolve()/file,output],report=True)
        expected=json.loads((native/(file+".preview.json")).read_text());assert cli["preview_sha256"]==expected["preview_sha256"] and cli["receipt"]==expected["receipt"]
        run("compare-"+file,[args.test,"compare","unused",native/(file+".qpproj"),output])
    for name,file in [("invalid","invalid-topology.wkt"),("dimension","z.wkt"),("outside","outside.wkt"),("bad-binary","multipart-type.wkb")]:
        output=args.output/(name+".qpproj");run(name,[args.cli,"--format-plugins",args.manifest.resolve(),"annotations-import","org.qupathxx.wkt-wkb",native/"base.qpproj",1,args.fixtures.resolve()/file,output],expected=2);assert not output.exists()
    run("offline",[args.test,"offline","unused",args.fixtures.resolve(),native.resolve()])
    assert all(sha(Path(name))==digest for name,digest in result["artifacts"].items())
    suite=json.loads((args.fixtures/"fixtures.json").read_text(encoding="utf8"));assert all(sha(args.fixtures/name)==digest for name,digest in suite["sha256"].items())
    result["passed"]=True;result["fixtures_sha256"]=sha(args.fixtures/"fixtures.json");(args.output/"validation.json").write_text(json.dumps(result,indent=2)+"\n");print(json.dumps({"passed":True,"native_report":report,"native_commands":len(result["commands"])}))


if __name__=="__main__":main()
