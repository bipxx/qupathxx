"""Bounded offline thin/universal Mach-O64 x86-64/arm64 load-name inventory.

Layouts: Apple xnu EXTERNAL_HEADERS/mach-o/loader.h and cctools
include/mach-o/fat.h. No loading, symbol/ABI validation or path resolution.
"""
import hashlib
import struct


class MachoError(ValueError):
    pass


def require(condition, message):
    if not condition:
        raise MachoError(message)


CPUS = {0x01000007: 'x86_64', 0x0100000c: 'arm64'}
KINDS = {2: 'executable', 6: 'dylib', 7: 'dynamic-linker', 8: 'bundle'}
DYLIBS = {0xc: 'load', 0x80000018: 'weak', 0x8000001f: 'reexport', 0x20: 'lazy', 0x80000023: 'upward'}
STRINGS = {0x8000001c: 'rpaths', 0xe: 'interpreters', 0xf: 'interpreter_ids', 0x27: 'dyld_environment',
           0x12: 'sub_frameworks', 0x13: 'sub_umbrellas', 0x14: 'sub_clients', 0x15: 'sub_libraries'}
REQUIRED = set(DYLIBS) | {0x8000001c, 0x80000022, 0x80000028, 0x80000033, 0x80000034}


class Slice:
    def __init__(self, data):
        self.data = data
        self.name_bytes = 0

    def bounds(self, offset, size):
        require(0 <= offset <= len(self.data) and 0 <= size <= len(self.data)-offset, 'Mach-O file range exceeds bounds')

    def unpack(self, layout, offset):
        self.bounds(offset, struct.calcsize(layout))
        return struct.unpack_from(layout, self.data, offset)

    def name(self, at, size, minimum):
        index, = self.unpack('<I', at+8)
        require(minimum <= index < size, 'Mach-O string offset overlaps command header or exceeds command')
        raw = bytes(self.data[at+index:at+min(size, index+32769)])
        end = raw.find(b'\0')
        require(end > 0, 'empty, unterminated or oversized Mach-O name')
        raw = raw[:end]
        self.name_bytes += len(raw)
        require(self.name_bytes <= 4*1024*1024, 'Mach-O names exceed aggregate budget')
        try:
            text = raw.decode('utf-8')
        except UnicodeDecodeError as error:
            raise MachoError('invalid UTF-8 Mach-O name') from error
        require(not any(ord(c) < 32 or ord(c) == 127 for c in text), 'control byte in Mach-O name')
        return text

    def inspect(self):
        magic, cpu, subtype, kind, count, size, flags, reserved = self.unpack('<IIIIIIII', 0)
        require(magic == 0xfeedfacf and cpu in CPUS and kind in KINDS and reserved == 0, 'unsupported Mach-O64 identity')
        require(0 <= count <= 8192 and size <= 16*1024*1024, 'Mach-O command budget exceeded')
        self.bounds(32, size)
        result = dict(machine=CPUS[cpu], cpu_type=cpu, cpu_subtype=subtype, kind=KINDS[kind], flags=flags,
                      dependencies=[], dylib_id=None, minimum_os=[], uuid=None,
                      **{field: [] for field in STRINGS.values()})
        cursor, commands, segments = 32, [], []
        for i in range(count):
            require(cursor+8 <= 32+size, 'Mach-O command header exceeds command table')
            command, length = self.unpack('<II', cursor)
            require(length >= 8 and length % 8 == 0 and length <= 32+size-cursor, 'invalid Mach-O command size/alignment')
            require(not command & 0x80000000 or command in REQUIRED, 'unsupported required Mach-O command')
            require(command not in [6, 7, 0x10], 'legacy Mach-O dependency command requires explicit support')
            commands.append(command)
            if command in DYLIBS or command == 0xd:
                require(length >= 24, 'truncated Mach-O dylib command')
                timestamp, current, compatibility = self.unpack('<III', cursor+12)
                record = dict(name=self.name(cursor, length, 24), timestamp=timestamp,
                              current_version=current, compatibility_version=compatibility)
                if command == 0xd:
                    require(result['dylib_id'] is None and kind == 6, 'duplicate or misplaced Mach-O dylib identity')
                    result['dylib_id'] = record
                else:
                    result['dependencies'].append(dict(mechanism=DYLIBS[command], **record))
            elif command in STRINGS:
                require(length >= 16, 'truncated Mach-O string command')
                result[STRINGS[command]].append(self.name(cursor, length, 12))
            elif command == 0x1b:
                require(length == 24 and result['uuid'] is None, 'invalid or duplicate Mach-O UUID')
                result['uuid'] = bytes(self.data[cursor+8:cursor+24]).hex()
            elif command in [0x24, 0x32]:
                if command == 0x24:
                    require(length == 16, 'invalid Mach-O minimum macOS command')
                    minimum, sdk = self.unpack('<II', cursor+8)
                    tools = []
                else:
                    require(length >= 24, 'truncated Mach-O build version')
                    platform, minimum, sdk, tool_count = self.unpack('<IIII', cursor+8)
                    require(platform == 1 and tool_count <= 64 and length == 24+8*tool_count, 'unsupported platform or malformed Mach-O build tools')
                    tools = [dict(zip(['tool', 'version'], self.unpack('<II', cursor+24+j*8))) for j in range(tool_count)]
                result['minimum_os'].append(dict(command=command, platform='macos', minimum=minimum, sdk=sdk, tools=tools))
            elif command in [0x25, 0x2f, 0x30]:
                raise MachoError('non-macOS minimum-version command')
            elif command == 0x19:
                require(length >= 72, 'truncated Mach-O64 segment')
                address, memory, offset, file_size, maximum, initial, sections, segflags = self.unpack('<QQQQIIII', cursor+24)
                require(sections <= 4096 and length == 72+80*sections, 'invalid Mach-O section layout/budget')
                require(file_size <= memory and address+memory <= 1 << 64, 'invalid Mach-O segment virtual range')
                self.bounds(offset, file_size)
                if file_size:
                    segments.append((offset, offset+file_size))
                for j in range(sections):
                    section = cursor+72+j*80
                    start, extent, file_at, alignment, reloc_at, reloc_count, attributes = self.unpack('<QQIIIII', section+32)
                    require(address <= start and extent <= memory and start-address <= memory-extent, 'Mach-O section exceeds segment memory')
                    require(alignment <= 63 and reloc_count <= 1000000, 'Mach-O section alignment/relocation budget exceeded')
                    self.bounds(reloc_at, reloc_count*8)
                    if attributes & 0xff not in [1, 0xc, 0x12]:
                        require(offset <= file_at and extent <= file_size and file_at-offset <= file_size-extent, 'Mach-O section exceeds file-backed segment')
            cursor += length
        require(cursor == 32+size, 'Mach-O command count/size differs')
        segments.sort()
        require(all(left[1] <= right[0] for left, right in zip(segments, segments[1:])), 'overlapping Mach-O file segments')
        require(len(result['interpreters']) <= 1 and len(result['interpreter_ids']) <= 1, 'duplicate Mach-O interpreter command')
        result['commands'] = commands
        return result


def inspect_macho(data):
    require(isinstance(data, bytes) and 32 <= len(data) <= 256*1024*1024, 'Mach-O byte budget or header differs')
    view = memoryview(data)
    magic = data[:4]
    slices = []
    if magic == b'\xcf\xfa\xed\xfe':
        slices.append(dict(offset=0, bytes=len(data), sha256=hashlib.sha256(view).hexdigest(), **Slice(view).inspect()))
        container = 'thin64'
    else:
        require(magic in (b'\xca\xfe\xba\xbe', b'\xca\xfe\xba\xbf'), 'unsupported Mach-O thin/fat magic or byte order')
        wide = magic == b'\xca\xfe\xba\xbf'
        count, = struct.unpack_from('>I', data, 4)
        entry_size = 32 if wide else 20
        require(1 <= count <= 16 and 8+count*entry_size <= len(data), 'invalid Mach-O universal slice budget/table')
        occupied, identities = [], set()
        for i in range(count):
            if wide:
                cpu, subtype, offset, size, align, reserved = struct.unpack_from('>IIQQII', data, 8+i*entry_size)
                require(reserved == 0, 'invalid Mach-O universal reserved field')
            else:
                cpu, subtype, offset, size, align = struct.unpack_from('>IIIII', data, 8+i*entry_size)
            require(cpu in CPUS and (cpu, subtype) not in identities, 'unsupported or duplicate Mach-O universal architecture')
            identities.add((cpu, subtype))
            require(align <= 63 and offset % (1 << align) == 0, 'invalid Mach-O universal alignment')
            require(offset >= 8+count*entry_size and size >= 32 and offset <= len(data) and size <= len(data)-offset, 'Mach-O universal slice exceeds bounds or overlaps header')
            require(all(offset >= end or start >= offset+size for start, end in occupied), 'overlapping Mach-O universal slices')
            occupied.append((offset, offset+size))
            binary = Slice(view[offset:offset+size]).inspect()
            require(binary['cpu_type'] == cpu and binary['cpu_subtype'] == subtype, 'Mach-O universal architecture differs from slice')
            slices.append(dict(offset=offset, bytes=size, sha256=hashlib.sha256(view[offset:offset+size]).hexdigest(), **binary))
        occupied.sort()
        require(all(left[1] <= right[0] for left, right in zip(occupied, occupied[1:])), 'overlapping Mach-O universal slices')
        container = 'fat64' if wide else 'fat32'
    return dict(format='Mach-O64', container=container, slices=slices)
