from pathlib import Path
import hashlib
import importlib.util
import json
import shutil
import subprocess

root=Path(__file__).resolve().parents[2];work=root/'build/ci/scientific-grid-159';out=root/'build/cell-policy-edges-303'
first=out/'capture1';second=out/'capture2';manifest=json.loads((first/'manifest.json').read_text(encoding='utf-8'))
shared=sorted(set(manifest['files'])-{'driver.log'})
assert len(shared)==146
for name in shared:assert (first/name).read_bytes()==(second/name).read_bytes(),name
(out/'repeat-proof.json').write_text(json.dumps(dict(shared_payloads=len(shared),all_shared_payloads_byte_exact=True,excluded=['driver.log','manifest.json'],first_manifest_sha256=hashlib.sha256((first/'manifest.json').read_bytes()).hexdigest(),second_manifest_sha256=hashlib.sha256((second/'manifest.json').read_bytes()).hexdigest()),indent=2)+'\n',encoding='utf-8')
target=work/'tests/fixtures/qupath-cell-policy-edges-0.7.0';assert not target.exists();shutil.copytree(first,target)
probe=out/'affine-probe.groovy'
probe.write_text('''import java.awt.geom.AffineTransform
import java.awt.geom.Point2D
import java.nio.file.Path
import java.nio.file.Files
import qupath.lib.io.GsonTools
def root=Path.of(args[0])
def contract=GsonTools.getInstance().fromJson(Files.readString(root.resolve('capture1/contract.json')),Map.class)
def cases=contract.cases.collect { item ->
    double step=(item.requested_pixel_size_um ?: .5)/.5
    def origin=item.roi ?: [0,0,256,192]
    def transform=new AffineTransform()
    transform.scale(1d/step,1d/step);transform.translate(-(origin[0] as double),-(origin[1] as double))
    def inverse=transform.createInverse()
    def matrix=new double[6];inverse.getMatrix(matrix)
    def center=inverse.transform(new Point2D.Double(.5,.5),null)
    [name:item.name,step:step,origin:origin.take(2),inverse:matrix,first_center:[center.x,center.y]]
}
Files.writeString(root.resolve('affine-reference.json'),GsonTools.getInstance(true).toJson(cases)+'\\n')
''',encoding='utf-8')
spec=importlib.util.spec_from_file_location('affine_runtime',work/'tools/fetch_qupath.py');module=importlib.util.module_from_spec(spec);spec.loader.exec_module(module)
runtime=module.fetch(root/'build/upstream/qupath')
args=[str(runtime),'script','--args',str(out),str(probe)]
with (out/'affine-driver.log').open('xb') as stream:r=subprocess.run(args,stdout=stream,stderr=subprocess.STDOUT,timeout=180,creationflags=subprocess.CREATE_NO_WINDOW)
(out/'affine-command.json').write_text(json.dumps(dict(argv=args,returncode=r.returncode),indent=2)+'\n',encoding='utf-8');assert r.returncode==0
print('All146 shared numeric/recipe/geometry payloads repeat exactly; fixture promoted unchanged; public affine probe retained',flush=True)
