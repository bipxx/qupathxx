"""Bounded offline ELF64 little-endian x86-64 dynamic-name inventory.

Uses program headers, including for stripped/sectionless images. No extraction
or loading. Layouts: https://gabi.xinuos.com/elf/02-eheader.html,
07-pheader.html and 08-dynamic.html. GNU tags follow glibc elf/elf.h.
This is not a loader, symbol/ABI validation or dependency-resolution policy.
"""
from bisect import bisect_right
import struct


class ElfError(ValueError):
    pass


def require(condition, message):
    if not condition:
        raise ElfError(message)


class Image:
    def __init__(self, data):
        require(isinstance(data, bytes) and 64 <= len(data) <= 256*1024*1024, 'ELF byte budget or header differs')
        self.data = data
        self.name_bytes = 0
        require(data[:7] == b'\x7fELF\x02\x01\x01', 'require ELF64 little-endian version 1')
        require(data[7] in (0, 3) and data[8] == 0, 'unsupported ELF OS ABI')
        (self.kind, machine, version, entry, phoff, shoff, flags, ehsize,
         phsize, phcount, shsize, shcount, shnames) = self.unpack('<HHIQQQIHHHHHH', 16)
        require(self.kind in (2, 3) and machine == 62 and version == 1, 'require x86-64 executable/shared ELF')
        require(ehsize == 64 and phsize == 56 and 1 <= phcount <= 1024, 'invalid ELF program header layout/budget')
        require(phoff >= 64, 'program headers overlap ELF header')
        self.bounds(phoff, phcount*56)
        self.loads, dynamic, interpreters = [], [], []
        for i in range(phcount):
            kind, flags, offset, address, physical, size, memory, align = self.unpack('<IIQQQQQQ', phoff+i*56)
            if kind == 0:
                continue
            self.bounds(offset, size)
            require(address+memory <= 1 << 64, 'ELF virtual range overflows')
            if kind == 1:
                require(size <= memory, 'load file size exceeds memory size')
                require(align in (0, 1) or (align & (align-1) == 0 and address % align == offset % align), 'invalid ELF load alignment')
                require(not self.loads or address >= self.loads[-1][0]+self.loads[-1][3], 'overlapping or unordered ELF load ranges')
                self.loads.append((address, offset, size, memory))
            elif kind == 2:
                dynamic.append((address, offset, size))
            elif kind == 3:
                interpreters.append((offset, size))
        require(self.loads and len(dynamic) == 1 and len(interpreters) <= 1, 'require one dynamic segment and unique interpreter')
        self.load_starts = [item[0] for item in self.loads]
        self.interpreter = None
        if interpreters:
            offset, size = interpreters[0]
            require(2 <= size <= 4096, 'invalid ELF interpreter size')
            raw = self.take(offset, size)
            require(raw[-1:] == b'\0' and b'\0' not in raw[:-1], 'invalid ELF interpreter terminator')
            self.interpreter = self.text(raw[:-1])
            require(self.interpreter.startswith('/'), 'require absolute ELF interpreter')
        address, offset, size = dynamic[0]
        require(size and size % 16 == 0 and size//16 <= 65536, 'invalid ELF dynamic table size/budget')
        require(self.mapped(address, size) == offset, 'dynamic segment mapping differs')
        self.tags = {}
        for i in range(size//16):
            tag, value = self.unpack('<qQ', offset+i*16)
            if tag == 0:
                break
            self.tags.setdefault(tag, []).append(value)
        else:
            raise ElfError('unterminated ELF dynamic table')
        # These dynamic tags are singletons. Repeated NEEDED order is meaningful.
        for tag in [5, 10, 14, 15, 29, 30, 0x6ffffffb, 0x6ffffefb, 0x6ffffefc, 0x6ffffffe, 0x6fffffff]:
            require(len(self.tags.get(tag, [])) <= 1, 'duplicate singleton ELF dynamic tag')
        require(5 in self.tags and 10 in self.tags, 'missing ELF dynamic string table')
        size = self.tags[10][0]
        require(1 <= size <= 8*1024*1024, 'ELF string table exceeds budget')
        self.strings = self.take(self.mapped(self.tags[5][0], size), size)
        require(self.strings[:1] == b'\0' and self.strings[-1:] == b'\0', 'invalid ELF string table boundary')

    def bounds(self, offset, size):
        require(0 <= offset <= len(self.data) and 0 <= size <= len(self.data)-offset, 'ELF file range exceeds bounds')

    def take(self, offset, size):
        self.bounds(offset, size)
        return self.data[offset:offset+size]

    def unpack(self, layout, offset):
        self.bounds(offset, struct.calcsize(layout))
        return struct.unpack_from(layout, self.data, offset)

    def mapped(self, address, size):
        require(address+size <= 1 << 64, 'ELF mapped range overflows')
        index = bisect_right(self.load_starts, address)-1
        require(index >= 0, 'ELF address is not file-backed')
        start, offset, length, memory = self.loads[index]
        require(size <= length and address-start <= length-size, 'ELF address is not file-backed')
        return offset+address-start

    @staticmethod
    def text(raw):
        require(len(raw) <= 32768 and all(32 <= c < 127 for c in raw), 'unsupported ELF name bytes/budget')
        return raw.decode('ascii')

    def string(self, index):
        require(0 <= index < len(self.strings), 'ELF string offset exceeds table')
        end = self.strings.find(b'\0', index, min(index+32769, len(self.strings)))
        require(end >= 0, 'ELF string is unterminated or exceeds budget')
        self.name_bytes += end-index
        require(self.name_bytes <= 4*1024*1024, 'ELF dependency names exceed aggregate budget')
        return self.text(self.strings[index:end])

    def names(self, tag):
        result = [self.string(index) for index in self.tags.get(tag, [])]
        require(all(result), 'empty ELF dependency/identity')
        return result

    def version_requirements(self):
        address = self.tags.get(0x6ffffffe)
        count = self.tags.get(0x6fffffff)
        require(bool(address) == bool(count), 'incomplete ELF version requirement tags')
        if not address:
            return []
        address, count = address[0], count[0]
        require(1 <= count <= 4096, 'ELF version requirement count exceeds budget')
        result, occupied, total = [], [], 0
        for i in range(count):
            at = self.mapped(address, 16)
            occupied.append((at, at+16))
            version, size, filename, auxiliary, following = self.unpack('<HHIII', at)
            require(version == 1 and 1 <= size <= 4096, 'invalid ELF version requirement header')
            total += size
            require(total <= 65536, 'ELF version auxiliaries exceed budget')
            require(auxiliary >= 16, 'ELF version auxiliary overlaps header')
            dependency = self.string(filename)
            require(bool(dependency), 'empty ELF version dependency')
            items, cursor = [], address+auxiliary
            for j in range(size):
                at = self.mapped(cursor, 16)
                occupied.append((at, at+16))
                name_hash, flags, index, name, next_aux = self.unpack('<IHHII', at)
                text = self.string(name)
                require(bool(text), 'empty ELF required version')
                items.append(dict(name=text, hash=name_hash, flags=flags, index=index))
                require(next_aux >= 16 if j+1 < size else next_aux == 0, 'ELF version auxiliary chain/count differs')
                cursor += next_aux
            result.append(dict(file=dependency, versions=items))
            require(following >= 16 if i+1 < count else following == 0, 'ELF version requirement chain/count differs')
            address += following
        occupied.sort()
        require(all(left[1] <= right[0] for left, right in zip(occupied, occupied[1:])), 'overlapping ELF version records')
        return result


def inspect_elf(data):
    image = Image(data)
    def optional(tag):
        values = image.names(tag)
        return values[0] if values else None
    def search(tag):
        values = image.tags.get(tag, [])
        return image.string(values[0]).split(':') if values else None
    return dict(format='ELF64', machine='x86_64', endian='little',
                kind='executable' if image.kind == 2 or image.interpreter or image.tags.get(0x6ffffffb, [0])[0] & 0x08000000 else 'shared-object',
                interpreter=image.interpreter, needed=image.names(1), soname=optional(14),
                rpath=search(15), runpath=search(29), auxiliary=image.names(0x7ffffffd),
                filters=image.names(0x7fffffff), audit=optional(0x6ffffefc), depaudit=optional(0x6ffffefb),
                version_requirements=image.version_requirements())
