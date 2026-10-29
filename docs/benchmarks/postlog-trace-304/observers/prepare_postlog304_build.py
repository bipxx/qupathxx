from pathlib import Path
root=Path(__file__).resolve().parents[2];target=root/'build/ci/build_postlog304.py';assert not target.exists()
text=(root/'build/ci/build_cell_policy302.py').read_text(encoding='utf-8')
replacements=[('cell-processing-policy-302','postlog-trace-304'),('cell_policy302_backend.py','postlog304_backend.py'),("source.startswith('2b4786ed')","source.startswith('e9d71f28')"),('len(files) == 429','len(files) == 433'),('build/reader-locale-297/final-products.json','build/cell-processing-policy-302/final-products.json'),('len(prior) == 71','len(prior) == 76'),("out/'build.log'","out/'app-build.log'"),("out/'build-exit.json'","out/'app-build-exit.json'"),('Cell policy302 build exit','Post-LoG304 build exit')]
for old,new in replacements:
    assert old in text,old;text=text.replace(old,new)
target.write_text(text,encoding='utf-8')
