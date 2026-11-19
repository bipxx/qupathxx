"""Independent authored ASAP XML/float32 fixtures and native workflow controls.

Python is development control/oracles only, never a product format interpreter.
No upstream image, annotation dataset or model is used or admitted.
"""
import argparse
import hashlib
import json
import os
import shutil
from pathlib import Path
import struct
import subprocess
from xml.dom import minidom
from xml.sax.saxutils import quoteattr
from check_ome_roi import tree, attr_value, chunks, elements
from check_imagej_roi import qro
from check_format_family_plugins import quantitative_tiff
from check_annotation_source_contract import volume_tiff

sha=lambda path:hashlib.sha256(path.read_bytes()).hexdigest()
f32=lambda value:struct.unpack('<f',struct.pack('<f',float(value)))[0]


def expected(source,geometries):
    root=minidom.parseString(source).documentElement
    annotations=next(n for n in elements(root) if n.tagName=='Annotations')
    group_nodes=[n for n in elements(root) if n.tagName=='AnnotationGroups']
    groups={n.getAttribute('Name'):n for n in elements(group_nodes[0])} if group_nodes else {}
    def rgb(n):
        value=n.getAttribute('Color')
        return int(value[1:],16) if value else 0xf4fa58
    out=[]
    for index,n in enumerate(elements(annotations)):
        name=n.getAttribute('Name');member=n.getAttribute('PartOfGroup');parent=member;ancestry=[]
        while parent!='None':
            ancestry.append(parent);parent=groups[parent].getAttribute('PartOfGroup')
        root_group=ancestry[-1] if ancestry else ''
        color=rgb(groups[root_group]) if ancestry else rgb(n)
        count=len(elements(next(c for c in elements(n) if c.tagName=='Coordinates')))
        facts={'asap.type':n.getAttribute('Type'),'asap.name':name,'asap.index':str(index),
               'asap.coordinates':str(count),'asap.coordinate_profile':'level-zero-image-pixels/asap-double-to-binary32/1',
               'asap.source_planes':'all/unpositioned','asap.group':member,'asap.groups':str(len(groups)),
               'asap.annotation_color_rgb':str(rgb(n)),'asap.effective_color_rgb':str(color),
               'asap.root_group':root_group,'asap.group_depth':str(len(ancestry))}
        chunks(facts,'asap.root.attributes',attr_value(root));chunks(facts,'asap.annotation.tree','QOX2'+tree(n))
        if group_nodes:chunks(facts,'asap.groups.tree','QOX2'+tree(group_nodes[0]))
        ident='annotation/'+str(index)
        out.append({'id':ident,'name':name or ident,'class':member if member!='None' else '',
                    'c':-1,'z':0,'t':0,'color':color,'locked':False,'geometry':geometries[index],'facts':facts})
    return out


def fixtures(root):
    root.mkdir(parents=True,exist_ok=False)
    suite={'schema':'qpx.asap-xml-fixtures/1','positive':[],'negative':[]}
    def annotation(kind='Dot',points=((1.25,2.5),),name='Shared μ',group='None',color='#123456',order=None,extra=''):
        points=list(points);order=range(len(points)) if order is None else order
        coords=''.join('<Coordinate Order='+quoteattr(str(i))+' X='+quoteattr(str(points[i][0]))+' Y='+quoteattr(str(points[i][1]))+'/>' for i in order)
        return '<Annotation Name='+quoteattr(name)+' Type='+quoteattr(kind)+' PartOfGroup='+quoteattr(group)+((' Color='+quoteattr(color)) if color is not None else '')+' '+extra+'><Coordinates>'+coords+'</Coordinates></Annotation>'
    def group(name='N',parent='None',color='#FF0000',values=''):
        return '<Group Name='+quoteattr(name)+' PartOfGroup='+quoteattr(parent)+((' Color='+quoteattr(color)) if color is not None else '')+'><Attributes>'+values+'</Attributes></Group>'
    def doc(annotations,groups='',with_groups=True):
        return '<ASAP_Annotations><Annotations>'+annotations+'</Annotations>'+('<AnnotationGroups>'+groups+'</AnnotationGroups>' if with_groups else '')+'</ASAP_Annotations>'
    def pos(file,source,geometries=None,encoding='utf8'):
        data=source.encode(encoding);(root/file).write_bytes(data);geometries=geometries or [qro(5,[(1.25,2.5)])]
        suite['positive'].append({'file':file,'options':{},'rows':expected(data,geometries)})
    def neg(file,source,options=None):
        (root/file).write_bytes(source.encode('utf8') if isinstance(source,str) else source);suite['negative'].append({'file':file,'options':options or {}})
    pos('point.asap.xml',doc(annotation()))
    polygon=[(1,1),(9,1),(9,8),(1,8)];line=[(1,2),(6,5)];points=[(1,2),(3,4),(5,2)]
    kinds=[('Polygon',polygon,qro(2,[(polygon,[])])),('Rectangle',polygon,qro(0,[1,1,8,7])),
           ('Measurement',line,qro(4,line)),('PointSet',points,qro(5,points))]
    for kind,pts,geo in kinds:pos(kind.lower()+'.asap.xml',doc(annotation(kind,pts)),[geo])
    pos('primitives.asap.xml',doc(annotation()+''.join(annotation(k,pts) for k,pts,g in kinds)),[qro(5,[(1.25,2.5)])]+[g for _,_,g in kinds])
    pos('group.asap.xml',doc(annotation(group='N'),group()))
    hierarchy=group('Child','Root','#445566','<Attribute Label="Cell &amp; μ"/><Attribute score="0.9"/>')+group('Root','None','#112233')
    pos('hierarchy.asap.xml',doc(annotation(group='Child'),hierarchy))
    pos('defaults.asap.xml',doc(annotation(color=None)))
    pos('group-default.asap.xml',doc(annotation(group='N'),group(color=None)))
    pos('empty-color.asap.xml',doc(annotation(color='')))
    pos('lower-hex.asap.xml',doc(annotation(color='#abCdEf')))
    pos('empty-name.asap.xml',doc(annotation(name='')))
    pos('no-groups.asap.xml',doc(annotation(),with_groups=False))
    pos('unused-group.asap.xml',doc(annotation(),group('Unused','None','#aaff00')))
    pos('duplicate-names.asap.xml',doc(annotation()+annotation(points=[(3,4)])),[qro(5,[(1.25,2.5)]),qro(5,[(3,4)])])
    pos('coordinate-order.asap.xml',doc(annotation('Polygon',polygon,order=[3,1,0,2])),[qro(2,[(polygon,[])])])
    pos('reverse-rectangle.asap.xml',doc(annotation('Rectangle',list(reversed(polygon)))),[qro(0,[1,1,8,7])])
    pos('closed-polygon.asap.xml',doc(annotation('Polygon',polygon+[polygon[0]])),[qro(2,[(polygon,[])])])
    for name,x,y in [('float32','1.23456789','2.987654321'),('tie-even','1.000000059604644775390625','2.5'),
                     ('tie-odd','1.000000178813934326171875','2.5'),('double-then-float','1.0000000596046448','2.5'),
                     ('signed-zero','-0','0'),('subnormal','1.401298464324817e-45','2.5'),('exponent','+1.25e0','2.5E0')]:
        pos(name+'.asap.xml',doc(annotation(points=[(x,y)])),[qro(5,[(f32(x),f32(y))])])
    pos('utf16le.asap.xml','<?xml version="1.0" encoding="UTF-16"?>'+doc(annotation()),encoding='utf16')
    pos('utf16be.asap.xml','\ufeff<?xml version="1.0" encoding="UTF-16"?>'+doc(annotation()),encoding='utf-16-be')
    pos('bom.asap.xml','\ufeff'+doc(annotation()))
    pos('名字-μ.asap.xml',doc(annotation(name='同名 μ α')))
    pos('attribute-context.asap.xml',doc(annotation(group='N'),group(values='<Attribute xmlns:p="urn:authored:units" Kind="p:Intensity" p:key="μ"/>')))
    pos('long-attribute.asap.xml',doc(annotation(group='N'),group(values='<Attribute Comment='+quoteattr('μ'*5000)+'/>')))
    pos('whitespace.asap.xml',doc(annotation()).replace('><','>\n\t<'))
    base=doc(annotation())
    for name,source in [('empty',doc('')),('no-root','<Annotations/>'),('wrong-root',base.replace('ASAP_Annotations','AperioAnnotations')),
                        ('namespace',base.replace('<ASAP_Annotations>','<ASAP_Annotations xmlns="urn:unknown">')),
                        ('unknown-child',base.replace('</ASAP_Annotations>','<Unknown/></ASAP_Annotations>')),
                        ('unknown-attribute',base.replace('Name="Shared μ"','Name="Shared μ" Units="micrometre"')),
                        ('missing-name',base.replace('Name="Shared μ" ','')),('missing-type',base.replace('Type="Dot" ','')),
                        ('missing-group',base.replace('PartOfGroup="None" ','')),('missing-order',base.replace('Order="0" ','')),
                        ('missing-x',base.replace('X="1.25" ','')),('missing-y',base.replace(' Y="2.5"','')),
                        ('repeated-coordinates',base.replace('</Annotation>','<Coordinates/></Annotation>')),
                        ('unknown-coordinate-child',base.replace('<Coordinate ','<Point ')),('coordinate-text',base.replace('/></Coordinates>','/>oops</Coordinates>')),
                        ('annotation-text',base.replace('<Coordinates>','oops<Coordinates>')),
                        ('duplicate-attribute',base.replace('X="1.25"','X="1.25" X="2"')),
                        ('wrong-case',base.replace('Type="Dot"','Type="dot"')),
                        ('spline',base.replace('Type="Dot"','Type="Spline"')),('none-type',base.replace('Type="Dot"','Type="None"')),
                        ('negative-space',base.replace('Type="Dot"','Type="Dot" NegativeROA="1"')),
                        ('source-plane',base.replace('Type="Dot"','Type="Dot" TheZ="1"')),
                        ('transform',base.replace('</Annotation>','<Transform A00="1"/></Annotation>')),
                        ('named-color',base.replace('#123456','red')),('alpha-color',base.replace('#123456','#80123456')),
                        ('invalid-color',base.replace('#123456','#XX3456'))]:neg(name+'.asap.xml',source)
    for name,value in [('nan','NaN'),('inf','INF'),('f32-overflow','1e39'),('double-overflow','1e999'),
                       ('nonzero-underflow','1e-50'),('double-underflow','1e-999'),('negative','-1'),('hex','0x1p2'),('partial','1.2x'),('bad-sign','+-1')]:
        neg(name+'.asap.xml',base.replace('X="1.25"','X='+quoteattr(value)))
    for name,value in [('negative-order','-1'),('large-order','18446744073709551615'),('fractional-order','0.5'),('nonzero-order','1')]:
        neg(name+'.asap.xml',base.replace('Order="0"','Order='+quoteattr(value)))
    for name,source in [('duplicate-order',doc(annotation('Measurement',line)).replace('Order="1"','Order="0"')),
                        ('gap-order',doc(annotation('Measurement',line)).replace('Order="1"','Order="2"')),
                        ('dot-multiple',doc(annotation('Dot',line))),('measurement-one',base.replace('Type="Dot"','Type="Measurement"')),
                        ('measurement-many',doc(annotation('Measurement',points))),('polygon-two',doc(annotation('Polygon',line))),
                        ('crossing',doc(annotation('Polygon',[(1,1),(8,8),(8,1),(1,8)]))),
                        ('rectangle-two',doc(annotation('Rectangle',line))),('rotated-rectangle',doc(annotation('Rectangle',[(1,2),(4,1),(5,4),(2,5)]))),
                        ('wrong-rectangle-order',doc(annotation('Rectangle',polygon,order=[0,1,2,3])).replace('Order="1"','Order="9"')),
                        ('repeated-corner',doc(annotation('Rectangle',[polygon[0],polygon[1],polygon[1],polygon[3]])))]:neg(name+'.asap.xml',source)
    for name,groups_,membership in [('duplicate-group',group()+group(),'N'),('empty-group-name',group(''),'None'),
                                  ('reserved-group',group('None'),'None'),('missing-parent',group('N','Missing'),'N'),
                                  ('self-cycle',group('N','N'),'N'),('cycle',group('N','P')+group('P','N'),'N'),
                                  ('group-color',group(color='blue'),'N'),('unknown-member',group(),'Missing')]:
        neg(name+'.asap.xml',doc(annotation(group=membership),groups_))
    neg('depth65.asap.xml',doc(annotation(group='G0'),''.join(group('G'+str(i),'G'+str(i+1) if i<64 else 'None') for i in range(65))))
    for name,head in [('doctype','<!DOCTYPE ASAP_Annotations>'),('entity','<!DOCTYPE ASAP_Annotations [<!ENTITY x "v">]>'),
                      ('external','<!DOCTYPE ASAP_Annotations SYSTEM "file:///C:/secret.xml">'),('network','<!DOCTYPE ASAP_Annotations SYSTEM "https://invalid.example/asap.dtd">')]:neg(name+'.asap.xml',head+base)
    neg('unknown-entity.asap.xml',base.replace('Shared μ','&boom;'));neg('xml11.asap.xml','<?xml version="1.1"?>'+base)
    neg('invalid-utf8.asap.xml',base.encode().replace('μ'.encode(),b'\xff'))
    neg('bad-option.asap.xml',base,{'guess_units':True})
    neg('unclosed.asap.xml',base[:-6])
    (root/'outside.asap.xml').write_text(doc(annotation(points=[(20,20)])),encoding='utf8')
    (root/'parse-arena-limit.asap.xml').write_text(doc(''.join(annotation(name=str(i)) for i in range(5000))),encoding='utf8')
    (root/'xml-arena-small.asap.xml').write_text(doc(''.join(annotation(name=str(i)) for i in range(100))),encoding='utf8')
    (root/'image.tif').write_bytes(quantitative_tiff());(root/'volume.tif').write_bytes(volume_tiff())
    suite['sha256']={f.name:sha(f) for f in root.iterdir() if f.is_file()};(root/'fixtures.json').write_text(json.dumps(suite,ensure_ascii=False,indent=2)+'\n',encoding='utf8')
    return suite


def main():
    p=argparse.ArgumentParser(description=__doc__)
    for name in ['fixtures','test','manifest','cli','output','runtime-dir','reuse-native']:p.add_argument('--'+name,type=Path,required=name=='fixtures')
    a=p.parse_args()
    if not a.test:
        suite=fixtures(a.fixtures.resolve());print(json.dumps({'positive':len(suite['positive']),'negative':len(suite['negative'])}));return
    assert a.manifest and a.cli and a.output;a.output.mkdir(parents=True,exist_ok=False)
    module=a.manifest.parent/json.loads(a.manifest.read_text())['plugins'][0]['module']['path']
    result={'schema':'qpx.asap-xml-validation/1','passed':False,'artifacts':{str(f.resolve()):sha(f) for f in [a.test,a.cli,a.manifest,module]},'fixtures_sha256':sha(a.fixtures/'fixtures.json'),'commands':[]}
    env={k:v for k,v in os.environ.items() if not k.upper().startswith('PYTHON')};env['PATH']=os.pathsep.join(([str(a.runtime_dir.resolve())] if a.runtime_dir else [])+[os.environ['SystemRoot']+'/System32',os.environ['SystemRoot']])
    def run(name,args,code=0):
        args=list(map(str,args));x=subprocess.run(args,capture_output=True,env=env);log=a.output/(name+'.log');log.write_bytes(x.stdout+x.stderr)
        result['commands'].append({'name':name,'argv':args,'returncode':x.returncode,'expected':code,'log':log.name,'log_sha256':sha(log)});(a.output/'validation.json').write_text(json.dumps(result,indent=2)+'\n')
        assert x.returncode==code,log.read_text(encoding='utf8',errors='replace')
        return json.loads(x.stdout) if x.stdout and code==0 else None
    native=a.output/'native';native.mkdir()
    if a.reuse_native:
        previous=json.loads(a.reuse_native.read_text())
        assert previous['artifacts']==result['artifacts'] and previous['fixtures_sha256']==result['fixtures_sha256'],'Native reuse requires exact products and pre-recorded fixture identity'
        command=next(c for c in previous['commands'] if c['name']=='native')
        log=a.reuse_native.parent/command['log'];assert command['returncode']==command['expected']==0 and sha(log)==command['log_sha256']
        source=a.reuse_native.parent/'native';snapshot={f.relative_to(source).as_posix():sha(f) for f in source.rglob('*') if f.is_file()}
        shutil.copytree(source,native,dirs_exist_ok=True)
        assert snapshot=={f.relative_to(native).as_posix():sha(f) for f in native.rglob('*') if f.is_file()}
        result['native_report']=previous['native_report'];result['commands'].append(dict(command,log=str(log.resolve()),reused=True))
        result['reused_native_validation']={'path':str(a.reuse_native.resolve()),'sha256':sha(a.reuse_native),'scope':'Unchanged native products/fixtures/assertions; CLI option-map repair only'}
    else:result['native_report']=run('native',[a.test,'validate',a.manifest.resolve(),a.fixtures.resolve(),native.resolve()])
    result['native_outputs_sha256']={f.relative_to(native).as_posix():sha(f) for f in native.rglob('*') if f.is_file()}
    suite=json.loads((a.fixtures/'fixtures.json').read_text(encoding='utf8'))
    for file in ['point.asap.xml','primitives.asap.xml','group.asap.xml','hierarchy.asap.xml','coordinate-order.asap.xml']:
        case=next(e for e in suite['positive'] if e['file']==file);class_map={e['class']:'Native' for e in case['rows'] if e['class']}
        output=a.output/(file+'.qpproj');args=[a.cli,'--format-plugins',a.manifest.resolve(),'annotations-import','org.qupathxx.asap-xml',native/'base.qpproj',1,a.fixtures.resolve()/file,output,'--class-map-json',json.dumps(class_map)];report=run('cli-'+file,args);expected_=json.loads((native/(file+'.preview.json')).read_text());assert report['preview_sha256']==expected_['preview_sha256'] and report['receipt']==expected_['receipt'];run('compare-'+file,[a.test,'compare','unused',native/(file+'.qpproj'),output])
    for file in ['spline.asap.xml','external.asap.xml','cycle.asap.xml','nonzero-underflow.asap.xml','outside.asap.xml']:
        output=a.output/(file+'.qpproj');run('refuse-'+file,[a.cli,'--format-plugins',a.manifest.resolve(),'annotations-import','org.qupathxx.asap-xml',native/'base.qpproj',1,a.fixtures.resolve()/file,output],2);assert not output.exists()
    run('offline',[a.test,'offline','unused',a.fixtures.resolve(),native.resolve()]);assert all(sha(Path(n))==h for n,h in result['artifacts'].items());suite=json.loads((a.fixtures/'fixtures.json').read_text(encoding='utf8'));assert all(sha(a.fixtures/n)==h for n,h in suite['sha256'].items());result['passed']=True;(a.output/'validation.json').write_text(json.dumps(result,indent=2)+'\n');print(json.dumps({'passed':True,'native_report':result['native_report'],'native_commands':len(result['commands'])}))


if __name__=='__main__':main()
