from pathlib import Path
import os,re,sys
sys.path.insert(0,'tests')
import qupathxx as q
from test_gui_docking import config
from test_gui_i18n import chooser_click
from PIL import Image
out=Path('build/ui-language')
os.environ['QUPATHXX_UI_LANGUAGE']='en'
project=q.Project.open(str(Path('build/ui-design/preview.qpxproj').absolute()))
o=config(out/'capture-layout.ini')
o.inputs=chooser_click(o,5,320,10)+chooser_click(o,10,350,39)
q.show(project,frame_limit=24,capture_path=str(out/'language-chooser.ppm'),options=o)
Image.open(out/'language-chooser.ppm').save('docs/images/ui-language-chooser.png')
s=(out/'capture-layout.ini').read_text()
m=re.search(r'\[Window\]\[Choose language\]\s+Pos=(\d+),(\d+)\s+Size=(\d+),(\d+)',s)
assert m
x,y,w,h=map(int,m.groups())
o.inputs=chooser_click(o,5,320,10)+chooser_click(o,10,350,39)+chooser_click(o,16,x+80,y+208)+chooser_click(o,22,x+35,y+h-20)
r=q.show(project,frame_limit=32,capture_path=str(out/'chinese-workspace.ppm'),options=o)
assert '; qupathxx language=zh-CN\n' in (out/'capture-layout.ini').read_text()
Image.open(out/'chinese-workspace.ppm').save('docs/images/ui-language-chinese.png')
print('Language chooser and Chinese UI captures saved; overflow:',r.ui_overflow_items)
