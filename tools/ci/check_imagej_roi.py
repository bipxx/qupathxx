"""Authored independent ImageJ binary/ZIP fixtures; native product validation.

Python is a development fixture/oracle/controller only. It does not interpret
user ROI input, implement application state or serve as a product fallback.
"""
from pathlib import Path
import argparse
import hashlib
import io
import json
import math
import os
import shutil
import struct
import subprocess
import zipfile
import zlib
from check_format_family_plugins import quantitative_tiff


def qro(kind, values):
    out = b"QRO\1" + bytes([kind])
    def path(points):
        return struct.pack("<I", len(points)) + b"".join(struct.pack("<dd", *p) for p in points)
    if kind in (0, 1):
        out += struct.pack("<dddd", *values)
    elif kind in (4, 5):
        out += path(values)
    else:
        out += struct.pack("<I", len(values))
        for shell, holes in values:
            out += path(shell) + struct.pack("<I", len(holes)) + b"".join(path(h) for h in holes)
    return out.hex()


def roi(kind=1, *, points=None, subpixel=False, name="", props="", group=0,
        plane=(0, 0, 0), legacy=0, counters=None, style=0, version=229,
        rectangle=(1.25, 2.5, 4.0, 3.0), shape=None, header2=True):
    b = bytearray(64)
    b[:4] = b"Iout"
    def u16(at, n): struct.pack_into(">H", b, at, n)
    def u32(at, n): struct.pack_into(">I", b, at, n)
    u16(4, version); b[6] = kind
    for at, n in zip((8, 10, 12, 14), (2, 1, 6, 7)): u16(at, n)
    u16(50, 8192 | (128 if subpixel else 0)); u32(56, legacy)
    u32(40, style); u32(44, 0x80123456); u16(34, 2)
    if kind in (1, 2) and subpixel:
        struct.pack_into(">ffff", b, 18, *rectangle)
    if kind == 3:
        struct.pack_into(">ffff", b, 18, 1.25, 2.5, 9.5, 10.75)
    if points is not None:
        u16(16, len(points))
        # Integer array is present even when the subsequent floats are selected.
        b += b"".join(struct.pack(">H", int(x)-1) for x, y in points)
        b += b"".join(struct.pack(">H", int(y)-2) for x, y in points)
        if subpixel:
            b += b"".join(struct.pack(">f", x) for x, y in points)
            b += b"".join(struct.pack(">f", y) for x, y in points)
    if shape is not None:
        u32(36, len(shape)); b += struct.pack(">"+"f"*len(shape), *shape)
    if header2:
        h = len(b); u32(60, h); b += bytes(64)
        for at, n in zip((4, 8, 12), plane): u32(h+at, n)
        if group > 255: u16(h+52, group)
        else: b[h+30] = group
        struct.pack_into(">f", b, h+36, 2.5)
        for value, offset, length in [(name, 16, 20), (props, 40, 44)]:
            if value:
                encoded = value.encode("utf-16-be"); u32(h+offset, len(b)); u32(h+length, len(encoded)//2); b += encoded
        if counters is not None:
            assert len(counters) == len(points)
            u32(h+48, len(b)); b += b"".join(struct.pack(">I", n) for n in counters)
    return bytes(b)


def archive(members, method=0, *, descriptor=False, force64=False, comment=b""):
    # An unseekable sink makes Python's independent writer emit descriptors.
    class Sink(io.BytesIO):
        def seekable(self): return False
        def seek(self, *a):
            if descriptor: raise io.UnsupportedOperation("unseekable fixture")
            return super().seek(*a)
    sink = Sink()
    with zipfile.ZipFile(sink, "w", compression=method) as z:
        z.comment = comment
        for name, data in members:
            info = zipfile.ZipInfo(name, (2026, 11, 17, 0, 0, 0)); info.compress_type = method
            with z.open(info, "w", force_zip64=force64) as f: f.write(data)
    out = sink.getvalue()
    with zipfile.ZipFile(io.BytesIO(out)) as z:
        assert [(i.filename, z.read(i)) for i in z.infolist()] == members
    return out


def rotated_rectangle(params, *, subpixel=True, **metadata):
    # The freehand vertex arrays are redundant for subtype 5. Use an unrelated
    # fallback polygon to detect accidental preference for the stored arrays.
    data = bytearray(roi(7, points=[(1, 2), (8, 2), (8, 8), (1, 8)],
                         subpixel=subpixel, **metadata))
    struct.pack_into(">H", data, 48, 5)
    struct.pack_into(">4f", data, 18, *params[:4])
    struct.pack_into(">f", data, 52, params[4])
    return bytes(data)


def rotated_geometry(params):
    def f32(value):
        return struct.unpack(">f", struct.pack(">f", value))[0]

    x1, y1, x2, y2, width = map(f32, params)
    angle = math.atan2(y2 - y1, x2 - x1)
    perpendicular = (math.sin(angle) * width / 2, -math.cos(angle) * width / 2)
    points = [(f32(x + side * perpendicular[0]), f32(y + side * perpendicular[1]))
              for x, y, side in [(x1, y1, 1), (x2, y2, 1), (x2, y2, -1), (x1, y1, -1)]]
    x0, y0 = (math.floor(min(axis)) for axis in zip(*points))
    points = [(f32(f32(x - x0) + x0), f32(f32(y - y0) + y0)) for x, y in points]
    return qro(2, [(points, [])])


def crc_signature_archive(source):
    """Independent GF(2) oracle: valid payload with CRC equal to ZIP's marker."""
    base=zlib.crc32(source+bytes(4));basis={}
    for bit in range(32):
        delta=zlib.crc32(source+(1<<bit).to_bytes(4,"little"))^base;mask=1<<bit
        while delta:
            pivot=delta.bit_length()-1
            if pivot not in basis: basis[pivot]=(delta,mask);break
            value,previous=basis[pivot];delta^=value;mask^=previous
    delta=0x08074b50^base;mask=0
    while delta:
        value,previous=basis[delta.bit_length()-1];delta^=value;mask^=previous
    payload=source+mask.to_bytes(4,"little");crc=zlib.crc32(payload);assert crc==0x08074b50
    name=b"crc.roi";n=len(payload)
    local=struct.pack("<I5H3I2H",0x04034b50,20,8,0,0,0,0,0,0,len(name),0)+name
    descriptor=struct.pack("<III",crc,n,n)
    central=struct.pack("<I6H3I5H2I",0x02014b50,20,20,8,0,0,0,crc,n,n,len(name),0,0,0,0,0,0)+name
    offset=len(local)+n+len(descriptor)
    data=local+payload+descriptor+central+struct.pack("<I4H2IH",0x06054b50,0,0,1,1,len(central),offset,0)
    with zipfile.ZipFile(io.BytesIO(data)) as z:assert z.read("crc.roi")==payload
    return data


def fixtures(root):
    root.mkdir(parents=True, exist_ok=False)
    suite = {"schema": "qpx.imagej-roi-fixtures/1", "positive": [], "negative": []}
    def row(geometry, name, *, id="roi", classification="", facts=None, c=-1, z=0, t=0, color=None):
        return {"geometry": geometry, "name": name, "id": id, "class": classification,
                "facts": facts or {}, "c": c, "z": z, "t": t, "color": color}
    def positive(file, data, rows):
        (root/file).write_bytes(data); suite["positive"].append({"file": file, "rows": rows})
    def negative(file, data):
        (root/file).write_bytes(data); suite["negative"].append(file)
    rect_geom = qro(0, (1, 2, 6, 4))
    rect = roi(name="Δ specimen 🧫", props="note=α\nline=β\tμ\n", group=300, style=0xff123456)
    positive("rectangle.roi", rect, [row(rect_geom, "Δ specimen 🧫", classification="group/300", color=0x123456,
             facts={"imagej.version": "229", "imagej.group": "300", "imagej.properties.0": "note=α\nline=β\tμ\n", "imagej.header.0": rect[:64].hex()})])
    positive("simple217.roi", roi(version=217, header2=False), [row(rect_geom, "simple217", facts={"imagej.version": "217"})])
    for kind, name in [(1, "float-rectangle"), (2, "float-ellipse")]:
        positive(name+".roi", roi(kind, subpixel=True, name="Shared name", plane=(1,1,1)),
                 [row(qro(kind-1, (1.25, 2.5, 4, 3)), "Shared name", c=0, facts={"imagej.c-position": "1", "imagej.stroke-argb": "0"})])
    positive("ellipse.roi", roi(2), [row(qro(1, (1, 2, 6, 4)), "ellipse")])
    positive("line.roi", roi(3), [row(qro(4, [(1.25, 2.5), (9.5, 10.75)]), "line")])
    rotated = (2.25, 3.5, 8.375, 6.125, 2.375)
    rotated_bytes = rotated_rectangle(rotated, name="Tilted strip μ", plane=(1, 1, 1))
    positive("rotated-rectangle.roi", rotated_bytes,
             [row(rotated_geometry(rotated), "Tilted strip μ", c=0,
                  facts={"imagej.subtype": "5", "imagej.header.0": rotated_bytes[:64].hex()})])
    reverse = (7.5, 8.75, 1.5, 3.5, 1.125)
    positive("rotated-rectangle.zip", archive([("rotated.roi", rotated_rectangle(reverse, subpixel=False))], 8),
             [row(rotated_geometry(reverse), "rotated", id="zip:rotated.roi", facts={"imagej.subtype": "5"})])
    points = [(1,2), (8,2), (8,8), (1,8)]
    for kind, name in [(0,"polygon"), (7,"freehand"), (8,"traced"), (4,"freeline"), (5,"polyline"), (9,"angle")]:
        p = points[:3] if kind==9 else points
        g = qro(2, [(p, [])]) if kind in (0,7,8) else qro(4,p)
        positive(name+".roi", roi(kind, points=p), [row(g,name)])
    p = [(1.25,2.5), (8.5,2.25), (8.75,8.5)]
    positive("float-polygon.roi", roi(0, points=p, subpixel=True), [row(qro(2,[(p,[])]),"float-polygon")])
    positive("points.roi", roi(10, points=p, subpixel=True, name="Same name", counters=[0,257,258]),
             [row(qro(5,[point]),"Same name",id="roi#point/"+str(i), classification="counter/"+str([0,1,2][i]),
                  facts={"imagej.point-index":str(i),"imagej.point-counter":str([0,1,2][i]),"imagej.point-position":str(0 if i==0 else 1)}) for i,point in enumerate(p)])
    positive("counter-all.roi",roi(10,points=p[:2],subpixel=True,legacy=2,counters=[0,0]),
             [row(qro(5,[point]),"counter-all",id="roi#point/"+str(i),classification="counter/0",
                  facts={"imagej.legacy-position":"2","imagej.point-position":"0"}) for i,point in enumerate(p[:2])])
    props = "λ"*2500+"🧫\nend"
    positive("long-properties.roi", roi(props=props, style=0x80112233), [row(rect_geom,"long-properties",facts={"imagej.properties.0":"λ"*2048,"imagej.properties.1":"λ"*452+"🧫\nend","imagej.stroke-argb":str(0x80112233)})])
    positive("legacy.roi", roi(legacy=1), [row(rect_geom,"legacy",facts={"imagej.legacy-position":"1"})])
    outer=[(1,1),(9,1),(9,9),(1,9)]; hole=[(2,2),(8,2),(8,8),(2,8)]; island=[(3,3),(7,3),(7,7),(3,7)]
    shape=[]
    for path in [outer,hole,island]:
        shape += [0,*path[0]]
        for point in path[1:]: shape += [1,*point]
        shape += [4]
    composite=roi(shape=shape)
    positive("composite.roi",composite,[row(qro(3,[(outer,[hole]),(island,[])]),"composite")])
    members=[("nested/α.roi",rect),("second.roi",roi(3,name="Δ specimen 🧫"))]
    for name,method,descriptor,wide in [("stored.zip",0,False,False),("deflated.zip",8,False,False),("descriptors.zip",8,True,False),("local64.zip",8,False,True),("descriptor64.zip",8,True,True)]:
        data=archive(members,method,descriptor=descriptor,force64=wide,comment=b"authored archive")
        positive(name,data,[row(rect_geom,"Δ specimen 🧫",id="zip:nested/α.roi",classification="group/300",color=0x123456,facts={"zip.entry":"nested/α.roi","zip.method":str(method)}),row(qro(4,[(1.25,2.5),(9.5,10.75)]),"Δ specimen 🧫",id="zip:second.roi",facts={"zip.entry":"second.roi"})])
    # Small ZIP64 end records are legal: independent handcrafted end metadata.
    data=archive([("r.roi",roi())]); at=len(data)-22; count=struct.unpack_from("<H",data,at+10)[0]; size,offset=struct.unpack_from("<II",data,at+12)
    record=struct.pack("<IQHHIIQQQQ",0x06064b50,44,45,45,0,0,count,count,size,offset)
    locator=struct.pack("<IIQI",0x07064b50,0,at,1); end=bytearray(data[at:]);struct.pack_into("<HHII",end,8,65535,65535,0xffffffff,0xffffffff)
    positive("end64.zip",data[:at]+record+locator+end,[row(rect_geom,"r",id="zip:r.roi")])
    # CP437 filename independent of Unicode flags and locale.
    data=bytearray(archive([("x.roi",roi())])); central=data.index(b"PK\1\2");data[30]=0x82;data[central+46]=0x82
    positive("cp437.zip",data,[row(rect_geom,"é",id="zip:é.roi",facts={"zip.entry":"é.roi"})])
    positive("crc-signature.zip",crc_signature_archive(roi()),[row(rect_geom,"crc",id="zip:crc.roi",facts={"zip.crc32":str(0x08074b50)})])
    def mutate(file, at, fmt, value, source=rect):
        data=bytearray(source);struct.pack_into(fmt,data,at,value);negative(file,data)
    negative("truncated.roi",rect[:63]);negative("magic.roi",b"Nope"+rect[4:]);mutate("version.roi",4,">H",230);mutate("type.roi",6,">B",6)
    for subtype in range(1,6): mutate("subtype-"+str(subtype)+".roi",48,">H",subtype)
    for name, at, fmt, value in [("rotated-empty-axis", 16, ">H", 0),
                                 ("rotated-short-vertices", 16, ">H", 3),
                                 ("rotated-composite", 36, ">I", 1),
                                 ("rotated-zero-width", 52, ">f", 0),
                                 ("rotated-negative-width", 52, ">f", -1),
                                 ("rotated-nonfinite", 18, ">f", float("nan")),
                                 ("rotated-overflow", 52, ">f", 1e30),
                                 ("rotated-metadata-overlap", 60, ">I", 64)]:
        mutate(name + ".roi", at, fmt, value, rotated_bytes)
    negative("rotated-coincident.roi", rotated_rectangle((1, 2, 1, 2, 3)))
    negative("rotated-atomic.zip", archive([("r.roi", rotated_bytes),
                                           ("bad.roi", rotated_rectangle((1, 2, 1, 2, 3)))], 8))
    mutate("spline.roi",50,">H",8193);mutate("rounded.roi",54,">H",2);mutate("header-offset.roi",60,">I",0xfffffff0);mutate("header-overlap.roi",60,">I",32)
    h=struct.unpack_from(">I",rect,60)[0]; name_at=struct.unpack_from(">I",rect,h+16)[0]
    mutate("name-offset.roi",h+16,">I",0xfffffff0);mutate("name-overlap.roi",h+16,">I",64);mutate("name-nul.roi",name_at,">H",0);mutate("surrogate.roi",name_at,">H",0xdc00)
    mutate("negative-plane.roi",h+4,">I",0xffffffff);mutate("nan-width.roi",26,">I",0x7fc00000,roi(subpixel=True))
    mutate("negative-local.roi",64,">H",65535,roi(0,points=points));negative("zero-rectangle.roi",roi(subpixel=True,rectangle=(1,2,0,4)))
    negative("self-crossing.roi",roi(0,points=[(1,2),(8,8),(8,2),(1,8)]));negative("curve.roi",roi(shape=[0,1,1,2,2,2,3,3,4]))
    negative("outside.roi",roi(subpixel=True,rectangle=(20,20,3,4)))  # preview retained; apply check is separate below
    # Outside coordinates are valid native geometry: remove from parser-negative list.
    suite["negative"].remove("outside.roi")
    good=archive([("r.roi",roi())],8)
    for name,at,value in [("zip-crc.zip",14,1),("zip-method.zip",8,99),("zip-encrypted.zip",6,1),("zip-name-mismatch.zip",30,ord("z"))]:
        data=bytearray(good);data[at]=value;negative(name,data)
    negative("zip-truncated.zip",good[:-4]);negative("zip-trailing.zip",good+b"extra");negative("zip-unsafe.zip",archive([("../r.roi",roi())]));negative("zip-non-roi.zip",archive([("readme.txt",b"authored")]))
    negative("zip-duplicates.zip",archive([("r.roi",roi()),("r.roi",roi())]));negative("zip-unsupported-member.zip",archive([("r.roi",roi()),("x.roi",roi(shape=[0,1,1,2,2,2,3,3,4]))]))
    negative("mixed-point-planes.roi",roi(10,points=p[:2],subpixel=True,plane=(1,1,1),counters=[257,257]))
    data=bytearray(archive([("r.roi",roi())]));data[35]^=1;negative("zip-data-crc.zip",data)
    negative("parse-arena-limit.roi",roi()+bytes(1024*1024))
    data=bytearray((root/"end64.zip").read_bytes());locator=len(data)-42;struct.pack_into("<Q",data,locator+8,locator-1);negative("zip64-overlap.zip",data)
    (root/"image.tif").write_bytes(quantitative_tiff())
    suite["sha256"]={p.name:hashlib.sha256(p.read_bytes()).hexdigest() for p in root.iterdir() if p.is_file()}
    (root/"fixtures.json").write_text(json.dumps(suite,ensure_ascii=False,indent=2)+"\n",encoding="utf8")
    return suite


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--fixtures",type=Path,required=True)
    parser.add_argument("--test",type=Path)
    parser.add_argument("--manifest",type=Path)
    parser.add_argument("--cli",type=Path)
    parser.add_argument("--output",type=Path)
    args=parser.parse_args()
    if not args.test:
        suite=fixtures(args.fixtures.resolve());print(json.dumps({"positive":len(suite["positive"]),"negative":len(suite["negative"])}));return
    assert args.manifest and args.cli and args.output
    args.output.mkdir(parents=True,exist_ok=False)
    sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest()
    module=args.manifest.parent/json.loads(args.manifest.read_text())["plugins"][0]["module"]["path"]
    result={"schema":"qpx.imagej-roi-validation/1","passed":False,"artifacts":{str(p.resolve()):sha(p) for p in [args.test,args.cli,args.manifest,module]},"commands":[]}
    env={k:v for k,v in os.environ.items() if not k.upper().startswith("PYTHON")};env["PATH"]=os.pathsep.join([os.environ["SystemRoot"]+"/System32",os.environ["SystemRoot"]])
    def run(name,cmd,expected=0,report=False):
        cmd=list(map(str,cmd));x=subprocess.run(cmd,capture_output=True,env=env);log=args.output/(name+".log");log.write_bytes(x.stdout+x.stderr)
        result["commands"].append({"name":name,"argv":cmd,"returncode":x.returncode,"expected":expected,"log":log.name,"log_sha256":sha(log)});(args.output/"validation.json").write_text(json.dumps(result,indent=2)+"\n")
        assert x.returncode==expected,log.read_text(encoding="utf8",errors="replace")
        return json.loads(x.stdout) if report else None
    native=args.output/"native";native.mkdir()
    report=run("native",[args.test,"validate",args.manifest.resolve(),args.fixtures.resolve(),native.resolve()],report=True)
    result["native_report"]=report
    for file,extra in [("rectangle.roi",["--class-map-json",json.dumps({"group/300":"Native"})]),("descriptors.zip",["--class-map-json",json.dumps({"group/300":"Native"})]),("legacy.roi",["--legacy-position-axis","z"]),("rotated-rectangle.roi",[]),("rotated-rectangle.zip",[])]:
        destination=args.output/(file+".qpproj")
        cli=run("cli-"+file,[args.cli,"--format-plugins",args.manifest.resolve(),"annotations-import","org.qupathxx.imagej-roi",native/"base.qpproj",1,args.fixtures.resolve()/file,destination,*extra],report=True)
        expected=json.loads((native/(file+".preview.json")).read_text());assert cli["preview_sha256"]==expected["preview_sha256"] and cli["receipt"]==expected["receipt"]
        run("compare-"+file,[args.test,"compare",args.manifest.resolve(),native/(file+".qpproj"),destination])
    for name,file,extra in [("missing-class","rectangle.roi",[]),("ambiguous-axis","legacy.roi",[]),("malformed","zip-crc.zip",[]),("outside","outside.roi",[])]:
        destination=args.output/(name+".qpproj");run(name,[args.cli,"--format-plugins",args.manifest.resolve(),"annotations-import","org.qupathxx.imagej-roi",native/"base.qpproj",1,args.fixtures.resolve()/file,destination,*extra],expected=2);assert not destination.exists()
    run("offline",[args.test,"offline","unused",args.fixtures.resolve(),native.resolve()])
    assert all(sha(Path(p))==value for p,value in result["artifacts"].items())
    fixture=json.loads((args.fixtures/"fixtures.json").read_text(encoding="utf8"));assert all(sha(args.fixtures/p)==value for p,value in fixture["sha256"].items())
    result["passed"]=True;(args.output/"validation.json").write_text(json.dumps(result,indent=2)+"\n");print(json.dumps({"passed":True,"native_report":report,"commands":len(result["commands"])}))


if __name__=="__main__":main()
