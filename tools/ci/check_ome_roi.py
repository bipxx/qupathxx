"""Authored OME XML fixtures and independent literal geometry/infoset oracles.

Development control only: user input interpretation, options, geometry, locks,
planes, identities and publication are owned exclusively by native services.
"""
import argparse
import hashlib
import json
import os
from pathlib import Path
import subprocess
from xml.dom import minidom, Node
from xml.sax.saxutils import quoteattr
from check_imagej_roi import qro
from check_format_family_plugins import quantitative_tiff
from check_annotation_source_contract import volume_tiff

NS = "http://www.openmicroscopy.org/Schemas/OME/2016-06"
sha = lambda p: hashlib.sha256(p.read_bytes()).hexdigest()


def field(value):
    return str(len(value.encode("utf8"))) + ":" + value


def attrs(node):
    return [a for a in node.attributes.values() if a.namespaceURI != "http://www.w3.org/2000/xmlns/"]


def namespace_value(node):
    # Independent DOM ancestor walk, including rebound/undeclared namespaces.
    chain=[]; current=node; bindings={}
    while current is not None and current.nodeType==Node.ELEMENT_NODE:
        chain.append(current);current=current.parentNode
    for current in reversed(chain):
        for a in current.attributes.values():
            if a.namespaceURI=="http://www.w3.org/2000/xmlns/":
                bindings["" if a.name=="xmlns" else a.localName]=a.value or ""
    return field(str(len(bindings)))+"".join(field(k)+field(v) for k,v in sorted(bindings.items()))


def tree(node):
    data = field(node.namespaceURI or "") + field(node.localName)
    data += namespace_value(node)
    attributes = attrs(node)
    data += field(str(len(attributes)))
    for a in attributes:
        data += field(a.namespaceURI or "") + field(a.localName) + field(a.value)
    content = []
    for c in node.childNodes:
        if c.nodeType == Node.ELEMENT_NODE:
            content.append(("E", tree(c)))
        elif c.nodeType in (Node.TEXT_NODE, Node.CDATA_SECTION_NODE):
            if content and content[-1][0] == "T":
                content[-1] = ("T", content[-1][1] + c.data)
            else:
                content.append(("T", c.data))
    data += field(str(len(content)))
    for kind, value in content:
        data += field(kind) + (field(value) if kind == "T" else value)
    return data


def attr_value(node):
    values = attrs(node)
    return "QOX2A" + namespace_value(node) + field(str(len(values))) + "".join(
        field(a.namespaceURI or "") + field(a.localName) + field(a.value) for a in values)


def chunks(facts, key, value):
    facts[key + ".bytes"] = str(len(value.encode("utf8")))
    at = 0; index = 0
    while at < len(value):
        end = at
        count = 0
        while end < len(value) and count + len(value[end].encode("utf8")) <= 4096:
            count += len(value[end].encode("utf8")); end += 1
        facts[key + "." + str(index)] = value[at:end]
        at = end; index += 1


def elements(n, name=None):
    return [c for c in n.childNodes if c.nodeType == Node.ELEMENT_NODE and
            (name is None or c.namespaceURI == NS and c.localName == name)]


def expected(source, geometries, options):
    """Independent XML DOM plus predeclared numeric expectations, no product code."""
    root = minidom.parseString(source).documentElement
    images = elements(root, "Image"); rois = elements(root, "ROI")
    by_id = {n.getAttribute("ID"): n for n in root.getElementsByTagName("*")
             if n.hasAttribute("ID") and not n.localName.endswith("Ref")}
    bound = {n.getAttribute("ID") for i in images for n in elements(i, "ROIRef")}
    image = next((i for i in images if i.getAttribute("ID") == options.get("image_id")), None)
    if image is None and len(images) == 1 and not options.get("unbound_rois"):
        image = images[0]
    selected = ([r for r in rois if r.getAttribute("ID") not in bound] if options.get("unbound_rois") else
                [by_id[r.getAttribute("ID")] for r in elements(image, "ROIRef")] if image else rois)
    out = []
    for roi in selected:
        for s in elements(elements(roi, "Union")[0]):
            ident = s.getAttribute("ID")
            facts = {"ome.namespace": NS, "ome.roi_id": roi.getAttribute("ID"), "ome.shape_id": ident,
                     "ome.shape_type": s.localName, "ome.coordinate_profile": "level-zero-image-pixels/binary64",
                     "ome.label_profile": "anchor-and-text/source-font-metadata" if s.localName == "Label" else "not-label",
                     "ome.selection": "unbound-rois" if options.get("unbound_rois") else "source-image-roi-refs" if image else "roi-only-document",
                     "ome.image_id": image.getAttribute("ID") if image else "", "ome.source_images": str(len(images)),
                     "ome.source_rois": str(len(rois)), "ome.selected_rois": str(len(selected)),
                     "ome.unselected_rois": str(len(rois)-len(selected))}
            chunks(facts,"ome.roi.attributes",attr_value(roi))
            for desc in elements(roi,"Description"):
                chunks(facts,"ome.roi.description","QOX2"+tree(desc))
            chunks(facts,"ome.shape.tree","QOX2"+tree(s))
            if image:
                pixels = elements(image,"Pixels")[0]
                chunks(facts,"ome.image.attributes",attr_value(image)); chunks(facts,"ome.pixels.attributes",attr_value(pixels))
                channels = elements(pixels,"Channel")
                for i,c in enumerate(channels): chunks(facts,"ome.channel."+str(i),attr_value(c))
                facts["ome.channels"] = str(len(channels))
            pending = ([image] if image else [])+[roi,s]; found = []
            for n in pending:
                for ref in elements(n,"AnnotationRef"):
                    target = by_id[ref.getAttribute("ID")]
                    if target not in found: found.append(target); pending.append(target)
            facts["ome.annotations.count"] = str(len(found))
            for i,n in enumerate(found): chunks(facts,"ome.annotation."+str(i),"QOX2"+tree(n))
            color = None
            primary = s.getAttribute("StrokeColor") if s.hasAttribute("StrokeColor") else s.getAttribute("FillColor")
            if primary and int(primary)&255 == 255: color = (int(primary)&0xffffffff)>>8
            out.append({"id":ident,"name":(s.getAttribute("Text") if s.hasAttribute("Text") else
                roi.getAttribute("Name") if roi.hasAttribute("Name") else ident) or ident,
                "geometry":geometries[ident],"class":"","c":int(s.getAttribute("TheC")) if s.hasAttribute("TheC") else -1,
                "z":int(s.getAttribute("TheZ")) if s.hasAttribute("TheZ") else 0,
                "t":int(s.getAttribute("TheT")) if s.hasAttribute("TheT") else 0,
                "color":color,"locked":s.getAttribute("Locked").strip() in ("true","1"),"facts":facts})
    return out


def fixtures(root):
    root.mkdir(parents=True, exist_ok=False)
    suite = {"schema":"qpx.ome-roi-fixtures/1","positive":[],"negative":[]}
    def doc(shapes, extra="", roi_attrs='ID="ROI:0" Name="ROI μ"', images=""):
        return f'<OME xmlns="{NS}">{images}<ROI {roi_attrs}><Union>{shapes}</Union></ROI>{extra}</OME>'
    def shape(kind="Point", attributes='X="1.25" Y="2.5"', ident="Shape:0001", child=""):
        return f'<{kind} ID="{ident}" {attributes}>{child}</{kind}>'
    def image(ident="Image:0", roi="ROI:0", samples=1, name="Image α", refs=True):
        pixels = (f'<Pixels ID="Pixels:{ident}" DimensionOrder="XYCZT" Type="uint8" SizeX="16" SizeY="16" SizeC="3" SizeZ="2" SizeT="2" PhysicalSizeX="0.25" PhysicalSizeXUnit="µm">'
                  f'<Channel ID="Channel:{ident}" SamplesPerPixel="{samples}"/></Pixels>')
        return f'<Image ID="{ident}" Name={quoteattr(name)}>{pixels}'+(f'<ROIRef ID="{roi}"/>' if refs else "")+'</Image>'
    point = qro(5,[(1.25,2.5)])
    def pos(name, source, geometries=None, options=None, encoding="utf8"):
        data = source.encode(encoding)
        (root/name).write_bytes(data)
        suite["positive"].append({"file":name,"options":options or {},"rows":expected(data, geometries or {"Shape:0001":point}, options or {})})
    def neg(name, source, options=None):
        (root/name).write_bytes(source.encode("utf8") if isinstance(source,str) else source)
        suite["negative"].append({"file":name,"options":options or {}})
    pos("point.ome.xml",doc(shape()))
    values = [("Rectangle",'X="1.25" Y="2.5" Width="4" Height="3"',qro(0,[1.25,2.5,4,3])),
              ("Ellipse",'X="5" Y="6" RadiusX="2" RadiusY="3"',qro(1,[3,3,4,6])),
              ("Label",'X="1.25" Y="2.5" Text="Cell &amp; μ" FontFamily="sans-serif" FontSize="12" FontSizeUnit="pt"',point),
              ("Line",'X1="1" Y1="2" X2="5" Y2="6" MarkerStart="Arrow"',qro(4,[(1,2),(5,6)])),
              ("Polyline",'Points="1,2 3,4 5,2"',qro(4,[(1,2),(3,4),(5,2)])),
              ("Polygon",'Points="1,1 8,1 8,8 1,8" FillRule="EvenOdd"',qro(2,[([(1,1),(8,1),(8,8),(1,8)],[])]))]
    for kind,a,g in values: pos(kind.lower()+".ome.xml",doc(shape(kind,a)),{"Shape:0001":g})
    all_shapes = shape()+"".join(shape(k,a,"Shape:"+str(i+2)) for i,(k,a,g) in enumerate(values))
    pos("primitives.ome.xml",doc(all_shapes),dict([("Shape:0001",point)]+[("Shape:"+str(i+2),g) for i,(_,_,g) in enumerate(values)]))
    for name,a in [("locked",' Locked="true" StrokeColor="305420031"'),("unlocked",' Locked="0"'),
                   ("locked-one",' Locked="1"'),("translucent",' StrokeColor="305419904" FillColor="-16711681"'),
                   ("fill",' FillColor="-16711681"'),("transparent",' FillColor="0"'),
                   ("style",' StrokeWidth="1.5" StrokeWidthUnit="µm" StrokeDashArray="2 3" FontStyle="Bold"')]:
        pos(name+".ome.xml",doc(shape(attributes='X="1.25" Y="2.5"'+a)))
    pos("no-name.ome.xml",doc(shape(),roi_attrs='ID="ROI:0"'))
    pos("empty-text.ome.xml",doc(shape(attributes='X="1.25" Y="2.5" Text=""')))
    pos("zero.ome.xml",doc(shape(attributes='X="-0" Y="0"')),{"Shape:0001":qro(5,[(-0.0,0.0)])})
    pos("precision.ome.xml",doc(shape(attributes='X="1.0000000000000002" Y="4.9406564584124654e-324"')),
        {"Shape:0001":qro(5,[(1.0000000000000002,5e-324)])})
    pos("plus-exponent.ome.xml",doc(shape(attributes='X=" +1.25e+0 " Y="2.5E0"')))
    identity = '<Transform A00="1" A01="0" A10="0" A11="1" A02="0" A12="0"/>'
    pos("identity.ome.xml",doc(shape(child=identity)))
    transform = '<Transform A00="2" A01="0" A10="0" A11="1" A02="1" A12="2"/>'
    pos("translated.ome.xml",doc(shape(child=transform)),{"Shape:0001":qro(5,[(3.5,4.5)])})
    shear = '<Transform A00="1" A01="0.5" A10="0" A11="1" A02="0" A12="0"/>'
    pos("sheared-rectangle.ome.xml",doc(shape("Rectangle",'X="1" Y="2" Width="3" Height="4"',child=shear)),
        {"Shape:0001":qro(2,[([(2,2),(5,2),(7,6),(4,6)],[])])})
    swap = '<Transform A00="0" A01="1" A10="1" A11="0" A02="0" A12="0"/>'
    pos("swapped-ellipse.ome.xml",doc(shape("Ellipse",'X="5" Y="6" RadiusX="2" RadiusY="3"',child=swap)),
        {"Shape:0001":qro(1,[3,3,6,4])})
    pos("image.ome.xml",doc(shape(),images=image()))
    pos("plane.ome.xml",doc(shape(attributes='X="1.25" Y="2.5" TheC="0" TheZ="0" TheT="0"'),images=image()))
    pos("bundled-all-c.ome.xml",doc(shape(),images=image(samples=3)))
    two = doc(shape(),images=image()+image("Image:1","ROI:1"),extra='<ROI ID="ROI:1"><Union>'+shape(ident="Shape:1")+'</Union></ROI>')
    pos("multi-first.ome.xml",two,options={"image_id":"Image:0"})
    pos("multi-second.ome.xml",two,{"Shape:1":point},options={"image_id":"Image:1"})
    pos("multi-escaped-option.ome.xml",doc(shape(),images=image("Image:μ")),options={"image_id":"Image:μ"})
    unbound = doc(shape(),images=image(refs=False))
    pos("unbound.ome.xml",unbound,options={"unbound_rois":True})
    pos("namespaced.ome.xml",doc(shape()).replace('<OME ', '<o:OME ').replace('</OME>', '</o:OME>').replace('xmlns=', 'xmlns:o=').replace('<ROI','<o:ROI').replace('</ROI','</o:ROI').replace('<Union','<o:Union').replace('</Union','</o:Union').replace('<Point','<o:Point').replace('</Point','</o:Point'))
    pos("utf16le.ome.xml",'<?xml version="1.0" encoding="UTF-16"?>'+doc(shape()),encoding="utf-16")
    pos("utf16be.ome.xml",'\ufeff<?xml version="1.0" encoding="UTF-16"?>'+doc(shape()),encoding="utf-16-be")
    pos("utf8-bom.ome.xml",'\ufeff<?xml version="1.0" encoding="UTF-8"?>'+doc(shape()))
    annotations = '<StructuredAnnotations><XMLAnnotation ID="Annotation:0"><Value>before<x:m xmlns:x="urn:authored:test" key="v">inside μ</x:m>after</Value></XMLAnnotation></StructuredAnnotations>'
    pos("annotation.ome.xml",doc(shape(child='<AnnotationRef ID="Annotation:0"/>'),extra=annotations))
    inherited = ('<StructuredAnnotations><XMLAnnotation ID="Annotation:0"><Value>'
        '<m:entry xmlns:m="urn:authored:test" Type="p:Intensity">'
        '<m:entry xmlns:p="urn:authored:units:v2" Type="p:Density"/>'
        '<plain xmlns="" Ref="p:Quantity"/></m:entry></Value></XMLAnnotation></StructuredAnnotations>')
    pos("inherited-namespace.ome.xml",doc(shape(child='<AnnotationRef ID="Annotation:0"/>'),extra=inherited).replace('<OME ', '<OME xmlns:p="urn:authored:units:v1" ',1))
    pos("chunked-annotation.ome.xml",doc(shape(child='<AnnotationRef ID="Annotation:0"/>'),extra=annotations.replace("inside μ","μ"*5000)))
    pos("long-text-μ.ome.xml",doc(shape("Label",'X="1.25" Y="2.5" Text='+quoteattr('μ' * 1800))),{"Shape:0001":point})
    neg("ambiguous.ome.xml",two); neg("missing-image.ome.xml",two,{"image_id":"Image:absent"})
    neg("wrong-id-type.ome.xml",two,{"image_id":"ROI:0"}); neg("unselected-unbound.ome.xml",unbound)
    neg("conflicting-options.ome.xml",two,{"image_id":"Image:0","unbound_rois":True})
    for i,opt in enumerate([{"unbound_rois":1},{"unbound_rois":"true"},{"unknown":False},{"image_id":1},{"image_id":""},{"image_id":[]},{"image_id":"x"*257}]): neg("bad-option-"+str(i)+".ome.xml",two,opt)
    for name,attrs_ in [("nan",'X="NaN" Y="2"'),("inf",'X="INF" Y="2"'),("overflow",'X="1e999" Y="2"'),
                         ("underflow",'X="1e-999" Y="2"'),("hex",'X="0x1" Y="2"'),("negative",'X="-1" Y="2"'),
                         ("missing-y",'X="1"'),("partial",'X="1x" Y="2"'),("bad-lock",'X="1" Y="2" Locked="yes"'),
                         ("bad-color",'X="1" Y="2" FillColor="4294967295"'),("bad-plane",'X="1" Y="2" TheZ="-1"'),
                         ("fractional-plane",'X="1" Y="2" TheZ="0.5"'),("unknown-attr",'X="1" Y="2" Units="µm"')]:
        neg(name+".ome.xml",doc(shape(attributes=attrs_)))
    neg("plane-range.ome.xml",doc(shape(attributes='X="1" Y="2" TheZ="2"'),images=image()))
    neg("bundled-c.ome.xml",doc(shape(attributes='X="1" Y="2" TheC="0"'),images=image(samples=3)))
    neg("mask.ome.xml",doc(shape("Mask",'X="1" Y="2" Width="2" Height="2"',child='<BinData Length="1" BigEndian="false">AA==</BinData>')))
    neg("rotated-ellipse.ome.xml",doc(shape("Ellipse",'X="5" Y="5" RadiusX="2" RadiusY="1"',child=shear)))
    neg("unknown-shape.ome.xml",doc(shape("Circle",'X="5" Y="5" Radius="2"')))
    neg("foreign-shape.ome.xml",doc(shape().replace('<Point ','<Point xmlns="urn:foreign" ')))
    neg("bad-transform.ome.xml",doc(shape(child='<Transform A00="1"/>')))
    neg("transform-overflow.ome.xml",doc(shape(attributes='X="2" Y="2.5"',child=transform.replace('A00="2"','A00="1e308"'))))
    neg("repeated-transform.ome.xml",doc(shape(child=identity+identity)))
    neg("negative-extent.ome.xml",doc(shape("Rectangle",'X="1" Y="2" Width="-2" Height="2"')))
    neg("zero-radius.ome.xml",doc(shape("Ellipse",'X="5" Y="5" RadiusX="0" RadiusY="1"')))
    for name,points in [("crossing","1,1 8,8 8,1 1,8"),("one-point","1,1"),("partial-pairs","1,1 2"),("invalid-pairs","1,1,2,2")]:
        neg(name+".ome.xml",doc(shape("Polygon",'Points='+quoteattr(points))))
    for name,src in [("no-ns",doc(shape()).replace(' xmlns="'+NS+'"',"")),
                     ("wrong-ns",doc(shape()).replace(NS,NS+"/other")),("old-schema",doc(shape()).replace("2016-06","2015-01")),
                     ("duplicate-id",doc(shape()+shape())),("duplicate-attr",doc(shape().replace('X="1.25"','X="1.25" X="2"'))),
                     ("unknown-child",doc(shape(child='<Unknown/>'))),("unresolved-annotation",doc(shape(child='<AnnotationRef ID="Annotation:missing"/>'))),
                     ("wrong-reference",doc(shape(child='<AnnotationRef ID="ROI:0"/>'))),("empty-union",doc("")),
                     ("repeated-union",doc(shape()).replace('</ROI>','<Union>'+shape(ident="Shape:1")+'</Union></ROI>')),
                     ("wrong-root",'<Root xmlns="'+NS+'"/>'),("text-in-shape",doc(shape(child="oops"))),
                     ("mismatched",doc(shape())[:-7]),("unknown-entity",doc(shape(attributes='X="1" Y="2" Text="&boom;"'))),
                     ("nul",doc(shape()).replace('X="1.25"','X="\0"')),("xml11",'<?xml version="1.1"?>'+doc(shape())),
                     ("latin1",'<?xml version="1.0" encoding="ISO-8859-1"?>'+doc(shape()).replace('ROI μ','ROI'))]: neg(name+".ome.xml",src)
    for name,head in [("doctype",'<!DOCTYPE OME>'),("entity",'<!DOCTYPE OME [<!ENTITY x "1">]>'),
                      ("external",'<!DOCTYPE OME SYSTEM "file:///C:/secret.xml">'),
                      ("network",'<!DOCTYPE OME SYSTEM "https://invalid.example/ome.dtd">')]: neg(name+".ome.xml",head+doc(shape()))
    neg("invalid-utf8.ome.xml",doc(shape()).encode().replace(b'ROI \xce\xbc',b'ROI \xff'))
    neg("missing-roi-ref.ome.xml",doc(shape(),images=image(roi="ROI:absent")))
    neg("deep.ome.xml",doc(shape(child="<Unknown>"*70+"</Unknown>"*70)))
    # Source profiles intentionally distinguish parser budgets from host quotas.
    (root/"parse-arena-limit.ome.xml").write_text(doc("".join(shape(ident="Shape:"+str(i)) for i in range(5000))),encoding="utf8")
    (root/"outside.ome.xml").write_text(doc(shape(attributes='X="20" Y="20"')),encoding="utf8")
    (root/"flags2.txt").write_bytes(b"2"); (root/"flags8.txt").write_bytes(b"8")
    (root/"image.tif").write_bytes(quantitative_tiff()); (root/"volume.tif").write_bytes(volume_tiff())
    suite["sha256"] = {p.name:sha(p) for p in root.iterdir() if p.is_file()}
    (root/"fixtures.json").write_text(json.dumps(suite,ensure_ascii=False,indent=2)+"\n",encoding="utf8")
    return suite


def main():
    p=argparse.ArgumentParser(description=__doc__)
    for name in ["fixtures","test","manifest","cli","output","runtime-dir"]: p.add_argument("--"+name,type=Path,required=name=="fixtures")
    a=p.parse_args()
    if not a.test:
        suite=fixtures(a.fixtures.resolve()); print(json.dumps({"positive":len(suite["positive"]),"negative":len(suite["negative"])})); return
    assert a.manifest and a.cli and a.output
    a.output.mkdir(parents=True,exist_ok=False)
    module=a.manifest.parent/json.loads(a.manifest.read_text())["plugins"][0]["module"]["path"]
    result={"schema":"qpx.ome-roi-validation/1","passed":False,"artifacts":{str(x.resolve()):sha(x) for x in [a.test,a.cli,a.manifest,module]},"fixtures_sha256":sha(a.fixtures/"fixtures.json"),"commands":[]}
    env={k:v for k,v in os.environ.items() if not k.upper().startswith("PYTHON")}
    env["PATH"]=os.pathsep.join(([str(a.runtime_dir.resolve())] if a.runtime_dir else [])+[os.environ["SystemRoot"]+"/System32",os.environ["SystemRoot"]])
    def run(name,cmd,code=0):
        cmd=list(map(str,cmd)); x=subprocess.run(cmd,capture_output=True,env=env); log=a.output/(name+".log"); log.write_bytes(x.stdout+x.stderr)
        result["commands"].append({"name":name,"argv":cmd,"returncode":x.returncode,"expected":code,"log":log.name,"log_sha256":sha(log)})
        (a.output/"validation.json").write_text(json.dumps(result,indent=2)+"\n",encoding="utf8")
        assert x.returncode==code,log.read_text(encoding="utf8",errors="replace")
        return json.loads(x.stdout) if x.stdout and code==0 else None
    native=a.output/"native"; native.mkdir()
    result["native_report"]=run("native",[a.test,"validate",a.manifest.resolve(),a.fixtures.resolve(),native.resolve()])
    suite=json.loads((a.fixtures/"fixtures.json").read_text(encoding="utf8"))
    for file in ["point.ome.xml","locked.ome.xml","primitives.ome.xml","multi-second.ome.xml","annotation.ome.xml"]:
        e=next(v for v in suite["positive"] if v["file"]==file); output=a.output/(file+".qpproj")
        report=run("cli-"+file,[a.cli,"--format-plugins",a.manifest.resolve(),"annotations-import","org.qupathxx.ome-roi",native/"base.qpproj",1,a.fixtures.resolve()/file,output,"--plugin-options-json",json.dumps(e["options"])])
        expected_=json.loads((native/(file+".preview.json")).read_text())
        assert report["preview_sha256"]==expected_["preview_sha256"] and report["receipt"]==expected_["receipt"]
        run("compare-"+file,[a.test,"compare","unused",native/(file+".qpproj"),output])
    for file in ["ambiguous.ome.xml","external.ome.xml","rotated-ellipse.ome.xml","mask.ome.xml","outside.ome.xml"]:
        output=a.output/(file+".qpproj");run("refuse-"+file,[a.cli,"--format-plugins",a.manifest.resolve(),"annotations-import","org.qupathxx.ome-roi",native/"base.qpproj",1,a.fixtures.resolve()/file,output],2);assert not output.exists()
    run("offline",[a.test,"offline","unused",a.fixtures.resolve(),native.resolve()])
    assert all(sha(Path(n))==s for n,s in result["artifacts"].items())
    assert all(sha(a.fixtures/n)==s for n,s in suite["sha256"].items())
    result["passed"]=True;(a.output/"validation.json").write_text(json.dumps(result,indent=2)+"\n",encoding="utf8")
    print(json.dumps({"passed":True,"native_report":result["native_report"],"native_commands":len(result["commands"])}))


if __name__=="__main__": main()
