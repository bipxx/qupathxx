from pathlib import Path
import hashlib
import importlib.util
import json
from PIL import Image
import qupathxx as q

root=Path(__file__).resolve().parents[2];work=root/'build/ci/scientific-grid-159';out=root/'build/nominal-cell-python-292'
assert q.build_environment().git_revision=='ab706d47bca3bb7fcbe05e1416b81c928e550ed4'
path=work/'tests/test_nominal_cell_python.py';spec=importlib.util.spec_from_file_location('nominal_scene',path)
scene=importlib.util.module_from_spec(spec);spec.loader.exec_module(scene)
doc,obj,transform,parameters=scene.scene();request=scene.request(doc,obj,transform,parameters)
job=q.CellJob();assert job.request(request) and job.wait(doc,obj).state=='ready'
run=job.keep_record(doc,obj);ids=doc.publish_cell_record(run);assert len(ids)==3 and doc.object_count==4
for id in ids:assert doc.cell_object_status(id)=='current'
options=q.GuiOptions();options.width=1100;options.height=800;options.docked_workspace=False
options.settle_tiles=True;options.autosave_interval_seconds=0
capture=out/'nominal-original-document.ppm';assert not capture.exists()
report=q.show(doc,frame_limit=24,capture_path=str(capture),options=options)
assert report.canvas_visible and report.tiles_settled and report.uploaded_tiles>0
png=out/'nominal-original-document.png'
with Image.open(capture) as original:
    original.save(png)
    with Image.open(png) as converted:assert original.size==converted.size and original.tobytes()==converted.tobytes()
proof=dict(native_source=q.build_environment().git_revision,original_image_shape=[256,192],published_cells=len(ids),
    record_id=run,nominal_downsample=job.result.nominal_sampling.source_request.downsample,
    canvas_visible=report.canvas_visible,tiles_settled=report.tiles_settled,uploaded_tiles=report.uploaded_tiles,
    ppm_sha256=hashlib.sha256(capture.read_bytes()).hexdigest(),png_sha256=hashlib.sha256(png.read_bytes()).hexdigest(),
    png_lossless=True,gui_creation_controls_delivered=False,managed_viewer_command_delivered=False)
(out/'nominal-desktop-proof.json').write_text(json.dumps(proof,indent=2)+'\n',encoding='utf-8')
print('Original document and three published nominal cells rendered through the existing ImGui viewer',flush=True)
