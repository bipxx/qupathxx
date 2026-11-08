from pathlib import Path
import hashlib,json,subprocess,time
r=Path(__file__).resolve().parents[2];w=r/'build/ci/scientific-grid-159';o=r/'build/source-verification-r5';cache=r/'build/ci/gridfrontends167-package'
digest=lambda p:hashlib.sha256(p.read_bytes()).hexdigest()
inputs=json.loads((o/'compiler-inputs-final.json').read_bytes())
changed=[p for p,h in inputs['files'].items() if digest(w/p)!=h]
assert changed==['cpp/tests/project_tests.cpp']
paths=['Release/qupathxx_core.lib','qupathxx_core.dir/Release/provenance.obj','Release/_native.cp312-win_amd64.pyd']
before={p:digest(cache/p) for p in paths}
source=subprocess.check_output(['git','rev-parse','HEAD'],cwd=w,text=True).strip()
assert not subprocess.check_output(['git','status','--porcelain'],cwd=w)
argv=[str(r/'.venv/Lib/site-packages/cmake/data/bin/cmake.exe'),'--build',str(cache),'--config','Release','--target','qupathxx_project_tests','--','/p:BuildProjectReferences=false']
started=time.perf_counter()
with (o/'native-observer-build.log').open('x') as f:result=subprocess.run(argv,cwd=w,stdout=f,stderr=subprocess.STDOUT)
after={p:digest(cache/p) for p in paths}
summary=dict(observer_source=source,product_source=inputs['source'],test_source_sha256=digest(w/'cpp/tests/project_tests.cpp'),argv=argv,returncode=result.returncode,elapsed_seconds=time.perf_counter()-started,retained_products=before,retained_products_unchanged=before==after)
(o/'native-observer-build.json').write_text(json.dumps(summary,indent=2))
assert before==after,'Observer build changed the retained core/product bytes'
assert result.returncode==0
print('Corrected native test built against byte-identical retained core and package.',flush=True)
