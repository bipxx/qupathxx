from pathlib import Path
root=Path(__file__).resolve().parents[2]
target=root/'build/ci/patch_policy302_stubs.py'
assert not target.exists()
text=(root/'build/ci/patch_nominal_python292_stubs.py').read_text()
text=text.replace('nominal-cell-python-292','cell-processing-policy-302').replace('unchanged_compiler_inputs=423','unchanged_compiler_inputs=429')
target.write_text(text,encoding='utf-8')
