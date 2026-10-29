from pathlib import Path
import hashlib
import json
import shutil
import re
root=Path(__file__).resolve().parents[2];work=root/'build/ci/scientific-grid-159';out=root/'build/reader-locale-297'
proof=json.loads((out/'source-runtime-proof.json').read_text())
assert proof['tests']['preflight.xml']['passed']==1087
linux=json.loads((out/'linux/linux-command-results.json').read_text())
assert linux[-1]['label']=='linux-native' and all(p['returncode']==0 for p in linux)
notes=root.parent/'compilexx/docs/notes/qupathxx-build-inefficiencies.md';before=notes.read_bytes()
addition='''

## 2026-10-28: locale-independent format selection297 and terminal295

- Reproduce the CI failure with the retained installed binary before compiling.
  PPM decoding and explicit reader selection pass; automatic selection fails
  under code page1252 and Turkish locale. Bytewise C-locale tolower corrupts
  multibyte UTF-8 basenames. Fold only ASCII format tokens in one tiny private
  helper shared by OME detection, project/recovery and the region CLI. Preserve
  filename bytes and caller locale; do not switch the application's locale.
- Keep global LC_CTYPE changes inside isolated test processes. Use supported
  Windows BCP47 tr-TR, not an assumed Turkish_Turkey alias. Reopened project
  images are lazy servers: inspect metadata rather than requesting dense image
  ownership. Give lowercase/uppercase fixtures distinct numeric basenames so
  Windows case-insensitivity does not collide their project destinations.
  Preserve the initial observer failures and corrected old-runtime red results.
- The private header avoids changing the public binding header. Authenticate all
  71 previous native products before replacing the shared incremental cache;
  retain71 new products, verify425 compiler inputs,898 source paths and39 wheel
  package members. Bind native28c33573 and observere1dc6457 separately. No stub
  regeneration is needed for a reader-dispatch implementation fix.
- Windows passes55 native suites,31 reader/batch cases plus one standalone
  uppercase-Unicode region check,700 scientific cases/13 declared skips and1087
  auditor controls. Counts overlap. The old binary fails both non-C locale
  regressions and the uppercase standalone check. Minimal GCC ASan/UBSan checks
  the raw-image format path separately; this does not qualify project/GUI/reader
  capabilities disabled in that profile. Shared test dependencies are explicit.
- All11 original295 archives now authenticate. The original complete Mac auditor
  accepts its native-only role; portable also accepts; engine rejects both
  Windows profiles on the original Unicode failure. Preserve terminal295 as
  unaccepted overall. Do not redispatch it, relabel binaries or transfer its
  source-bound role acceptance to the repaired source. Qualify the successor.
- Keep broader biological/scientific, hardware, supported-platform, licensing and
  release gates open. A local implementation repair is concrete progress, not
  project completion, a speed-superiority claim or an M0-M7 exit.
'''.encode()
with notes.open('ab') as stream:stream.write(addition)
assert notes.read_bytes()==before+addition
(out/'compilexx-append-proof.json').write_text(json.dumps(dict(path=str(notes),prefix_bytes=len(before),prefix_sha256=hashlib.sha256(before).hexdigest(),appended_bytes=len(addition),final_sha256=hashlib.sha256(notes.read_bytes()).hexdigest(),prefix_preserved=True),indent=2)+'\n')
def publish(folder,entries,receipt):
    target=work/'docs/benchmarks'/folder;target.mkdir(exist_ok=False);files=[]
    for source,name in entries:
        dest=target/name;dest.parent.mkdir(parents=True,exist_ok=True);shutil.copyfile(source,dest);assert source.read_bytes()==dest.read_bytes()
        files.append(dict(path=name,bytes=dest.stat().st_size,sha256=hashlib.sha256(dest.read_bytes()).hexdigest()))
    (target/'receipt.json').write_text(json.dumps(dict(**receipt,files=files),indent=2)+'\n')
entries=[(p,p.relative_to(out).as_posix()) for p in sorted(out.rglob('*')) if p.is_file() and p.suffix in {'.json','.log','.xml','.hpp','.txt'} and 'final-products' not in p.parts and 'linux-products' not in p.parts]
for name in ('build_reader_locale297.py','reader_locale297_backend.py','validate_reader_locale297.py','prove_reader_locale297.py','prepare_linux_reader297.py','linux_reader_locale297.py','publish_reader_locale297.py'):
    entries.append((root/'build/ci'/name,'observers/'+name))
publish('reader-locale-297',entries,dict(**proof,state='locale-independent-reader-repair-locally-verified',linux_scope='minimal raw-image suite with ASan/UBSan; project/GUI/Python/OpenSlide/TIFF/GEOS/training/interchange disabled',old_runtime_red=True,prior_observer_errors_preserved=True,shared_test_dependencies=True,clean_machine=False,remaining=['Own-source complete hosted qualification','Broader research/science/biology/hardware/support/license/release gates and all M0-M7 exits']))
campaign=root/'build/nominal-cell-qualification-295';obs=root/'build/q295-all/20261028T235925Z'
audit=json.loads((campaign/'macos-acceptance.json').read_text());assert audit['native_engineering_accepted']
entries=[(campaign/name,name) for name in ('macos-run-terminal.json','macos-complete-audit.log','macos-audit-exit.json','macos-acceptance.json')]
entries.extend((obs/name,name) for name in ('macos-run.json','macos-artifacts.json','verified.json','macos-native-arm64-artifact.json','macos-native-x86_64-artifact.json'))
entries.append((root/'build/ci/audit_macos295_terminal297.py','observers/audit_macos295_terminal297.py'))
publish('nominal-cell-qualification-295/terminal297',entries,dict(schema='qpx.nominal-cell-qualification-terminal/1',state='terminal-unaccepted-overall',source='a7eb78de75387bce2da7b3d59b7f0a2ab6126cee',engine_run=40782637825,macos_run=40782648204,portable_run=40782658569,authenticated_archives=11,complete_original_auditors=True,engine_accepted=False,macos_native_accepted=True,portable_accepted=True,combined_accepted=False,main_promoted=False,release_ready=False))
card='''**Windows Unicode reader selection is repaired locally; candidate295 is terminal/unaccepted.**
The [repair297 receipt](benchmarks/reader-locale-297/receipt.json) binds native
28c33573 and observere1dc6457. ASCII-only format folding preserves UTF-8 filename
bytes and caller locale; automatic/explicit PPM opening, uppercase extension,
project reopen and nominal batch changed-source resume pass. Windows passes55
native suites,31 reader/batch checks plus one standalone region check,700
scientific cases/13 declared skips and1087 auditor controls. Minimal GCC
ASan/UBSan raw-image coverage is separate.425 compiler inputs,898 source paths,
71 retained products and39 installed package members authenticate. Test
dependencies are shared; this is local engineering evidence.
All11 frozen295 archives now authenticate. Its [terminal receipt](benchmarks/nominal-cell-qualification-295/terminal297/receipt.json)
records complete original Mac-native/portable acceptance and Windows engine
rejection. No old-source restart or acceptance transfer follows. Qualify the
repaired successor; retain qualified281. Nominal comparison296 still has8/9
exact synthetic nucleus/body cases and the explicit clipped-ROI differences.
Broader science/biology/resource/hardware/support/license/release gates and
every M0-M7 exit remain open.

'''
for name in ('current-task.md','feature-matrix.md','project-plan.md','api-delivery-plan.md'):
    path=work/'docs'/name;text=path.read_text(encoding='utf-8');start=text.index('**Nominal cell consumers are locally delivered; frozen295 portable passes.**');end=text.index('**Native original-source nominal cells291',start)
    assert end-start<2200
    headings=re.findall(r'^#{1,3} .+$',text,re.M)
    updated=text[:start]+card+text[end:]
    assert re.findall(r'^#{1,3} .+$',updated,re.M)==headings
    path.write_text(updated,encoding='utf-8',newline='\n')
path=work/'docs/project-plan.md';text=path.read_text();anchor=text.index('## Execution order from the present state');start=text.index('Candidate295 is frozen',anchor);end=text.index('The current task card',start)
assert end-start<900 and '\n## ' not in text[start:end]
text=text[:start]+'''Candidate295 is terminal/unaccepted overall: all11 archives authenticate,
original Mac-native and portable auditors accept their roles, and Windows engine
rejects Unicode auto-selection. Repair297 is locally verified at native28c33573;
freeze and qualify its successor using complete own-source auditors. Preserve
qualified281 and both terminal failed campaigns. Comparison296 retains its
separate two-host corpus evidence and explicit clipped-ROI differences. Continue
remaining SCI1-B workflows, the decision lane and REL1 through every M0-M7 exit.
'''+text[end:]
assert len(re.findall(r'^#{1,3} .+$',text,re.M))==19
path.write_text(text,encoding='utf-8',newline='\n')
print('Published repair297 and original terminal295 receipts; full plan scope preserved',flush=True)
