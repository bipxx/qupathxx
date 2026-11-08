from pathlib import Path
import json,hashlib,re
out=Path('../../raw-replay-arm64-252/offline');binary=Path('../../raw-replay-macos-240/hosted/macos-native-arm64/qupathxx_pixel_workflow_tests');data=binary.read_bytes()
def require(ok,message):
    if not ok:raise ValueError(message)
require(hashlib.sha256(data).hexdigest()=='a55c203c8fc7c05c1dfddd5b6ee024c63a638f5c908b75f32a34cc1d8239b9a2','changed retained binary')
symbols=(out/'symbols.log').read_text();unwind=(out/'unwind.log').read_text();fixups=(out/'dyld-info.log').read_text()
symbol_lines=[line for line in symbols.splitlines() if 'rejects<std::invalid_argument, raw_window_classifier_checks' in line and any("'lambda"+str(n)+"'" in line for n in [9,10])]
findings=[]
for line in symbol_lines:
    address=int(line.split()[0],16);offset=address-0x100000000
    match=re.search(r'function offset=0x0*'+format(offset,'x')+r', LSDA offset=0x([0-9a-f]+)',unwind);require(match is not None,'missing LSDA')
    lsda=int(match.group(1),16);header=data[lsda:lsda+28];require(header[:5]==bytes.fromhex('ff9b190111'),'unexpected LSDA header')
    # Fixed authenticated table format: indirect PC-relative signed32 type entry.
    class_info=lsda+3+header[2];entry=class_info-4;delta=int.from_bytes(data[entry:entry+4],'little',signed=True);slot=0x100000000+entry+delta
    binding=next(s for s in fixups.splitlines() if len(s.split())>=3 and s.split()[2].lower()==hex(slot).lower())
    require('__ZTISt12length_error' in binding,'unexpected bound catch type');require(header[22:24]==bytes([1,0]),'unexpected catch action')
    findings.append(dict(symbol=line,aliases_at_address=[v for v in symbols.splitlines() if v.startswith(line.split()[0]+' ')],function_address=hex(address),lsda_address=hex(0x100000000+lsda),lsda_bytes=header.hex(' '),catch_action_type_index=1,type_entry_address=hex(0x100000000+entry),pcrel_sdata4_delta=delta,indirect_slot=hex(slot),binding_line=binding,source_expected_catch='std::invalid_argument',binary_bound_catch='std::length_error'))
require(len(findings)==2,'missing target functions')
(out/'catch-table-findings.json').write_text(json.dumps(dict(product_sha256=hashlib.sha256(data).hexdigest(),findings=findings,interpretation='Retained final binary catch tables disagree with source template type. Linker/compiler origin and product repair remain unproven.',reference='https://github.com/llvm/llvm-project/blob/main/libcxxabi/src/cxa_personality.cpp',raw_files=[dict(path=p.name,bytes=p.stat().st_size,sha256=hashlib.sha256(p.read_bytes()).hexdigest()) for p in [out/name for name in ['symbols.log','unwind.log','dyld-info.log','sections.log','rejects-lambda9-disassembly.log','bind.log']]]),indent=2)+'\n',encoding='utf-8')
(out/'symbols-excerpt.log').write_text('\n'.join(v for v in symbols.splitlines() if any(v.startswith(line.split()[0]+' ') for line in symbol_lines))+'\n',encoding='utf-8')
(out/'unwind-excerpt.log').write_text('\n'.join(s for s in unwind.splitlines() if any(v in s for v in ['function offset=0x000611f8','function offset=0x000612ac']))+'\n',encoding='utf-8')
(out/'fixups-excerpt.log').write_text('\n'.join(s for s in fixups.splitlines() if any(v.lower() in s.lower() for v in ['0x100688D90','0x100688DA8']))+'\n',encoding='utf-8')
print('Two authenticated catch tables point to length_error, while source and symbols specify invalid_argument.')
