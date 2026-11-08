from pathlib import Path
import hashlib,json,runpy,subprocess
import numpy as np
import qupathxx as q
r=Path(__file__).resolve().parents[2];s=r/'build/ci/scientific-grid-159';out=r/'build/rawbatch231-backend-off';cache=r/'build/ci/gridfrontends167-package';exe=cache/'Release/qupathxx_batch.exe'
flags={line.split(':')[0]:line.split('=',1)[1] for line in (cache/'CMakeCache.txt').read_text(encoding='utf-8').splitlines() if line.startswith('QUPATHXX_BUILD_') and ':BOOL=' in line};assert flags['QUPATHXX_BUILD_CLASSIFIER_TRAINING']=='OFF'
ctest=r/'.venv/Lib/site-packages/cmake/data/bin/ctest.exe';cmd=[str(ctest),'--test-dir',str(cache),'-C','Release','-R','^project_batch$','--output-on-failure','--test-output-size-passed','1048576','--no-tests=error','--output-junit',str(out/'native-tests.xml')]
with (out/'native-tests.log').open('wb') as log:done=subprocess.run(cmd,stdout=log,stderr=subprocess.STDOUT,timeout=180)
assert done.returncode==0,(out/'native-tests.log').read_text(encoding='utf-8',errors='replace')
fixture=out/'fixture';fixture.mkdir(exist_ok=False);project=q.Project();ids=[]
tiff=runpy.run_path(str(s/'tests/test_raw_pixel_python.py'))['tiff']
for i,w in enumerate((8,10)):
 path=fixture/('source-'+str(i)+'.tif');tiff(path,w,6);image=project.add_image(str(path),'tiff');doc=project.document(image)
 labels=[doc.add_detection(q.Roi.rectangle(q.Rect(0,0,3,6)),class_name='A'),doc.add_detection(q.Roi.rectangle(q.Rect(4,0,w-4,6)),class_name='B')]
 target=doc.add_roi(q.Roi.rectangle(q.Rect(0,0,w,6)));ids.append((image,target,labels))
first=project.document(ids[0][0]);lattice=q.ScientificRawLattice(8,6,q.ScientificGridAxis(0,8,8),q.ScientificGridAxis(0,6,6));selection=q.ScientificRawOptions();selection.channels=2;features=q.RawPixelFeatureOptions();features.offsets=[0,0];features.scales=[1,1]
table=q.sample_raw_pixel_tiled_training(q.RawPixelTiledTrainingRequest(first,ids[0][2],lattice,selection,features));parameters=q.ForestParameters();parameters.trees=5;parameters.max_depth=3;parameters.work_bytes=2*1024**2
model=q.train_raw_pixel_tiled_classifier(table,parameters);model_path=fixture/'model.qrm';q.save_raw_pixel_classifier(model,str(model_path));source=fixture/'input.qpx';project.save(str(source));before=source.read_bytes()
cmd=[str(exe),str(source),str(fixture/'out'),'--kind','raw-pixels','--raw-pixel-model',str(model_path),'--raw-pixel-tile-width','2','--raw-pixel-tile-height','2','--raw-pixel-work-bytes',str(16*1024**2)]
results=[]
for name in ('fresh','reuse'):
 done=subprocess.run(cmd,capture_output=True,timeout=60);(out/(name+'.stdout')).write_bytes(done.stdout);(out/(name+'.stderr')).write_bytes(done.stderr);assert done.returncode==0,done.stdout+done.stderr;report=json.loads(done.stdout.splitlines()[-1]);assert report['saved' if name=='fresh' else 'reused']==2;results.append(report)
stream_options=q.RawPixelStreamOptions();stream_options.tile_width=stream_options.tile_height=2;stream_options.work_bytes=16*1024**2;blocks=0
for value,(image,root,_) in zip(results[0]['images'],ids):
 restored=q.Project.open(value['path']);record,=restored.document(image).raw_pixel_stream_records;file=q.RawPixelStreamFile(record.link.path);file.verify()
 doc=project.document(image);m=doc.metadata;geometry=q.ScientificRawLattice(m.width,m.height,q.ScientificGridAxis(0,m.width,m.width),q.ScientificGridAxis(0,m.height,m.height));expected=q.RawPixelPredictionStream(q.RawPixelTiledPredictionRequest(doc,model,geometry,stream_options,object_id=root))
 for i in range(file.declared_summary.completed_windows):
  actual,reference=file.read_block(i),expected.next()
  for method in ('labels_numpy','vote_fractions_numpy','valid_numpy'):np.testing.assert_array_equal(getattr(actual,method)(),getattr(reference,method)())
  blocks+=1
 assert expected.next() is None;restored.document(image).server.close()
assert source.read_bytes()==before
for image,_,_ in ids:project.document(image).server.close()
proof=dict(source=q.build_environment().git_revision,fingerprint=q.build_environment().source_tree_sha256,flags=flags,command=cmd,saved=results[0]['saved'],reused=results[1]['reused'],verified_numeric_blocks=blocks,model_sha256=model.sha256,source_project_unchanged=True,executable_sha256=hashlib.sha256(exe.read_bytes()).hexdigest())
(out/'portable-backend-off-proof.json').write_text(json.dumps(proof,indent=2)+'\n',encoding='utf-8',newline='\n');print('Backend-OFF native batch suite and actual-backend-model CLI save/reuse passed;',blocks,'numeric blocks match the installed ON consumer')
