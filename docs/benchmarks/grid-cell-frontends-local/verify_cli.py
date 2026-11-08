from pathlib import Path
import subprocess,json,csv,sqlite3
root=Path(__file__).resolve().parents[2]
bin=root/'build/ci/gridfrontends167-native/Release'
out=root/'build/gridfrontends167-cli-final';out.mkdir(exist_ok=True)
logs=[]
def run(args, code=0, error=None):
    p=subprocess.run([str(v) for v in args],capture_output=True,text=True)
    logs.append(dict(args=[str(v) for v in args],exit=p.returncode,stdout=p.stdout,stderr=p.stderr))
    (out/'commands.json').write_text(json.dumps(logs,indent=2)+'\n')
    assert p.returncode==code,logs[-1]
    if error: assert error in p.stderr,logs[-1]
    return p.stdout
fixture=out/'input.qpxproj'
if not fixture.exists(): run([bin/'qupathxx_project_batch_tests.exe','--write-grid-fixture',fixture])
cli=bin/'qupathxx_batch.exe'
base=[cli,fixture,out/'results','--kind','cells','--cell-algorithm','grid','--cell-grid-samples','3,3','--cell-grid-context','1,1','--grid-units','um','--grid-sigma','0','--grid-prominence','.2','--grid-min-area','.75','--grid-expansion','.9','--grid-contour-bytes','1048576','--stains','1,0,0,0,1,0,0,0,1','--normalized']
first=[json.loads(v) for v in run(base).splitlines()]
assert first[-1]['state']=='succeeded' and first[-1]['saved']==1,first
second=[json.loads(v) for v in run(base).splitlines()]
assert second[-1]['reused']==1 and second[-1]['saved']==0,second
checkpoint=Path(next(v['path'] for v in second if v.get('state')=='reused'))
run([cli,'--export-cell-table',checkpoint,out/'cells.csv','--capture-calibration'])
rows=list(csv.DictReader((out/'cells.csv').open(newline='')));assert len(rows)==1,rows
r=rows[0]
for k,v in {'table_schema':'qpx.project-cell-table/6','sampling_grid':'explicit','sample_step_x_px':'2','sample_step_y_px':'3','grid_parameter_units':'um','grid_resolved_step_x':'1','grid_resolved_step_y':'0.75','nucleus_samples':'1','cell_samples':'3','downsample':'','sigma_sample_px':''}.items(): assert r[k]==v,(k,r[k],v)
with sqlite3.connect(checkpoint) as db:
    columns=[v[1] for v in db.execute('pragma table_info(cell_records)')]
    row=db.execute('select * from cell_records').fetchone()
    assert any(isinstance(v,bytes) and v.startswith(bytes.fromhex('51434c05')) for v in row),columns
for flags,error in [
    (['--cell-grid-samples','0,3'],'grid cell sampling requires'),
    (['--grid-units','yards'],'grid units'),
    (['--grid-sigma','nan'],'expected finite number'),
    (['--level','-1'],'without legacy algorithm'),
    (['--tile-size','256'],'without legacy algorithm'),
    (['--cell-algorithm','distance'],'without legacy algorithm'),
    (['--cell-context-padding','32'],'without legacy algorithm'),
    (['--cell-min-area-um2','1'],'without legacy algorithm')]:
    run([cli,out/'missing.qpxproj',out/'invalid','--kind','cells','--cell-algorithm','grid',*flags],2,error)
print('Grid CLI saved/resumed checkpoint, QCL5, per-axis typed CSV and eight pre-IO argument rejections passed')
