import sys,json
from pathlib import Path
import qupathxx as q
r=Path(__file__).resolve().parents[2]
sys.path.insert(0,str(r/'build/ci/scientific-grid-159/tests'))
from test_tma import scene
from test_gui_tma import options,click
doc,ids=scene();o=options();o.tma_tab=1;o.tma_insert_label='Review row';o.tma_insert_missing=True
o.inputs=click(4,q.GuiInputTarget.TMA_INSERT_ROW_AFTER)
report=q.show(doc,frame_limit=25,capture_path=str(r/'build/tma193-evidence/axis-controls.ppm'),options=o)
assert not report.tma_error and report.ui_overflow_items==0
assert doc.tma_grid().info.rows==3
print(json.dumps(dict(rows=doc.tma_grid().info.rows,columns=doc.tma_grid().info.columns,cores=doc.tma_core_count,missing=report.tma_missing_cores,overflow=report.ui_overflow_items)))
