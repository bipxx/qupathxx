"""Independent sectionless ELF byte fixtures; do not compile/load binaries."""
import importlib.util
from pathlib import Path
import struct

import pytest

spec = importlib.util.spec_from_file_location('qpx_elf_dependencies', Path(__file__).resolve().parents[1]/'tools/ci/elf_dependencies.py')
elf = importlib.util.module_from_spec(spec)
spec.loader.exec_module(elf)


def image(needed=('libc.so.6', 'libCase.so.1'), interpreter=None, rpath=None,
          runpath=None, soname='libfixture.so.1', extra=(), executable=False):
    data = bytearray(0x1200)
    data[:9] = b'\x7fELF\x02\x01\x01\0\0'
    struct.pack_into('<HHIQQQIHHHHHH', data, 16, 2 if executable else 3, 62, 1,
                     0, 64, 0, 0, 64, 56, 3, 0, 0, 0)
    # File offsets are deliberately different from dynamic virtual addresses.
    if interpreter is not None:
        raw = interpreter.encode('ascii')+b'\0'
        data[0x100:0x100+len(raw)] = raw
        struct.pack_into('<IIQQQQQQ', data, 64, 3, 4, 0x100, 0, 0, len(raw), len(raw), 1)
    struct.pack_into('<IIQQQQQQ', data, 120, 1, 4, 0x200, 0x400000, 0, 0x1000, 0x1100, 0x100)
    struct.pack_into('<IIQQQQQQ', data, 176, 2, 4, 0x300, 0x400100, 0, 0x200, 0x200, 8)
    strings = bytearray(b'\0')
    tags = [(5, 0x400600)]
    def name(tag, text):
        tags.append((tag, len(strings)))
        strings.extend(text.encode('ascii')+b'\0')
    for text in needed:
        name(1, text)
    if soname is not None:
        name(14, soname)
    for tag, text in [(15, rpath), (29, runpath)]:
        if text is not None:
            name(tag, text)
    for tag, text in extra:
        if isinstance(text, str):
            name(tag, text)
        else:
            tags.append((tag, text))
    tags.append((10, len(strings)))
    for i, (tag, value) in enumerate(tags):
        struct.pack_into('<qQ', data, 0x300+i*16, tag, value)
    data[0x800:0x800+len(strings)] = strings
    return bytes(data)


def test_sectionless_elf_preserves_dependency_order_case_paths_and_gnu_names():
    result = elf.inspect_elf(image(needed=('libZ.so', 'libA.so', 'libZ.so', '/opt/libfixed.so'),
        rpath='$ORIGIN/old', runpath='$ORIGIN:$ORIGIN/../qupathxx.libs::', extra=[
            (0x7ffffffd, 'libaux.so'), (0x7fffffff, 'libfilter.so'),
            (0x6ffffefc, 'audit.so'), (0x6ffffefb, 'depaudit.so')]))
    assert result == dict(format='ELF64', machine='x86_64', endian='little', kind='shared-object',
        interpreter=None, needed=['libZ.so', 'libA.so', 'libZ.so', '/opt/libfixed.so'],
        soname='libfixture.so.1', rpath=['$ORIGIN/old'],
        runpath=['$ORIGIN', '$ORIGIN/../qupathxx.libs', '', ''],
        auxiliary=['libaux.so'], filters=['libfilter.so'], audit='audit.so', depaudit='depaudit.so', version_requirements=[])


@pytest.mark.parametrize('options', [dict(executable=True), dict(interpreter='/lib64/ld-linux-x86-64.so.2'),
                                   dict(extra=[(0x6ffffffb, 0x08000000)])])
def test_exec_interpreter_and_pie_identity(options):
    assert elf.inspect_elf(image(**options))['kind'] == 'executable'


def test_no_dependencies_or_search_tags_is_explicit():
    result = elf.inspect_elf(image(needed=(), soname=None))
    assert result['needed'] == [] and result['soname'] is None
    assert result['rpath'] is None and result['runpath'] is None
    assert elf.inspect_elf(image(runpath=''))['runpath'] == ['']


@pytest.mark.parametrize('offset,layout,value', [
    (0, 'B', 0), (4, 'B', 1), (5, 'B', 2), (6, 'B', 0), (7, 'B', 9), (8, 'B', 1),
    (16, 'H', 1), (18, 'H', 183), (20, 'I', 2), (32, 'Q', 0), (32, 'Q', 0xffffffffffffffff),
    (52, 'H', 63), (54, 'H', 55), (56, 'H', 0), (56, 'H', 1025),
    (120+32, 'Q', 0x1200), (120+32, 'Q', 0x1101), (120+40, 'Q', 1),
    (120+16, 'Q', 0xffffffffffffffff), (120+48, 'Q', 3), (120+48, 'Q', 0x1000),
    (176, 'I', 0), (176+8, 'Q', 0x310), (176+16, 'Q', 0x400101),
    (176+32, 'Q', 0), (176+32, 'Q', 31), (176+32, 'Q', 0x10000000),
    (0x308, 'Q', 0x401000), (0x308, 'Q', 0x400fff), (0x308, 'Q', 0xffffffffffffffff),
    (0x318, 'Q', 0x100000), (0x318, 'Q', 0),
])
def test_corrupt_header_segment_and_string_ranges_are_rejected(offset, layout, value):
    data = bytearray(image())
    struct.pack_into('<'+layout, data, offset, value)
    with pytest.raises(elf.ElfError):
        elf.inspect_elf(bytes(data))


@pytest.mark.parametrize('failure', ['truncated', 'unterminated_dynamic', 'duplicate_tag',
    'missing_strings', 'missing_size', 'huge_strings', 'string_boundary', 'nonascii',
    'control', 'interpreter_nul', 'relative_interpreter', 'duplicate_interpreter', 'duplicate_dynamic',
    'overlapping_load', 'bss_strings', 'string_without_terminator'])
def test_corrupt_tables_and_ambiguous_layouts_fail(failure):
    data = bytearray(image(interpreter='/lib64/ld-linux-x86-64.so.2'))
    if failure == 'truncated':
        data = data[:200]
    elif failure == 'unterminated_dynamic':
        for offset in range(0x300, 0x500, 16):
            struct.pack_into('<qQ', data, offset, 1, 1)
    elif failure == 'duplicate_tag':
        struct.pack_into('<qQ', data, 0x350, 5, 0x400600)
    elif failure == 'missing_strings':
        struct.pack_into('<q', data, 0x300, 4)
    elif failure == 'missing_size':
        struct.pack_into('<q', data, 0x340, 4)
    elif failure == 'huge_strings':
        struct.pack_into('<Q', data, 0x348, 8*1024*1024+1)
    elif failure == 'string_boundary':
        data[0x800] = 1
    elif failure in ['nonascii', 'control']:
        data[0x801] = 255 if failure == 'nonascii' else 10
    elif failure == 'interpreter_nul':
        data[0x101] = 0
    elif failure == 'relative_interpreter':
        data[0x100] = ord('x')
    elif failure in ['duplicate_interpreter', 'duplicate_dynamic', 'overlapping_load']:
        struct.pack_into('<H', data, 56, 4)
        origin = {'duplicate_interpreter': 64, 'duplicate_dynamic': 176, 'overlapping_load': 120}[failure]
        data[232:288] = data[origin:origin+56]
    elif failure == 'bss_strings':
        struct.pack_into('<Q', data, 0x308, 0x401000)
    elif failure == 'string_without_terminator':
        data[0x801:0x800+int.from_bytes(data[0x348:0x350], 'little')] = b'x'*(int.from_bytes(data[0x348:0x350], 'little')-1)
    with pytest.raises(elf.ElfError):
        elf.inspect_elf(bytes(data))


def test_non_bytes_inputs_are_rejected():
    for data in [None, bytearray(image()), b'', b'\0'*64]:
        with pytest.raises(elf.ElfError):
            elf.inspect_elf(data)


def test_repeated_string_references_cannot_expand_past_name_budget():
    data = bytearray(image())
    data.extend(b'\0'*(0x10000-len(data)))
    data[0x800:0x1802] = b'\0'+b'x'*4096+b'\0'
    struct.pack_into('<QQ', data, 120+32, 0xfe00, 0xfe00)
    tags = [(5, 0x400600), (10, 4098)]+[(1, 1)]*1025+[(0, 0)]
    struct.pack_into('<IIQQQQQQ', data, 176, 2, 4, 0x8000, 0x407e00, 0, len(tags)*16, len(tags)*16, 8)
    for i, (tag, value) in enumerate(tags):
        struct.pack_into('<qQ', data, 0x8000+i*16, tag, value)
    with pytest.raises(elf.ElfError, match='aggregate budget'):
        elf.inspect_elf(bytes(data))


def versioned_image():
    data = bytearray(image(needed=('libc.so.6',), soname=None, extra=[
        (0x6ffffffe, 0x400800), (0x6fffffff, 1)]))
    # String indices: 1=libc.so.6, 11=GLIBC_2.28, 22=GLIBC_2.2.5.
    strings = b'\0libc.so.6\0GLIBC_2.28\0GLIBC_2.2.5\0'
    data[0x800:0x800+len(strings)] = strings
    struct.pack_into('<Q', data, 0x348, len(strings))
    struct.pack_into('<HHIII', data, 0xa00, 1, 2, 1, 16, 0)
    struct.pack_into('<IHHII', data, 0xa10, 0x1234, 0, 2, 11, 16)
    struct.pack_into('<IHHII', data, 0xa20, 0x5678, 2, 3, 22, 0)
    return bytes(data)


def test_version_requirements_use_virtual_addresses_without_sections():
    assert elf.inspect_elf(versioned_image())['version_requirements'] == [dict(file='libc.so.6', versions=[
        dict(name='GLIBC_2.28', hash=0x1234, flags=0, index=2),
        dict(name='GLIBC_2.2.5', hash=0x5678, flags=2, index=3)])]


@pytest.mark.parametrize('offset,layout,value', [
    (0x320, 'q', 4), (0x330, 'q', 4), (0x338, 'Q', 0), (0x338, 'Q', 4097),
    (0x328, 'Q', 0x401000), (0xa00, 'H', 2), (0xa02, 'H', 0),
    (0xa02, 'H', 4097), (0xa04, 'I', 0), (0xa08, 'I', 0),
    (0xa08, 'I', 0xffffffff), (0xa0c, 'I', 16),
    (0xa18, 'I', 0), (0xa18, 'I', 4096), (0xa1c, 'I', 0),
    (0xa1c, 'I', 8), (0xa1c, 'I', 0xffffffff), (0xa2c, 'I', 16),
])
def test_corrupt_version_pointer_count_name_and_chain_fail(offset, layout, value):
    data = bytearray(versioned_image())
    struct.pack_into('<'+layout, data, offset, value)
    with pytest.raises(elf.ElfError):
        elf.inspect_elf(bytes(data))


def test_multiple_version_dependencies_and_overlapping_auxiliaries():
    data = bytearray(versioned_image())
    struct.pack_into('<Q', data, 0x338, 2)
    struct.pack_into('<I', data, 0xa0c, 48)
    struct.pack_into('<HHIII', data, 0xa30, 1, 1, 1, 16, 0)
    struct.pack_into('<IHHII', data, 0xa40, 0x1234, 0, 4, 11, 0)
    result = elf.inspect_elf(bytes(data))['version_requirements']
    assert len(result) == 2 and [len(row['versions']) for row in result] == [2, 1]
    assert result[1]['versions'][0]['index'] == 4
    # Two otherwise well-formed dependency headers point to the same auxiliary.
    struct.pack_into('<H', data, 0xa02, 1)
    struct.pack_into('<I', data, 0xa08, 64)
    with pytest.raises(elf.ElfError, match='overlapping ELF version records'):
        elf.inspect_elf(bytes(data))
