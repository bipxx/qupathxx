from pathlib import Path
import hashlib
import importlib.util
import json
import sys
import numpy as np
import PIL
import qupathxx as q
root=Path('/mnt/c/Users/HEQ/Projects/qupathxx');out=root/'build/gaussian-characterization-299/linux';work=root/'build/ci/scientific-grid-159'
prior=json.loads((root/'build/nominal-cell-comparison-296/linux/installed-proof.json').read_text());site=Path(q.__file__).parent.parent
for entry in prior['installed']:
    path=site/entry['path'];assert path.stat().st_size==entry['bytes'] and hashlib.sha256(path.read_bytes()).hexdigest()==entry['sha256']
assert q.build_environment().git_revision==prior['native_source']
spec=importlib.util.spec_from_file_location('gaussian_linux_reference',work/'tools/qupath_gaussian.py');module=importlib.util.module_from_spec(spec);spec.loader.exec_module(module)
report=module.verify()
(out/'verification.json').write_text(json.dumps(report,indent=2)+'\n')
(out/'runtime-proof.json').write_text(json.dumps(dict(executable=sys.executable,python=sys.version,numpy=np.__version__,pillow=PIL.__version__,unchanged_package_members=len(prior['installed']),native_source=prior['native_source'],native_used_for_gaussian=False,shared_test_dependencies=True,clean_machine=False),indent=2)+'\n')
print('Linux reference verified against unchanged authenticated installed environment; no native calls')
