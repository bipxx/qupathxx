from pathlib import Path
root=Path(__file__).resolve().parents[2];ci=root/'build/ci'
text=(ci/'dispatch_reader298.py').read_text(encoding='utf-8')
text=text.replace('reader-locale-qualification-298','original-source-cell-qualification-306').replace('9fbc9315e8c2417c35492cf505a8f676ecdfcf75','701fa672db3745aebc7fc5d71108b2c8780cbb6e')
text=text.replace('==898','==1428').replace('source_paths=898','source_paths=1428').replace('==425','==434').replace('canonical_compiler_blobs_equal=425','canonical_compiler_blobs_equal=434').replace('==1087','==1106')
assert '898' not in text and '425' not in text and '1087' not in text and '9fbc9315' not in text
target=ci/'dispatch_original306.py'
with target.open('x',encoding='utf-8') as stream:stream.write(text)
print('Prepared complete-role dispatch helper; no dispatch performed')
