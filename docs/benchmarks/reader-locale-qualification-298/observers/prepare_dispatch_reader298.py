from pathlib import Path
root=Path(__file__).resolve().parents[2]
text=(root/'build/ci/dispatch_nominal295.py').read_text()
text=text.replace('nominal-cell-qualification-295','reader-locale-qualification-298').replace('a7eb78de75387bce2da7b3d59b7f0a2ab6126cee','9fbc9315e8c2417c35492cf505a8f676ecdfcf75')
text=text.replace('==894','==898').replace('source_paths=894','source_paths=898').replace('==424','==425').replace('canonical_compiler_blobs_equal=424','canonical_compiler_blobs_equal=425')
text=text.replace('==1086','==1087')
assert '894' not in text and '424' not in text
target=root/'build/ci/dispatch_reader298.py';assert not target.exists();target.write_text(text,encoding='utf-8')
