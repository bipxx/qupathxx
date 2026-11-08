from pathlib import Path
root=Path(__file__).resolve().parents[2];target=root/'build/ci/validate_postlog304.py';assert not target.exists()
text=(root/'build/ci/validate_cell_policy302.py').read_text(encoding='utf-8')
text=text.replace('cell-processing-policy-302','postlog-trace-304').replace('cell-policy302-env','postlog304-env')
target.write_text(text,encoding='utf-8')
