import sys,os,importlib.util,runpy
from pathlib import Path
binary=Path(sys.argv[1]).resolve();runtime=Path(sys.argv[2]).resolve();bench=Path(sys.argv[3]).resolve();args=sys.argv[4:]
dll=os.add_dll_directory(str(runtime))
spec=importlib.util.spec_from_file_location('_native',binary);native=importlib.util.module_from_spec(spec);sys.modules[spec.name]=native;spec.loader.exec_module(native)
sys.modules['qupathxx']=native
sys.argv=[str(bench),*args]
runpy.run_path(str(bench),run_name='__main__')
