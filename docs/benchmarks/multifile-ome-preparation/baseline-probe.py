from pathlib import Path
import json
import qupathxx as q
r=Path(r"C:/Users/HEQ/Projects/qupathxx");root=r/'build/ome205-fixture-current';manifest=json.loads((root/'manifest.json').read_text(encoding='utf-8'));cases=[]
for info in manifest['files']:
 path=root/info['name'];assert path.is_file()
 try:
  server=q.open_tiff(str(path));server.close();cases.append(dict(file=info['name'],unexpected_success=True))
 except (ValueError,RuntimeError) as error:
  cases.append(dict(file=info['name'],exception_type=type(error).__name__,message=str(error)))
env=q.build_environment();value=dict(scope='Unsupported baseline, not native multi-file acceptance',engine=dict(revision=env.git_revision,fingerprint=env.source_tree_sha256),cases=cases)
(r/'build/ome205-native-baseline-current.json').write_bytes((json.dumps(value,indent=2)+'\n').encode())
assert all(c.get('message')=='multi-file OME references are unsupported' for c in cases)
print('Both valid fixture entry points refuse multi-file references on retained4b0engine, as currently documented.')
