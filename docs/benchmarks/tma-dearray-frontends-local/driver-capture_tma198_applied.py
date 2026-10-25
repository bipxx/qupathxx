from pathlib import Path
import json
import qupathxx as q
r=Path(__file__).resolve().parents[2];source=r/'build/tma198-evidence-retry/dearray-desktop-tmp/test_detect_apply_undo_redo_an0/detected.qpxproj';project=q.Project.open(str(source));doc=project.document(1);assert doc.tma_core_count==12 and doc.tma_grid().core(1,1).tma_core.missing
options=q.GuiOptions();options.width=1280;options.height=900;report=q.show(doc,frame_limit=20,capture_path=str(r/'build/tma198-evidence-retry/applied-grid.ppm'),options=options);assert report.ui_overflow_items==0
(r/'build/tma198-evidence-retry/applied-capture.json').write_text(json.dumps(dict(source=str(source),cores=doc.tma_core_count,missing=1,ui_overflow_items=report.ui_overflow_items,scope='Saved/reopened ImGui-applied synthetic grid; native renderer capture, not biological acceptance'),indent=2)+'\n',encoding='utf-8')
