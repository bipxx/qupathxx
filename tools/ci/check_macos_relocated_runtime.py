"""Run trusted relocated native Mac bytes from a fresh archive extraction.

Checks exact pixels, synthetic project OD/checkpoint/CSV and GEOS union/mappings.
These are host engineering probes, not biology or an older-OS support matrix.
"""
import argparse
import csv
import ctypes
import hashlib
import importlib.util
import io
import json
import math
import os
from pathlib import Path
import platform
import posixpath
import re
import sqlite3
import struct
import subprocess
import sys
import zipfile

spec=importlib.util.spec_from_file_location('qpx_mac_runtime_relocation',Path(__file__).with_name('macos_relocate.py'))
relocation=importlib.util.module_from_spec(spec);spec.loader.exec_module(relocation)
common=relocation.common
require=relocation.require
OD_TOLERANCE=2e-7


def sha(path):
    with Path(path).open('rb') as stream:return common.file_hash(stream)


def closure(plan,root):
    outgoing={}
    for edge in plan['dependency_edges']:outgoing.setdefault(edge['source'],[]).append(edge)
    pending,visited=[root],set()
    while pending:
        current=pending.pop()
        if current in visited:continue
        visited.add(current)
        pending.extend(edge['target'] for edge in outgoing.get(current,[]) if edge['target'])
    return sorted(path for path in visited if path.startswith('lib/'))


def csv_contract(data):
    reader=csv.DictReader(io.StringIO(data));rows=list(reader)
    require(reader.fieldnames is not None and len(reader.fieldnames)==len(set(reader.fieldnames)),'duplicate/missing relocated CSV columns')
    require(len(rows)==1,'relocated batch CSV requires one selected ROI record')
    row=rows[0]
    for key,value in dict(table_schema='qpx.roi-table/1',algorithm='qpx.roi-channel-means/1',
                          status_at_capture='current',channel_space='optical_density_rgb',batch_image_id='1',source_object_id='1').items():
        require(row[key]==value,'relocated batch CSV identity differs: '+key)
    for key,value in dict(samples=4,roi_area_px2=4,roi_area_um2=0.5,roi_length_um=3,
                          roi_min_x_px=0,roi_min_y_px=0,roi_width_px=2,roi_height_px=2,nucleus=0,roi_kind=0,
                          white_r=255,white_g=255,white_b=255,
                          captured_pixel_width_um=0.5,captured_pixel_height_um=0.25).items():
        observed=float(row[key]);require(math.isfinite(observed) and observed==value,'relocated CSV count/calibration differs: '+key)
    means=[]
    for i,sample in enumerate([30,60,90],1):
        observed=float(row['mean_channel_'+str(i)])
        require(math.isfinite(observed) and abs(observed-math.log10(255/sample))<=OD_TOLERANCE,'relocated optical density differs')
        means.append(observed)
    require(re.fullmatch('[0-9a-f]{64}',row['source_tree_sha256']) is not None,'relocated provenance hash is absent')
    return dict(rows=1,samples=4,roi_area_px2=4,roi_area_um2=0.5,roi_length_um=3,
                means=means,absolute_tolerance=OD_TOLERANCE,source_tree_sha256=row['source_tree_sha256'])


def batch_contract(data):
    lines=[json.loads(line,object_pairs_hook=common.unique_object) for line in data.splitlines() if line.strip()]
    require(len(lines)==2 and lines[-1]['schema']=='qpx.project-batch/1','relocated batch JSONL cohort differs')
    result=lines[-1]
    require(all(type(result[key]) is int for key in ['saved','failed','reused']),'relocated batch count types differ')
    require(result['state']=='succeeded' and result['saved']==1 and result['failed']==0 and result['reused']==0,'relocated batch did not save one checkpoint')
    require(len(result['images'])==1 and result['images'][0]['image_id']==1 and result['images'][0]['records']==1,'relocated batch selected records differ')
    image=result['images'][0]
    require(all(type(image[key]) is int for key in ['image_id','records','objects']),'relocated batch image count types differ')
    require(image['schema']=='qpx.batch-image/1' and image['state']=='saved' and image['objects']==1 and image['error']=='',
            'relocated batch image did not save the selected object')
    require(lines[0]==result['images'][0],'relocated batch image records disagree')
    return dict(saved=1,failed=0,records=1,source_identity=result['source_identity'])


def fixture_contract(path):
    path=Path(path).resolve()
    require(path.stat().st_size<=1024*1024,'embedded project fixture exceeds budget')
    with sqlite3.connect(path.as_uri()+'?mode=ro',uri=True) as database:
        projects=database.execute('SELECT identity FROM project').fetchall()
        require(len(projects)==1 and re.fullmatch('[0-9a-f]{32}',projects[0][0]) is not None,'project fixture identity differs')
        row=database.execute("SELECT width,height,rgb,cal_x,cal_y,source,embedded FROM images WHERE id='1'").fetchone()
        require(row==(2,2,bytes([30,60,90])*4,0.5,0.25,'',1),'embedded project fixture pixels/calibration differ')
        geometry=database.execute("SELECT kind,geometry FROM objects WHERE image_id='1' AND id='1'").fetchone()
        require(geometry is not None and geometry[0]==0 and geometry[1][:5]==b'QRO\x01\0'
                and len(geometry[1])==37 and struct.unpack('<dddd',geometry[1][5:])==(0,0,2,2),'embedded fixture selected ROI differs')
    return dict(identity=projects[0][0],sha256=sha(path))


def alias_names(plan):
    return {Path(alias).name for item in plan['edits'] if item['target'].startswith('lib/') for alias in item['source_aliases']} | {
        Path(item['target']).name for item in plan['edits'] if item['target'].startswith('lib/')}


def loaded_from_diagnostics(text,names):
    result=set()
    for line in text.splitlines():
        if not re.match(r'^dyld(?:\[[0-9]+\])?:',line):continue
        at=line.find('/')
        if at<0:continue
        path=line[at:].strip()
        if any(path.endswith('/'+name) for name in names):result.add(posixpath.normpath(path))
    return sorted(result)


def geometry_child(library,output):
    require(platform.system()=='Darwin','GEOS image probe requires macOS')
    library=Path(library).resolve()
    geos=ctypes.CDLL(str(library))
    pointer=ctypes.c_void_p
    geos.GEOS_init_r.argtypes=[];geos.GEOS_init_r.restype=pointer
    geos.GEOS_finish_r.argtypes=[pointer];geos.GEOS_finish_r.restype=None
    geos.GEOSGeomFromWKT_r.argtypes=[pointer,ctypes.c_char_p];geos.GEOSGeomFromWKT_r.restype=pointer
    geos.GEOSUnion_r.argtypes=[pointer,pointer,pointer];geos.GEOSUnion_r.restype=pointer
    geos.GEOSArea_r.argtypes=[pointer,pointer,ctypes.POINTER(ctypes.c_double)];geos.GEOSArea_r.restype=ctypes.c_int
    geos.GEOSGeom_destroy_r.argtypes=[pointer,pointer];geos.GEOSGeom_destroy_r.restype=None
    geos.GEOSversion.argtypes=[];geos.GEOSversion.restype=ctypes.c_char_p
    context=geos.GEOS_init_r();require(bool(context),'GEOS context allocation failed')
    objects=[]
    try:
        for text in [b'POLYGON((0 0,10 0,10 10,0 10,0 0))',b'POLYGON((5 0,15 0,15 10,5 10,5 0))']:
            geometry=geos.GEOSGeomFromWKT_r(context,text);require(bool(geometry),'GEOS WKT fixture failed');objects.append(geometry)
        united=geos.GEOSUnion_r(context,objects[0],objects[1]);require(bool(united),'GEOS union failed');objects.append(united)
        area=ctypes.c_double()
        require(geos.GEOSArea_r(context,united,ctypes.byref(area))==1 and area.value==150,'GEOS union area differs')
        # Single-threaded child: no other code loads/unloads libraries during this enumeration.
        process=ctypes.CDLL(None)
        process._dyld_image_count.argtypes=[];process._dyld_image_count.restype=ctypes.c_uint32
        process._dyld_get_image_name.argtypes=[ctypes.c_uint32];process._dyld_get_image_name.restype=ctypes.c_char_p
        mapped=[]
        for i in range(process._dyld_image_count()):
            raw=process._dyld_get_image_name(i)
            if raw:
                path=Path(raw.decode('utf-8')).resolve()
                if path.name.startswith('libgeos'):mapped.append(dict(path=str(path),sha256=sha(path)))
        result=dict(schema='qpx.macos-geos-relocated-child/1',library=str(library),geos_version=geos.GEOSversion().decode('ascii'),
                    union_area=area.value,mapped=sorted(mapped,key=lambda item:item['path']),release_ready=False)
        common.publish_new(Path(output),common.canonical_json(result))
        print(json.dumps(result))
    finally:
        for geometry in reversed(objects):geos.GEOSGeom_destroy_r(context,geometry)
        geos.GEOS_finish_r(context)


def probe(archive,plan,project_fixture,work,output):
    require(platform.system()=='Darwin' and platform.machine()==plan['architecture'],'runtime probe requires requested native Mac host')
    archive,project_fixture,work,output=map(Path,[archive,project_fixture,work,output])
    require(not work.exists() and not output.exists(),'runtime probe work/output must be new')
    manifest=relocation.inspect_archive(archive,plan)
    work.mkdir(parents=True,exist_ok=False);package=work/'fresh relocated package with spaces';package.mkdir()
    records={row['path']:row for row in manifest['files']}
    with archive.open('rb') as source_stream:
        before=common.version(os.fstat(source_stream.fileno()))
        require(common.file_hash(source_stream)==manifest['archive_sha256'],'runtime archive changed before extraction')
        source_stream.seek(0)
        with zipfile.ZipFile(source_stream) as source:
            for key,item in records.items():
                info=source.getinfo(key)
                require(info.file_size==item['bytes'] and info.file_size<=256*1024*1024,'runtime extraction size differs')
                with source.open(info) as entry:data=entry.read(256*1024*1024+1)
                require(len(data)==info.file_size and hashlib.sha256(data).hexdigest()==item['sha256'],'runtime extraction differs from archive')
                path=package/key;path.parent.mkdir(parents=True,exist_ok=True)
                require(path.resolve().is_relative_to(package.resolve()),'runtime extraction escapes package')
                with path.open('xb') as stream:stream.write(data)
                path.chmod(item['mode'])
        require(common.file_hash(source_stream)==manifest['archive_sha256'] and common.version(os.fstat(source_stream.fileno()))==before,
                'runtime archive changed during extraction')
    file_records=[]
    def run(label,arguments,diagnostics=True):
        environment=relocation.clean_environment()
        if diagnostics:environment['DYLD_PRINT_LIBRARIES']='1'
        done=subprocess.run(list(map(str,arguments)),cwd=work,env=environment,capture_output=True,timeout=90)
        require(len(done.stdout)+len(done.stderr)<=8*1024*1024,'runtime command logs exceed budget')
        (work/(label+'.stdout.log')).write_bytes(done.stdout);(work/(label+'.stderr.log')).write_bytes(done.stderr)
        file_records.append(dict(label=label,arguments=list(map(str,arguments)),exit_code=done.returncode,
            stdout_sha256=hashlib.sha256(done.stdout).hexdigest(),stderr_sha256=hashlib.sha256(done.stderr).hexdigest(),loader_diagnostics=diagnostics))
        common.publish_new(work/(label+'.json'),common.canonical_json(file_records[-1]))
        require(done.returncode==0,'relocated runtime command failed: '+label)
        if diagnostics:
            expected=[str((package/key).resolve()) for key in closure(plan,'bin/'+Path(arguments[0]).name)]
            loaded=loaded_from_diagnostics(done.stderr.decode('utf-8'),alias_names(plan))
            require(loaded==sorted(expected),'relocated native CLI mapped SDK libraries outside package or omitted mapping evidence: '+label)
        return done
    source=work/'input.ppm';source.write_bytes(b'P6\n2 1\n255\n'+bytes([40,80,160,80,120,200]))
    pam=work/'output.pam'
    run('region',[package/'bin/qupathxx_region','--thumbnail',source,'1','1',pam,'--source-pixels','2'])
    require(pam.read_bytes().split(b'ENDHDR\n',1)[1]==bytes([60,100,180,255]),'relocated native region pixels differ')
    fixture=fixture_contract(project_fixture);original_fixture_sha=fixture['sha256']
    project=work/'input.qpxproj'
    with project_fixture.open('rb') as stream:data=stream.read(1024*1024+1)
    require(len(data)<=1024*1024 and hashlib.sha256(data).hexdigest()==original_fixture_sha,'runtime project fixture changed before copy')
    project.write_bytes(data)
    batch=run('batch',[package/'bin/qupathxx_batch',project,work/'batch','--kind','od','--image','1','--object','1'])
    batch_summary=batch_contract(batch.stdout.decode('utf-8'))
    require(batch_summary['source_identity']==fixture['identity'],'relocated batch source identity differs from fixture')
    checkpoints=list((work/'batch').rglob('*.qpxproj'));require(len(checkpoints)==1,'relocated batch checkpoint cohort differs')
    with sqlite3.connect(checkpoints[0].resolve().as_uri()+'?mode=ro',uri=True) as database:
        require(database.execute('SELECT count(*) FROM analysis_records').fetchone()[0]==1,'relocated batch record was not retained')
    csv_path=work/'measurements.csv'
    run('export',[package/'bin/qupathxx_batch','--export-csv',checkpoints[0],csv_path])
    csv_summary=csv_contract(csv_path.read_text(encoding='utf-8'))
    require(sha(project_fixture)==sha(project)==original_fixture_sha,'relocated native batch changed original fixture')
    capis=[item['target'] for item in plan['edits'] if Path(item['target']).name.startswith('libgeos_c.')]
    require(len(capis)==1,'relocated GEOS C API provider cohort differs')
    geometry_path=work/'geometry.json'
    run('geometry',[sys.executable,'-I',Path(__file__).resolve(),'--geometry-child',package/capis[0],'--geometry-output',geometry_path],False)
    geometry=json.loads(geometry_path.read_text(encoding='utf-8'))
    expected_paths=sorted(str((package/key).resolve()) for key in closure(plan,capis[0]) if Path(key).name.startswith('libgeos'))
    require([item['path'] for item in geometry['mapped']]==expected_paths and geometry['union_area']==150,'relocated GEOS mappings/area differ')
    for item in geometry['mapped']:
        relative=Path(item['path']).relative_to(package.resolve()).as_posix()
        require(item['sha256']==records[relative]['sha256']==sha(package/relative),'mapped GEOS bytes differ from relocated archive')
    for key,item in records.items():require(sha(package/key)==item['sha256'],'runtime changed extracted package bytes')
    report=dict(schema='qpx.macos-relocated-runtime/1',architecture=plan['architecture'],relocated_archive_sha256=manifest['archive_sha256'],
        plan_sha256=manifest['plan_sha256'],fixture_sha256=original_fixture_sha,fixture_preserved=True,
        fresh_package=str(package.resolve()),path_with_spaces=True,loader_search_overrides_used=False,
        loader_diagnostics_used=True,commands=file_records,region_rgba=[60,100,180,255],
        batch=batch_summary,csv=csv_summary,geometry=geometry,geometry_report_sha256=sha(geometry_path),
        checkpoint_sha256=sha(checkpoints[0]),csv_sha256=sha(csv_path),pam_sha256=sha(pam),
        all_package_bytes_preserved=True,release_ready=False,
        scope='Fresh archive extraction with spaces: exact native region pixels, migrated embedded-project OD/checkpoint/CSV, '
              'GEOS union and actual native/GEOS package image mappings with clean loader search environment. '
              'Host engineering proof only; no older-OS, arbitrary ABI, biological, Python/GUI/OpenSlide or supported-release acceptance.')
    common.publish_new(output,common.canonical_json(report))
    return report


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--geometry-child',type=Path)
    parser.add_argument('--geometry-output',type=Path)
    parser.add_argument('--archive',type=Path)
    parser.add_argument('--plan',type=Path)
    parser.add_argument('--project-fixture',type=Path)
    parser.add_argument('--work',type=Path)
    parser.add_argument('--output',type=Path)
    args=parser.parse_args()
    if args.geometry_child:
        require(args.geometry_output is not None,'geometry child output required');geometry_child(args.geometry_child,args.geometry_output)
    else:
        require(all([args.archive,args.plan,args.project_fixture,args.work,args.output]),'runtime probe arguments required')
        require(args.plan.stat().st_size<=64*1024*1024,'runtime plan exceeds budget')
        plan=json.loads(args.plan.read_text(encoding='utf-8'),object_pairs_hook=common.unique_object)
        # Recompute the trusted plan from its source snapshot in the producer;
        # this CLI consumes that exact source-bound record.
        print(json.dumps(probe(args.archive,plan,args.project_fixture,args.work,args.output)))


if __name__=='__main__':main()
