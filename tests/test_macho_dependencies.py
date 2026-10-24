"""Independent Mach-O bytes; never execute or compile fixture binaries."""
import hashlib
import importlib.util
from pathlib import Path
import struct

import pytest

spec=importlib.util.spec_from_file_location('qpx_macho_tests',Path(__file__).resolve().parents[1]/'tools/ci/macho_dependencies.py')
macho=importlib.util.module_from_spec(spec)
spec.loader.exec_module(macho)


def command(kind, payload):
    length=(8+len(payload)+7)//8*8
    return struct.pack('<II',kind,length)+payload+b'\0'*(length-8-len(payload))


def named(kind, text, dylib=False):
    payload=struct.pack('<IIII',24,123,0x010203,0x010000) if dylib else struct.pack('<I',12)
    return command(kind,payload+text.encode('utf-8')+b'\0')


def thin(cpu=0x01000007, subtype=None, kind=6, extra=(), segment=True):
    subtype=(3 if cpu==0x01000007 else 0) if subtype is None else subtype
    commands=[]
    if segment:
        section=struct.pack('<16s16sQQIIIIIIII',b'__text',b'__TEXT',0x100001000,0x100,0x1000,4,0,0,0,0,0,0)
        commands.append(command(0x19,struct.pack('<16sQQQQIIII',b'__TEXT',0x100000000,0x3000,0,0x2000,7,5,1,0)+section))
    if kind==6:commands.append(named(0xd,'@rpath/libfixture.dylib',True))
    commands.extend([named(0xc,'/usr/lib/libSystem.B.dylib',True),named(0x8000001c,'@loader_path/libs'),
                     command(0x1b,bytes(range(16))),command(0x32,struct.pack('<IIIIII',1,0x0b0000,0x0f0000,1,3,0x010203))])
    commands.extend(extra)
    data=bytearray(0x2000)
    struct.pack_into('<IIIIIIII',data,0,0xfeedfacf,cpu,subtype,kind,len(commands),sum(map(len,commands)),0x85,0)
    cursor=32
    for value in commands:data[cursor:cursor+len(value)]=value;cursor+=len(value)
    return bytes(data)


def universal(wide=False):
    first,second=thin(),thin(cpu=0x0100000c)
    data=bytearray(0xa000)
    struct.pack_into('>II',data,0,0xcafebabf if wide else 0xcafebabe,2)
    for i,(cpu,subtype,offset,blob) in enumerate([(0x01000007,3,0x4000,first),(0x0100000c,0,0x8000,second)]):
        if wide:struct.pack_into('>IIQQII',data,8+i*32,cpu,subtype,offset,len(blob),14,0)
        else:struct.pack_into('>IIIII',data,8+i*20,cpu,subtype,offset,len(blob),14)
        data[offset:offset+len(blob)]=blob
    return bytes(data)


def test_all_dependency_kinds_versions_order_unicode_and_metadata():
    values=[named(code,'@rpath/lib'+str(i)+'.dylib',True) for i,code in enumerate([0x80000018,0x8000001f,0x20,0x80000023])]
    values += [named(0x8000001c,'@loader_path/é'),named(0xe,'/usr/lib/dyld'),named(0x27,'DYLD_TEST=observed'),
               named(0x12,'Framework'),named(0x13,'Umbrella'),named(0x14,'Client'),named(0x15,'Library')]
    data=thin(extra=values)
    result=macho.inspect_macho(data)
    assert result['container']=='thin64' and len(result['slices'])==1
    row=result['slices'][0]
    assert row['machine']=='x86_64' and row['kind']=='dylib' and row['flags']==0x85
    assert row['sha256']==hashlib.sha256(data).hexdigest()
    assert [item['mechanism'] for item in row['dependencies']]==['load','weak','reexport','lazy','upward']
    assert all(item['current_version']==0x010203 and item['compatibility_version']==0x010000 for item in row['dependencies'])
    assert row['dylib_id']['name']=='@rpath/libfixture.dylib'
    assert row['rpaths']==['@loader_path/libs','@loader_path/é']
    assert row['interpreters']==['/usr/lib/dyld'] and row['dyld_environment']==['DYLD_TEST=observed']
    assert row['sub_frameworks']==['Framework'] and row['sub_umbrellas']==['Umbrella']
    assert row['sub_clients']==['Client'] and row['sub_libraries']==['Library']
    assert row['uuid']==bytes(range(16)).hex() and row['minimum_os'][0]['minimum']==0x0b0000
    assert row['minimum_os'][0]['tools']==[dict(tool=3,version=0x010203)]


@pytest.mark.parametrize('wide',[False,True])
def test_universal_slice_offsets_architectures_and_hashes(wide):
    data=universal(wide)
    result=macho.inspect_macho(data)
    assert result['container']==('fat64' if wide else 'fat32')
    assert [item['machine'] for item in result['slices']]==['x86_64','arm64']
    assert [item['offset'] for item in result['slices']]==[0x4000,0x8000]
    for row in result['slices']:
        assert row['sha256']==hashlib.sha256(data[row['offset']:row['offset']+row['bytes']]).hexdigest()


@pytest.mark.parametrize('kind,label',[(2,'executable'),(7,'dynamic-linker'),(8,'bundle')])
def test_supported_image_kinds(kind,label):
    assert macho.inspect_macho(thin(kind=kind))['slices'][0]['kind']==label


@pytest.mark.parametrize('offset,layout,value',[
    (0,'I',0), (4,'I',7), (12,'I',1), (16,'I',8193),(20,'I',16*1024*1024+1),(20,'I',0),
    (28,'I',1),(36,'I',0),(36,'I',7),(36,'I',0x2000),
    (32+48,'Q',0x4000),(32+40,'Q',1),(32+24,'Q',0xffffffffffffffff),
    (32+64,'I',4097),(32+72+32,'Q',0),(32+72+40,'Q',0x100000),
    (32+72+48,'I',0x2000),(32+72+52,'I',64),
    (32+72+56,'I',0x2001),(32+72+60,'I',1000001),
    (32+72+60,'I',0x1000),
])
def test_corrupt_headers_commands_segments_and_sections_fail(offset,layout,value):
    data=bytearray(thin())
    struct.pack_into('<'+layout,data,offset,value)
    with pytest.raises(macho.MachoError):macho.inspect_macho(bytes(data))


@pytest.mark.parametrize('extra',[
    command(0x80000100,b''), command(6,b''), command(0x10,b''),
    command(0xc,b''), command(0x8000001c,b''),named(0xc,'',True),
    named(0xc,'bad\nname',True),named(0xd,'second-id',True),
    command(0x1b,bytes(range(16))),named(0xe,'first')+named(0xe,'second'),
    command(0x32,struct.pack('<IIII',2,0,0,0)),
    command(0x32,struct.pack('<IIII',1,0,0,65)),
    command(0x25,struct.pack('<II',0,0)),
])
def test_invalid_or_unsupported_load_command_content(extra):
    # A concatenated pair of commands is deliberately miscounted in one case;
    # the command-size/count consistency guard must also reject it.
    with pytest.raises(macho.MachoError):macho.inspect_macho(thin(extra=[extra]))


@pytest.mark.parametrize('wide',[False,True])
@pytest.mark.parametrize('failure',['count','cpu','duplicate','offset','size','alignment','overlap','slice_cpu','slice_subtype','reserved'])
def test_corrupt_universal_tables_fail(wide,failure):
    data=bytearray(universal(wide))
    entry=32 if wide else 20
    integer='Q' if wide else 'I'
    if failure=='count':struct.pack_into('>I',data,4,17)
    elif failure=='cpu':struct.pack_into('>I',data,8,7)
    elif failure=='duplicate':data[8+entry:16+entry]=data[8:16]
    elif failure=='offset':struct.pack_into('>'+integer,data,16,8)
    elif failure=='size':struct.pack_into('>'+integer,data,24 if wide else 20,0xfffffff0)
    elif failure=='alignment':struct.pack_into('>I',data,32 if wide else 24,64)
    elif failure=='overlap':struct.pack_into('>'+integer,data,16+entry,0x4000)
    elif failure=='slice_cpu':struct.pack_into('<I',data,0x4004,0x0100000c)
    elif failure=='slice_subtype':struct.pack_into('<I',data,0x4008,0)
    elif failure=='reserved':
        if wide:struct.pack_into('>I',data,36,1)
        else:struct.pack_into('<I',data,0x401c,1)
    with pytest.raises(macho.MachoError):macho.inspect_macho(bytes(data))


def test_invalid_name_offsets_terminators_utf8_and_command_count():
    for failure in ['header_offset','outside','terminator','utf8','count']:
        data=bytearray(thin(segment=False))
        if failure=='header_offset':struct.pack_into('<I',data,40,8)
        elif failure=='outside':struct.pack_into('<I',data,40,100000)
        elif failure=='terminator':
            length=int.from_bytes(data[36:40],'little');data[56:32+length]=b'x'*(length-24)
        elif failure=='utf8':data[56]=255
        else:struct.pack_into('<I',data,16,0)
        with pytest.raises(macho.MachoError):macho.inspect_macho(bytes(data))


def test_non_bytes_and_truncated_inputs():
    for data in [None,bytearray(thin()),b'',b'\0'*32,thin()[:31],universal()[:45]]:
        with pytest.raises(macho.MachoError):macho.inspect_macho(data)


def test_zero_relocation_count_and_zero_fill_do_not_require_file_payload():
    data=bytearray(thin())
    struct.pack_into('<I',data,32+72+56,len(data))
    assert macho.inspect_macho(bytes(data))['slices'][0]['kind']=='dylib'
    struct.pack_into('<I',data,32+72+48,0xffffffff)
    struct.pack_into('<I',data,32+72+64,1)
    assert macho.inspect_macho(bytes(data))['slices'][0]['kind']=='dylib'


def test_duplicate_interpreter_and_overlapping_file_segments():
    with pytest.raises(macho.MachoError,match='duplicate Mach-O interpreter'):
        macho.inspect_macho(thin(extra=[named(0xe,'first'),named(0xe,'second')]))
    data=thin()
    with pytest.raises(macho.MachoError,match='overlapping Mach-O file segments'):
        macho.inspect_macho(thin(extra=[data[32:184]]))


def test_name_and_aggregate_expansion_budgets():
    maximum='x'*32768
    assert macho.inspect_macho(thin(segment=False,extra=[named(0x8000001c,maximum)]))['slices'][0]['rpaths'][-1]==maximum
    with pytest.raises(macho.MachoError,match='oversized Mach-O name'):
        macho.inspect_macho(thin(segment=False,extra=[named(0x8000001c,maximum+'x')]))
    with pytest.raises(macho.MachoError,match='aggregate budget'):
        macho.inspect_macho(thin(segment=False,extra=[named(0x8000001c,maximum)]*129))


def test_universal_subtype_capability_bits_are_preserved():
    data=bytearray(universal())
    struct.pack_into('>I',data,32,0x80000002)
    struct.pack_into('<I',data,0x8008,0x80000002)
    assert macho.inspect_macho(bytes(data))['slices'][1]['cpu_subtype']==0x80000002
