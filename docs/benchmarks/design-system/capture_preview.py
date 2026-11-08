from pathlib import Path
import os,json,uuid
import qupathxx as q
from PIL import Image
root=Path.cwd();out=Path('C:/Users/HEQ/Projects/qupathxx/build/design-system-preview')
slide='C:/Users/HEQ/Projects/qupathxx/build/fixtures/CMU-1.svs'
doc=q.Document(q.open_slide(slide));doc.add_rectangle(q.Rect(29185,2048,512,512),'Research ROI')
project=q.Project();image=project.add_document(doc,'CMU-1 / Research slide',slide)
view=q.ProjectView();view.center=q.Point(29441,2304);view.zoom=1.1;project.set_view(image,view)
preview=out/f'research-preview-{uuid.uuid4().hex[:8]}.qpxproj'
project.save(str(preview))
(out/'preview-project-path.txt').write_text(str(preview),encoding='utf8')
for theme,language in [('dark','en'),('light','en'),('dark','zh-CN')]:
 os.environ['QUPATHXX_UI_THEME']=theme;os.environ['QUPATHXX_UI_LANGUAGE']=language
 options=q.GuiOptions();options.docked_workspace=True;options.settle_tiles=True;options.autosave_interval_seconds=0;options.overview_source_pixel_limit=16*1024*1024
 file=out/f'{theme}-{language}.ppm'
 r=q.show(project,frame_limit=30,capture_path=str(file),options=options)
 Image.open(file).save(root/'docs/images'/f'design-system-{theme}-{language}.png')
 assert r.canvas_visible and r.tiles_settled and r.ui_overflow_items==0
 print(theme,language,'tiles',r.uploaded_tiles,'fonts',r.font_rebuilds)
