"""Authored COCO polygon/RLE oracles; native import/CLI/persistence checks.

Development-only Python. No upstream dataset, image, model or annotation asset.
"""
import argparse
import copy
import ctypes
import hashlib
import json
import os
from pathlib import Path
import subprocess
from check_format_family_plugins import quantitative_tiff
from check_annotation_source_contract import volume_tiff


def sha(path):
    return hashlib.file_digest(path.open('rb'), 'sha256').hexdigest()


def counts(pixels):
    # Independent column-major encoder from literal row-major coverage.
    result = []
    current = 0
    length = 0
    for x in range(16):
        for y in range(16):
            bit = pixels[y * 16 + x]
            if bit != current:
                result.append(length)
                length = 0
                current = bit
            length += 1
    result.append(length)
    return result


def compressed(runs):
    result = ''
    for i, value in enumerate(runs):
        if i > 2:
            value -= runs[i - 2]
        while True:
            digit = value & 31
            value >>= 5
            more = value != (-1 if digit & 16 else 0)
            result += chr((digit | (32 if more else 0)) + 48)
            if not more:
                break
    return result


def bits(predicate):
    return [int(predicate(x, y)) for y in range(16) for x in range(16)]


def document(segmentation, *, ident=11, category=3, image=7, crowd=0):
    return {'info': {'description': 'Independently authored μ fixture'},
            'licenses': [{'id': 1, 'name': 'AGPL-3.0-only authored fixture', 'url': 'https://www.gnu.org/licenses/agpl-3.0.html'}],
            'images': [{'id': image, 'width': 16, 'height': 16, 'file_name': '../never-opened-image.tif', 'license': 1}],
            'categories': [{'id': category, 'name': 'Repeated name μ', 'supercategory': 'Cells'}],
            'annotations': [{'id': ident, 'image_id': image, 'category_id': category, 'segmentation': segmentation,
                             'area': 999.25, 'bbox': [-2.5, 0, 19, 20], 'iscrowd': crowd}]}


def encoded(value):
    return json.dumps(value, ensure_ascii=False, separators=(',', ':'))


def chunks(facts, key, value):
    value = encoded(value)
    raw = value.encode('utf8')
    facts[key + '.bytes'] = str(len(raw))
    index = 0
    while raw:
        cut = min(4096, len(raw))
        while cut < len(raw) and raw[cut] & 192 == 128:
            cut -= 1
        facts[key + '.' + str(index)] = raw[:cut].decode('utf8')
        raw = raw[cut:]
        index += 1


def expected(doc, selected, masks, topology=None):
    image = next(i for i in doc['images'] if i['id'] == selected)
    categories = {c['id']: c for c in doc['categories']}
    result = []
    annotations = [a for a in doc['annotations'] if a['image_id'] == selected]
    assert len(annotations) == len(masks)
    for index, (a, pixels) in enumerate(zip(annotations, masks)):
        category = categories[a['category_id']]
        facts = {'coco.profile': 'coco-instance-pixel-masks/1', 'coco.image_id': str(selected),
                 'coco.category_id': str(a['category_id']), 'coco.annotation_id': str(a['id']),
                 'coco.iscrowd': str(a['iscrowd']), 'coco.category_name': category['name'],
                 'coco.coverage': 'origin-zero-level-zero-unit-pixel-cells',
                 'coco.segmentation': 'polygon-union' if isinstance(a['segmentation'], list) else
                 'uncompressed-rle' if isinstance(a['segmentation']['counts'], list) else 'compressed-rle'}
        for key, val in [('annotation', a), ('image', image), ('category', category), ('info', doc.get('info')), ('licenses', doc.get('licenses'))]:
            if val is not None:
                chunks(facts, 'coco.' + key + '.json', val)
        row = {'id': 'annotation/' + str(a['id']), 'class': 'category/' + str(a['category_id']), 'pixels': pixels, 'facts': facts}
        if topology:
            row.update(topology[index])
        result.append(row)
    return result


def fixtures(root):
    root.mkdir(parents=True, exist_ok=False)
    suite = {'schema': 'qpx.coco-fixtures/1', 'positive': [], 'negative': []}

    def positive(name, doc, masks, image=7, topology=None):
        file = name + '.json'
        (root / file).write_text(encoded(doc), encoding='utf8')
        suite['positive'].append({'file': file, 'options': {'image_id': image}, 'rows': expected(doc, image, masks, topology)})

    rect = [[1, 2, 6, 2, 6, 7, 1, 7]]
    rectangle = bits(lambda x, y: 1 <= x < 6 and 2 <= y < 7)
    positive('rectangle', document(rect), [rectangle], topology=[{'parts': 1, 'holes': 0}])
    positive('fractional', document([[1.25, 2.25, 5.75, 2.25, 5.75, 6.75, 1.25, 6.75]]), [rectangle])
    positive('negative-clipped', document([[-5, -3, 4, -3, 4, 3, -5, 3]]), [bits(lambda x, y: x < 4 and y < 3)])
    positive('beyond-clipped', document([[13, 13, 20, 13, 20, 20, 13, 20]]), [bits(lambda x, y: x >= 13 and y >= 13)])
    positive('closed-repeated', document([[1, 2, 6, 2, 6, 2, 6, 7, 1, 7, 1, 2]]), [rectangle])
    positive('reverse', document([[1, 7, 6, 7, 6, 2, 1, 2]]), [rectangle])
    union = bits(lambda x, y: (1 <= x < 6 and 2 <= y < 7) or (4 <= x < 10 and 4 <= y < 9))
    positive('overlap-union', document(rect + [[4, 4, 10, 4, 10, 9, 4, 9]]), [union], topology=[{'parts': 1, 'holes': 0}])
    positive('nested-is-union', document([ [1,1,10,1,10,10,1,10], [3,3,7,3,7,7,3,7] ]), [bits(lambda x,y: 1<=x<10 and 1<=y<10)], topology=[{'parts':1,'holes':0}])
    disconnected = bits(lambda x,y: (1<=x<4 and 1<=y<4) or (9<=x<12 and 10<=y<13))
    positive('polygon-disconnected', document([[1,1,4,1,4,4,1,4],[9,10,12,10,12,13,9,13]]), [disconnected], topology=[{'parts':2,'holes':0}])
    # Literal triangle rows; its diagonal lies between pixel centers.
    triangle = bits(lambda x,y: 2<=x<9 and 2<=y<9 and x+y<=9)
    positive('triangle', document([[2,2,9,2,2,9]]), [triangle])
    donut = bits(lambda x,y: 2<=x<13 and 1<=y<12 and not (5<=x<9 and 4<=y<8))
    patterns = {'donut': donut, 'disconnected': disconnected, 'rectangle': rectangle,
                'all': [1]*256, 'first': [1]+[0]*255, 'last': [0]*255+[1],
                'asymmetric': bits(lambda x,y: (x==2 and 1<=y<12) or (4<=x<10 and y==3)),
                'diagonal': bits(lambda x,y: x==y and 1<=x<=5)}
    for name, pixels in patterns.items():
        topo = [{'holes':1,'parts':1}] if name=='donut' else [{'parts':2,'holes':0}] if name=='disconnected' else None
        for mode in ['raw','compressed']:
            runs = counts(pixels)
            positive('rle-'+name+'-'+mode, document({'size':[16,16],'counts':runs if mode=='raw' else compressed(runs)}, crowd=int(name=='donut')), [pixels], topology=topo)
    zero_runs = counts(rectangle)
    zero_runs = [0,0] + zero_runs
    positive('zero-length-runs', document({'size':[16,16],'counts':zero_runs}), [rectangle])
    huge = document(rect, ident=18446744073709551615, category=9007199254740993, image=9007199254740995)
    positive('exact-large-ids', huge, [rectangle], image=9007199254740995)
    multi = document(rect)
    multi['categories'].append({'id':4,'name':multi['categories'][0]['name'],'supercategory':'Other'})
    multi['annotations'].append(dict(multi['annotations'][0], id=12, category_id=4, segmentation={'size':[16,16],'counts':compressed(counts(donut))}, iscrowd=1))
    positive('instances-and-categories', multi, [rectangle,donut], topology=[{'holes':0},{'holes':1}])
    suite['positive'][-1]['rows'][1]['native_class']='Other'
    multi_image = document(rect)
    multi_image['images'].append({'id':8,'width':8,'height':8,'file_name':'https://never-fetched.invalid/a.tif'})
    multi_image['annotations'].append(dict(multi_image['annotations'][0],id=12,image_id=8,segmentation={'size':[8,8],'counts':[0,64]}))
    positive('explicit-image-selection', multi_image, [rectangle])
    # Source area/bbox intentionally disagree with raster area; they remain
    # source metadata, never become scientific measurements or calibration.
    bare = document(rect);del bare['info'];del bare['licenses']
    positive('minimal-metadata', bare, [rectangle])
    positive('unicode-metadata', dict(document(rect),info={'description':'line\nquote" μ 🧫','version':'1.0'}), [rectangle])

    def negative(name, change=None, doc=None, raw=None, options=None):
        if doc is None:
            doc = document(rect)
        if change:
            change(doc)
        file = name+'.json'
        (root/file).write_bytes(raw if raw is not None else encoded(doc).encode('utf8'))
        suite['negative'].append({'file':file,'options':{'image_id':7} if options is None else options})

    for name,key,value in [('float-id','id',11.0),('negative-id','id',-1),('overflow-id','id',18446744073709551616),
                           ('missing-image-reference','image_id',99),('missing-category-reference','category_id',99),
                           ('bad-crowd','iscrowd',2),('bool-crowd','iscrowd',True),('negative-area','area',-1),
                           ('bbox-short','bbox',[0,0,1]),('bbox-negative','bbox',[0,0,-1,1]),
                           ('empty-polygons','segmentation',[]),('short-polygon','segmentation',[[1,2,3,4]]),
                           ('odd-polygon','segmentation',[[1,1,2,2,3,3,4]]),('polygon-string','segmentation',[[1,1,2,1,'3',3]]),
                           ('outside-empty','segmentation',[[20,20,22,20,22,22,20,22]]),
                           ('zero-area','segmentation',[[1,1,1,1,1,1]]),('huge-coordinate','segmentation',[[1,1,1e9,1,1,4]])]:
        negative(name,lambda d,k=key,v=value:d['annotations'][0].__setitem__(k,v))
    for name,seg in [('rle-size-transpose',{'size':[8,32],'counts':[0,256]}),('rle-size-short',{'size':[16],'counts':[0,256]}),
                     ('rle-count-short',{'size':[16,16],'counts':[0,255]}),('rle-count-long',{'size':[16,16],'counts':[0,257]}),
                     ('rle-negative',{'size':[16,16],'counts':[0,-1,257]}),('rle-float',{'size':[16,16],'counts':[0,256.0]}),
                     ('rle-empty',{'size':[16,16],'counts':[]}),('rle-background',{'size':[16,16],'counts':[256]}),
                     ('rle-overflow',{'size':[16,16],'counts':[18446744073709551616]}),
                     ('rle-compressed-empty',{'size':[16,16],'counts':''}),('rle-invalid-character',{'size':[16,16],'counts':'/' }),
                     ('rle-invalid-high-character',{'size':[16,16],'counts':'p'}),('rle-truncated',{'size':[16,16],'counts':'P'}),
                     ('rle-negative-delta',{'size':[16,16],'counts':'O'}),('rle-shift-overflow',{'size':[16,16],'counts':'oooooooo0'}),
                     ('rle-unknown-field',{'size':[16,16],'counts':[0,256],'order':'C'})]:
        negative(name,lambda d,s=seg:d['annotations'][0].__setitem__('segmentation',s))
    for name,section,field,value in [('units-extension','images','units','micrometre'),('keypoints-schema','annotations','keypoints',[]),
                                     ('panoptic-schema','annotations','segments_info',[]),('unknown-category','categories','color',[1,2,3]),
                                     ('zero-width','images','width',0),('float-width','images','width',16.0),
                                     ('canvas-mismatch','images','width',15),('canvas-large','images','width',1000000)]:
        negative(name,lambda d,s=section,f=field,v=value:d[s][0].__setitem__(f,v))
    for section in ['images','categories','annotations']:
        negative('duplicate-'+section,lambda d,s=section:d[s].append(copy.deepcopy(d[s][0])))
    for key in ['segmentation','area','bbox','iscrowd','id']:
        negative('missing-'+key,lambda d,k=key:d['annotations'][0].pop(k))
    negative('no-image-choice',options={})
    negative('absent-image-choice',options={'image_id':99})
    negative('floating-image-choice',options={'image_id':7.0})
    negative('unsupported-options',options={'image_id':7,'scale':2})
    raw = encoded(document(rect)).encode('utf8')
    for name,data in [('duplicate-member',raw.replace(b'"id":11',b'"id":11,"id":12')),
                      ('duplicate-root',raw[:-1]+b',"images":[]}'),('trailing',raw+b'{}'),('comment',b'/*comment*/'+raw),
                      ('utf8-invalid',raw.replace('μ'.encode(),b'\xff')),('nul-escape',raw.replace(b'Cells',b'\\u0000')),
                      ('bad-surrogate',raw.replace(b'Cells',b'\\ud800')),('trailing-comma',raw.replace(b'"iscrowd":0}',b'"iscrowd":0,}')),
                      ('leading-zero',raw.replace(b'"id":11',b'"id":011')),('nan',raw.replace(b'999.25',b'NaN')),
                      ('overflow-real',raw.replace(b'999.25',b'1e999')),('deep-json',b'{"info":'+b'['*34+b'0'+b']'*34+b'}')]:
        negative(name,raw=data)
    arena=document(rect)
    arena['annotations']=[dict(arena['annotations'][0],id=i) for i in range(1000)]
    (root/'arena.json').write_text(encoded(arena),encoding='utf8')
    (root/'image.tif').write_bytes(quantitative_tiff())
    (root/'volume.ome.tif').write_bytes(volume_tiff())
    suite['sha256']={p.name:sha(p) for p in root.iterdir() if p.is_file()}
    (root/'fixtures.json').write_text(json.dumps(suite,ensure_ascii=False,indent=2)+'\n',encoding='utf8')
    return suite


def reference(path, root, suite):
    lib=ctypes.CDLL(str(path.resolve()))
    lib.qpx_coco_reference_polygon.argtypes=[ctypes.POINTER(ctypes.c_double),ctypes.c_uint32,ctypes.c_uint32,ctypes.c_uint32,ctypes.POINTER(ctypes.c_uint8)]
    lib.qpx_coco_reference_polygon.restype=ctypes.c_int
    lib.qpx_coco_reference_rle.argtypes=[ctypes.c_char_p,ctypes.c_uint32,ctypes.c_uint32,ctypes.POINTER(ctypes.c_uint8)]
    lib.qpx_coco_reference_rle.restype=ctypes.c_int
    checks=0
    for case in suite['positive']:
        source=json.loads((root/case['file']).read_text(encoding='utf8'))
        annotations=[r for r in source['annotations'] if r['image_id']==case['options']['image_id']]
        for ann, expected_ in zip(annotations,case['rows']):
            segmentation=ann['segmentation']
            actual=[0]*256
            if isinstance(segmentation,list):
                for polygon in segmentation:
                    coordinates=(ctypes.c_double*len(polygon))(*polygon)
                    mask=(ctypes.c_uint8*256)()
                    assert lib.qpx_coco_reference_polygon(coordinates,len(polygon)//2,16,16,mask)==1
                    actual=[a|int(mask[x*16+y]) for y in range(16) for x,a in enumerate(actual[y*16:(y+1)*16])]
            elif isinstance(segmentation['counts'],str):
                mask=(ctypes.c_uint8*256)()
                assert lib.qpx_coco_reference_rle(segmentation['counts'].encode('ascii'),16,16,mask)==1
                actual=[int(mask[x*16+y]) for y in range(16) for x in range(16)]
            else:
                continue
            assert actual==expected_['pixels'],case['file']+': independent literal oracle differs from pinned COCO C reference'
            checks+=1
    return checks


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    for name in ['fixtures','test','manifest','cli','output','runtime-dir','reference']:
        parser.add_argument('--'+name,type=Path,required=name=='fixtures')
    a=parser.parse_args()
    if not a.test:
        s=fixtures(a.fixtures.resolve());print(json.dumps({'positive':len(s['positive']),'negative':len(s['negative'])}));return
    assert a.manifest and a.cli and a.output
    a.output.mkdir(parents=True,exist_ok=False)
    manifest=json.loads(a.manifest.read_text())
    module=a.manifest.parent/manifest['plugins'][0]['module']['path']
    suite=json.loads((a.fixtures/'fixtures.json').read_text(encoding='utf8'))
    result={'schema':'qpx.coco-validation/1','passed':False,'artifacts':{str(p.resolve()):sha(p) for p in [a.test,a.cli,a.manifest,module]},
            'fixtures_sha256':sha(a.fixtures/'fixtures.json'),'commands':[]}
    env={k:v for k,v in os.environ.items() if not k.upper().startswith('PYTHON')}
    env['PATH']=os.pathsep.join(([str(a.runtime_dir.resolve())] if a.runtime_dir else [])+[os.environ['SystemRoot']+'/System32',os.environ['SystemRoot']])
    def run(name,args,code=0):
        args=list(map(str,args));r=subprocess.run(args,capture_output=True,env=env)
        log=a.output/(name+'.log');log.write_bytes(r.stdout+r.stderr)
        result['commands'].append({'name':name,'argv':args,'returncode':r.returncode,'expected':code,'log':log.name,'log_sha256':sha(log)})
        (a.output/'validation.json').write_text(json.dumps(result,indent=2)+'\n',encoding='utf8')
        assert r.returncode==code,log.read_text(encoding='utf8',errors='replace')
        return json.loads(r.stdout) if r.stdout and code==0 else None
    if a.reference:
        result['reference']={'path':str(a.reference.resolve()),'sha256':sha(a.reference),'checks':reference(a.reference,a.fixtures,suite)}
    native=a.output/'native';native.mkdir()
    result['native_report']=run('native',[a.test,'validate',a.manifest.resolve(),a.fixtures.resolve(),native.resolve()])
    result['native_outputs_sha256']={p.relative_to(native).as_posix():sha(p) for p in native.rglob('*') if p.is_file()}
    for file in ['rectangle.json','rle-donut-raw.json','rle-donut-compressed.json','exact-large-ids.json','instances-and-categories.json','explicit-image-selection.json']:
        case=next(e for e in suite['positive'] if e['file']==file)
        mapping={r['class']:r.get('native_class','Native') for r in case['rows']}
        output=a.output/(file+'.qpproj')
        report=run('cli-'+file,[a.cli,'--format-plugins',a.manifest.resolve(),'annotations-import','org.qupathxx.coco',native/'base.qpproj',1,a.fixtures.resolve()/file,output,
                               '--plugin-options-json',encoded(case['options']),'--class-map-json',encoded(mapping)])
        reviewed=json.loads((native/(file+'.preview.json')).read_text(encoding='utf8'))
        assert report['preview_sha256']==reviewed['preview_sha256'] and report['receipt']==reviewed['receipt']
        run('persist-'+file,[a.test,'reopen',output,native/(file+'.expected.json')])
    for file in ['canvas-mismatch.json','rle-truncated.json','duplicate-member.json','keypoints-schema.json','missing-category-reference.json']:
        output=a.output/(file+'.qpproj')
        run('refuse-'+file,[a.cli,'--format-plugins',a.manifest.resolve(),'annotations-import','org.qupathxx.coco',native/'base.qpproj',1,a.fixtures.resolve()/file,output,
                           '--plugin-options-json','{"image_id":7}','--class-map-json','{"category/3":"Native"}'],2)
        assert not output.exists()
    # Fresh process with no manifest proves persisted geometry/source facts are
    # independent of the optional provider DLL.
    run('provider-free-reopen',[a.test,'reopen',native/'rle-donut-compressed.json.qpproj',native/'rle-donut-compressed.json.expected.json'])
    assert all(sha(Path(p))==v for p,v in result['artifacts'].items())
    assert all(sha(a.fixtures/p)==v for p,v in suite['sha256'].items())
    result['passed']=True
    (a.output/'validation.json').write_text(json.dumps(result,indent=2)+'\n',encoding='utf8')
    print(json.dumps({'passed':True,'native_report':result['native_report'],'commands':len(result['commands'])}))


if __name__=='__main__':
    main()
