from pathlib import Path
from fractions import Fraction
import csv,io,json,subprocess,sys
import numpy as np
import tifffile
import qupathxx as q
r=Path(__file__).resolve().parents[2];out=r/'build/raw180-cli-check';out.mkdir(exist_ok=False)
data=np.arange(32,dtype='uint64').reshape(2,2,2,2,2)+np.uint64(2**53+5)
tiff=out/'source.ome.tif';tifffile.imwrite(tiff,data,ome=True,photometric='minisblack',metadata={'axes':'TZCYX'})
project=q.Project();image=project.add_tiff_image(str(tiff));doc=project.document(image);doc.add_rectangle(q.Rect(0,0,2,1),'whole')
path=out/'项目.qpxproj';project.save(str(path));cli=r/'build/ci/gridfrontends167-package/Release/qupathxx_batch.exe'
cmd=[str(cli),str(path),str(out/'结果'),'--kind','raw','--raw-grid-samples','1,1','--raw-grid-context','1,0','--raw-channel','1','--raw-z','1','--raw-time','1']
results=[]
for run in range(2):
 p=subprocess.run(cmd,capture_output=True,timeout=60);(out/(str(run)+'.stdout')).write_bytes(p.stdout);(out/(str(run)+'.stderr')).write_bytes(p.stderr);assert p.returncode==0,p.stderr.decode('utf-8',errors='replace')
 rows=[json.loads(line) for line in p.stdout.decode('utf-8').splitlines()];result=next(v for v in rows if v.get('schema')=='qpx.batch-image/1');results.append(result)
 assert result['state']==('saved' if run==0 else 'reused')
 assert result['raw_tiles']==(1 if run==0 else 0) and result['raw_hashed_tiles']==(0 if run==0 else 1)
 loaded=q.Project.open(result['path']);record=loaded.document(image).raw_analysis_records[0]
 expected=float(sum((Fraction(int(v)) for v in data[1,1,1,0].flat),Fraction())/2)
 assert record.result.grid is not None and record.result.statistics.means[0].hex()==expected.hex()
 table=list(csv.DictReader(io.StringIO(q.batch_checkpoint_csv(result['path']))));assert table[0]['schema']=='qpx.raw-analysis-csv/2'
for args in [['--raw-level','0'],['--tile-size','256'],['--level','0']]:
 p=subprocess.run(cmd+args,capture_output=True);assert p.returncode==2 and b'level or tile' in p.stderr
p=subprocess.run([str(cli),str(path),str(out/'bad'),'--kind','raw','--raw-grid-context','1,0'],capture_output=True);assert p.returncode==2 and b'require --raw-grid-samples' in p.stderr
(out/'results.json').write_bytes((json.dumps({'command':cmd,'images':results,'expected_mean_hex':expected.hex()},indent=2)+'\n').encode('utf-8'))
print('Native raw grid CLI exact UInt64, axes, Unicode checkpoint, resume and flag conflicts passed')
