from pathlib import Path
import json,os,subprocess,sys
r=Path(__file__).resolve().parents[2];w=r/'build/ci/scientific-grid-159';o=r/'build/source-verification-r5';python=r/'build/ci/source-verification-r5-ui-env/Scripts/python.exe'
mode=sys.argv[1];env=os.environ.copy();env['QUPATHXX_TEST_GUI']='0'
if mode=='model':
    argv=[str(python),'-I','-X','utf8','-m','pytest','tests/test_project_source.py','tests/test_project_relink.py','tests/test_project.py','tests/test_project_views.py','tests/test_project_save.py','tests/test_source_identity.py','tests/test_file_fingerprint.py','tests/test_tiff_project.py','tests/test_pixel_stream_records.py','tests/test_analysis_records.py','tests/test_cell_feature_links.py','tests/test_cell_records.py','tests/test_object_classifier_archives.py','tests/test_pixel_archives.py','tests/test_provenance.py','tests/test_raw_records.py','tests/test_recovery_association.py','tests/test_tissue_records.py','tests/test_tma.py','tests/test_engine_successor_audit.py','tests/test_scientific_grid_evidence.py','tests/test_public_stubs.py','tests/test_stub_signature_guard.py','-q','--junitxml='+str(o/'model.xml'),'--basetemp',str(o/'model-temp')]
elif mode=='native':
    argv=[str(r/'.venv/Lib/site-packages/cmake/data/bin/ctest.exe'),'--test-dir',str(r/'build/ci/gridfrontends167-package'),'-C','Release','-R','^(qupath_project|project|interchange|spatial_records|file_fingerprint)$','--output-on-failure','--test-output-size-passed','10000000','--test-output-size-failed','10000000','--output-junit',str(o/'native.xml')]
elif mode=='desktop':
    env['QUPATHXX_TEST_GUI']='1'
    argv=[str(python),'-I','-X','utf8','-m','pytest','tests/test_project_source.py::test_desktop_hash_review_apply_and_save','-q','--junitxml='+str(o/'desktop-ui.xml'),'--basetemp',str(o/'desktop-ui-temp')]
elif mode=='stubs':argv=[str(python),'-I','-X','utf8',str(w/'tools/generate_stubs.py'),'--check']
elif mode in ('typing-valid','typing-invalid'):
    kind='invalid' if mode.endswith('invalid') else 'valid'
    argv=[str(python),'-I','-m','mypy','--strict','--no-incremental','tests/typing/project_source_'+kind+'.py']
else:raise ValueError(mode)
with (o/(mode+'-ui.log')).open('x',encoding='utf-8') as f:result=subprocess.run(argv,cwd=w,env=env,stdout=f,stderr=subprocess.STDOUT)
(o/(mode+'-ui-command.json')).write_bytes((json.dumps(dict(argv=argv,returncode=result.returncode),indent=2)+'\n').encode())
print(mode,result.returncode,flush=True)
if mode=='typing-invalid':assert result.returncode==1
else:raise SystemExit(result.returncode)
