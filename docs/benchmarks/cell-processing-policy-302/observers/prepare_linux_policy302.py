from pathlib import Path
root=Path(__file__).resolve().parents[2]
path=root/'build/ci/linux_cell_policy302.py'
assert not path.exists()
text=(root/'build/ci/linux_nominal_python292.py').read_text()
text=text.replace('nominal-cell-python-292','cell-processing-policy-302').replace('nominal292','policy302')
text=text.replace("commit=='ab706d47bca3bb7fcbe05e1416b81c928e550ed4'", "commit.startswith('2b4786ed')")
text=text.replace("'qupathxx_stains_tests']", "'qupathxx_stains_tests','qupathxx_cell_table_tests','qupathxx_imagej_gaussian_tests']")
text=text.replace('log_cell_recipe|stains)$','log_cell_recipe|stains|cell_tables|imagej_gaussian)$')
text=text.replace('len(proof)==423','len(proof)==429')
text=text.replace("print(name,result.returncode,flush=True);assert result.returncode==0,log", "print(name,result.returncode,flush=True)\n    if name!='linux-native':assert result.returncode==0,log")
text=text.replace("print('Six affected native suites passed under ASan/UBSan; seven products and full logs retained before exit',flush=True)", "assert results[-1]['returncode']==0, 'Native failure; products and logs retained'\nprint('Eight affected native suites passed under ASan/UBSan; nine products and full logs retained before exit',flush=True)")
path.write_text(text,encoding='utf-8')
(root/'build/cell-processing-policy-302/linux').mkdir(exist_ok=False)
