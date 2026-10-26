"""Independent seam oracle and the existing bounded native tracer/producer."""
from fractions import Fraction
import importlib.util
import json
from pathlib import Path
import struct
import subprocess
import sys

import numpy as np
import pytest
import qupathxx as q

ROOT=Path(__file__).resolve().parents[1]
PATH=ROOT/'tools/reference_raw_contour_seams.py'
spec=importlib.util.spec_from_file_location('raw_contour_reference',PATH)
reference=importlib.util.module_from_spec(spec)
exec(compile(PATH.read_bytes(),str(PATH),'exec',dont_inherit=True),reference.__dict__)
FIXTURE=ROOT/'tests/fixtures/raw-contour-seams-v1/reference.json'


def test_reference_is_byte_reproducible_and_existing_capture_is_preserved(tmp_path):
    assert reference.encoded()==FIXTURE.read_bytes()
    output=tmp_path/'reference.json'
    command=[sys.executable,'-I',str(PATH),str(output)]
    subprocess.run(command,check=True,capture_output=True,timeout=15)
    assert output.read_bytes()==FIXTURE.read_bytes()
    result=subprocess.run(command,capture_output=True,timeout=15)
    assert result.returncode!=0 and output.read_bytes()==FIXTURE.read_bytes()


@pytest.mark.parametrize('name,expected',[
    ('nested_holes',[(1,16,1,16,32),(2,8,1,8,16)]),
    ('diagonal_contacts',[(1,4,4,4,16)]),
    ('three_class_junction',[(1,5,1,5,10),(2,7,1,7,16),(3,4,1,4,10)]),
    ('cropped_anisotropic',[(1,12,1,105,72),(2,4,1,35,24)]),
])
def test_reference_matches_hand_counted_geometry_and_exposes_false_seams(name,expected):
    case=next(c for c in reference.reference()['cases'] if c['name']==name)
    assert [(c['label'],c['pixels'],c['parts'],Fraction(c['area']),Fraction(c['perimeter']))
            for c in case['classes']]==expected
    boundary=reference.edges(case['rows'],case['first'])
    for tiling in case['tilings']:
        blocks=reference.tile_edges(case,tiling['width'],tiling['height'])
        assert reference.cancel_seams(blocks)==boundary
        assert reference.cancel_seams(reversed(blocks))==boundary
    # Joining is observable: concatenating individually closed tiles invents edges.
    # Isolated diagonal cells have only point contacts: there is no edge to cancel.
    assert any(t['naive_edges']>t['joined_edges'] for t in case['tilings'])==(name!='diagonal_contacts')


def test_crop_phase_cannot_be_replaced_by_local_tile_coordinates():
    case=next(c for c in reference.reference()['cases'] if c['name']=='cropped_anisotropic')
    fx,fy=case['first'];blocks=reference.tile_edges(case,2,3)
    displaced=[{label:[(x0-fx,y0-fy,x1-fx,y1-fy) for x0,y0,x1,y1 in segments]
                for label,segments in block.items()} for block in blocks]
    assert reference.cancel_seams(displaced)!=reference.edges(case['rows'],case['first'])
    assert Fraction(case['origin'][0])+fx*Fraction(30,12)==Fraction(17,2)
    assert Fraction(case['origin'][1])+fy*Fraction(63,18)==Fraction(25,2)


def write_float_tiff(path,case):
    width,height=case['image_size'];sx,sy=[Fraction(e,n) for e,n in zip(case['extent'],case['samples'])]
    # The cropped region has fractional edges through source pixels. Its source
    # halo must be finite; the captured ROI excludes outside samples. Unit-grid
    # unclassified cells instead contain NaNs over their entire footprints.
    values=np.full((height,width,2),0 if case['name']=='cropped_anisotropic' else np.nan,dtype='<f4')
    for y in range(height):
        iy=int((Fraction(2*y+1,2)-case['origin'][1])//sy)-case['first'][1]
        for x in range(width):
            ix=int((Fraction(2*x+1,2)-case['origin'][0])//sx)-case['first'][0]
            if 0<=iy<len(case['rows']) and 0<=ix<len(case['rows'][0]):
                label=int(case['rows'][iy][ix])
                if label:values[y,x,:]=(label-1)*100
    data=bytearray(160+values.nbytes)
    struct.pack_into('<2sHIH',data,0,b'II',42,8,12)
    tags=[(256,4,1,width),(257,4,1,height),(258,3,2,0x00200020),
          (259,3,1,1),(262,3,1,1),(273,4,1,160),(277,3,1,2),
          (278,4,1,height),(279,4,1,values.nbytes),(284,3,1,1),
          (338,3,1,0),(339,3,2,0x00030003)]
    for i,tag in enumerate(tags):struct.pack_into('<HHII',data,10+12*i,*tag)
    data[160:]=values.tobytes();path.write_bytes(data)


def geometry_edges(geometry,case):
    sx,sy=[Fraction(e,n) for e,n in zip(case['extent'],case['samples'])]
    result=[]
    for ring in geometry.paths:
        points=[]
        for point in ring:
            x=(point.x-case['origin'][0])/float(sx)
            y=(point.y-case['origin'][1])/float(sy)
            assert abs(x-round(x))<1e-12 and abs(y-round(y))<1e-12
            points.append((round(x),round(y)))
        for (x0,y0),(x1,y1) in zip(points,points[1:]+points[:1]):
            assert x0==x1 or y0==y1
            length=abs(x1-x0)+abs(y1-y0)
            if not length:continue
            dx,dy=(x1-x0)//length,(y1-y0)//length
            for i in range(length):
                a=(x0+i*dx,y0+i*dy);b=(a[0]+dx,a[1]+dy)
                result.append(tuple(sorted((a,b))))
    assert len(result)==len(set(result))
    return sorted(result)


@pytest.mark.skipif(not (q.has_tiff and q.has_geos and q.has_classifier_training),
                    reason='quantitative TIFF, GEOS and actual forest training')
@pytest.mark.parametrize('case',reference.reference()['cases'],ids=lambda case:case['name'])
def test_existing_full_tracer_and_stream_match_independent_seam_oracle(tmp_path,case):
    source=tmp_path/'source.tif';write_float_tiff(source,case);server=q.open_tiff(str(source))
    try:
        doc=q.Document(server);width,height=case['image_size']
        axes=[q.ScientificGridAxis(o,e,n) for o,e,n in zip(case['origin'],case['extent'],case['samples'])]
        grid=q.ScientificGrid(width,height,*axes);lattice=q.ScientificRawLattice(grid)
        selection=q.ScientificRawOptions();selection.channels=2
        features=q.RawPixelFeatureOptions();features.offsets=[0,0];features.scales=[1,1]
        recipe=q.RawPixelRecipe(server.raw_metadata,grid,selection,features)
        samples=np.repeat(np.array([[0,0],[100,100],[200,200]],np.float32),32,axis=0)
        labels=np.repeat(np.arange(3,dtype=np.uint32),32)
        parameters=q.ForestParameters();parameters.trees=5;parameters.max_depth=4;parameters.seed=1729
        forest=q.train_forest(samples,labels,recipe.feature_schema,['A','B','C'],parameters)
        model=q.RawPixelClassifier(forest,recipe)
        sx,sy=[Fraction(e,n) for e,n in zip(case['extent'],case['samples'])]
        fx,fy=case['first'];rows=case['rows'];w,h=len(rows[0]),len(rows)
        roi=doc.add_roi(q.Roi.rectangle(q.Rect(float(case['origin'][0]+fx*sx),
            float(case['origin'][1]+fy*sy),float(w*sx),float(h*sy))))
        request=q.RawPixelPredictionRequest(doc,model,grid,roi)
        prediction=q.predict_raw_pixel_region(request)
        expected=np.array([[int(label) for label in row] for row in rows],np.uint32)
        complete=np.zeros((case['samples'][1],case['samples'][0]),np.uint32)
        complete[fy:fy+h,fx:fx+w]=expected
        np.testing.assert_array_equal(prediction.labels_numpy(),complete)
        traced=q.trace_raw_pixel_prediction(request,prediction)
        by_label={c.label:c for c in traced.contours.components}
        assert set(by_label)=={c['label'] for c in case['classes']}
        for c in case['classes']:
            native=by_label[c['label']];geometry=native.geometry
            assert native.pixels==c['pixels']
            # Every coordinate and length in these fixtures is a binary64-exact
            # integer or half-integer, so no numeric tolerance is necessary.
            assert geometry.area==float(Fraction(c['area']))
            assert geometry.perimeter==float(Fraction(c['perimeter']))
            assert len(geometry.polygon_parts)==c['parts']
            assert sum(len(part.holes) for part in geometry.polygon_parts)==c['holes']
            target=sorted(tuple(sorted(((x0,y0),(x1,y1)))) for x0,y0,x1,y1 in c['edges'])
            assert geometry_edges(geometry,case)==target
        for t in case['tilings']:
            options=q.RawPixelStreamOptions();options.tile_width=t['width'];options.tile_height=t['height']
            region=q.RawPixelStreamRegion();region.first_x=fx;region.first_y=fy;region.width=w;region.height=h
            stream=q.RawPixelPredictionStream(q.RawPixelTiledPredictionRequest(doc,model,lattice,options,region,roi))
            visited=np.zeros((h,w),np.uint8)
            while (block:=stream.next()) is not None:
                x=block.window.x.first_core_index-fx;y=block.window.y.first_core_index-fy
                bh,bw=block.labels_numpy().shape
                np.testing.assert_array_equal(block.labels_numpy(),expected[y:y+bh,x:x+bw])
                visited[y:y+bh,x:x+bw]+=1
            assert np.all(visited==1) and stream.state=='complete'
    finally:
        server.close()
