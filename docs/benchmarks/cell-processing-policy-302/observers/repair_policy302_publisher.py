from pathlib import Path
import json
import shutil
import subprocess
root=Path(__file__).resolve().parents[2];work=root/'build/ci/scientific-grid-159';out=root/'build/cell-processing-policy-302'
retained=out/'publisher-encoding-failure';retained.mkdir(exist_ok=False)
head=subprocess.check_output(['git','rev-parse','HEAD'],cwd=work,text=True).strip();assert head.startswith('28e062dd')
for name in ('current-task.md','feature-matrix.md','project-plan.md'):
    path=work/'docs'/name;shutil.copyfile(path,retained/name)
    raw=subprocess.check_output(['git','show',head+':docs/'+name],cwd=work)
    raw.decode('utf-8');path.write_bytes(raw)
script=root/'build/ci/publish_cell_policy302.py';shutil.copyfile(script,retained/script.name)
text=script.read_text(encoding='utf-8').replace('.read_text()', ".read_text(encoding='utf-8')")
script.write_text(text,encoding='utf-8')
(retained/'diagnosis.json').write_text(json.dumps(dict(first_failure='UnicodeDecodeError under default GBK while reading API delivery plan; prior three docs had been written',resume_failure='Input301 already replaced in first three docs',restoration_source=head,restored_docs=['current-task.md','feature-matrix.md','project-plan.md'],notes_prefix_and_single_append_verified=True,native_products_or_test_results_changed=False,repair='Restore docs from verified UTF-8 git blobs, then use explicit UTF-8 reads'),indent=2)+'\n',encoding='utf-8')
