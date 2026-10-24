"""Bounded offline PE32+ x64 dependency inventory; never loads an image.

Field layouts follow https://learn.microsoft.com/en-us/windows/win32/debug/pe-format.
This reads dependency names, not a complete Windows-loader validity proof.
"""
import re
import struct


class PeError(ValueError):
    pass


def require(condition, message):
    if not condition:
        raise PeError(message)


def dll_name(value):
    require(bool(re.fullmatch(r'[A-Za-z0-9_+.-]+\.dll', value, flags=re.IGNORECASE)), 'invalid dependency DLL name')
    require(not value.startswith('.') and '..' not in value, 'invalid dependency DLL name')
    return value.lower()


class Image:
    def __init__(self, data):
        require(isinstance(data, bytes) and 64 <= len(data) <= 256*1024*1024, 'PE byte budget or header differs')
        self.data = data
        require(data[:2] == b'MZ', 'missing DOS signature')
        pe = self.u32(60)
        require(pe >= 64 and self.take(pe, 4) == b'PE\0\0', 'missing PE signature')
        coff = pe+4
        require(self.u16(coff) == 0x8664, 'require x64 PE machine')
        count = self.u16(coff+2)
        require(1 <= count <= 96, 'invalid PE section count')
        optional_size = self.u16(coff+16)
        self.characteristics = self.u16(coff+18)
        require(self.characteristics & 2, 'PE image is not executable')
        optional = coff+20
        self.take(optional, optional_size)
        require(optional_size >= 112 and self.u16(optional) == 0x20b, 'require PE32+ optional header')
        self.image_base = self.u64(optional+24)
        self.headers_size = self.u32(optional+60)
        directories = self.u32(optional+108)
        require(directories <= 128 and 112+8*directories <= optional_size, 'PE data directory table is truncated')
        self.directories = [struct.unpack_from('<II', data, optional+112+8*i) for i in range(directories)]
        section_table = optional+optional_size
        require(section_table+count*40 <= self.headers_size <= len(data), 'PE headers exceed file bounds')
        self.sections = []
        for i in range(count):
            at = section_table+i*40
            virtual_size, address, raw_size, raw = struct.unpack('<IIII', self.take(at+8, 16))
            require(address+max(virtual_size, raw_size) <= 1 << 32, 'PE section RVA range overflows')
            if raw_size:
                require(raw >= self.headers_size, 'PE section overlaps headers')
                self.take(raw, raw_size)
            self.sections.append((address, max(virtual_size, raw_size), raw, raw_size))
        for i, (address, size, raw, raw_size) in enumerate(self.sections):
            for other_address, other_size, other_raw, other_raw_size in self.sections[:i]:
                require(not (size and other_size and max(address, other_address) < min(address+size, other_address+other_size)), 'overlapping PE virtual sections')
                require(not (raw_size and other_raw_size and max(raw, other_raw) < min(raw+raw_size, other_raw+other_raw_size)), 'overlapping PE raw sections')

    def take(self, offset, size):
        require(offset >= 0 and size >= 0 and offset+size <= len(self.data), 'PE field exceeds file bounds')
        return self.data[offset:offset+size]

    def u16(self, at):
        return struct.unpack('<H', self.take(at, 2))[0]

    def u32(self, at):
        return struct.unpack('<I', self.take(at, 4))[0]

    def u64(self, at):
        return struct.unpack('<Q', self.take(at, 8))[0]

    def offset(self, rva, size):
        require(0 < rva < 1 << 32 and size >= 0 and rva+size <= 1 << 32, 'invalid PE RVA range')
        if rva < self.headers_size:
            require(rva+size <= self.headers_size, 'PE RVA crosses header boundary')
            self.take(rva, size)
            return rva
        for address, virtual_size, raw, raw_size in self.sections:
            if address <= rva < address+virtual_size:
                delta = rva-address
                require(delta+size <= raw_size, 'PE RVA has no complete file-backed data')
                self.take(raw+delta, size)
                return raw+delta
        raise PeError('PE RVA is outside sections')

    def string(self, rva, end_rva=None):
        raw = bytearray()
        for i in range(512):
            require(end_rva is None or rva+i < end_rva, 'PE forwarder leaves export directory')
            byte = self.data[self.offset(rva+i, 1)]
            if not byte:
                require(bool(raw), 'empty PE dependency string')
                try:
                    return raw.decode('ascii')
                except UnicodeDecodeError as error:
                    raise PeError('non-ASCII PE dependency string') from error
            raw.append(byte)
        raise PeError('unterminated or oversized PE dependency string')

    def directory(self, index):
        rva, size = self.directories[index] if index < len(self.directories) else (0, 0)
        require(bool(rva) == bool(size) and rva+size <= 1 << 32, 'incomplete or overflowing PE data directory')
        return rva, size

    def imports(self, delay=False):
        rva, size = self.directory(13 if delay else 1)
        if not rva:
            return []
        stride = 32 if delay else 20
        require(size >= stride and size // stride <= 4096, 'invalid PE import directory size')
        result = set()
        for at in range(0, size-stride+1, stride):
            fields = struct.unpack('<'+'I'*(stride//4), self.take(self.offset(rva+at, stride), stride))
            if not any(fields):
                return sorted(result)
            if delay:
                require(fields[0] in (0, 1), 'unknown PE delay import attributes')
                name = fields[1] if fields[0] else fields[1]-self.image_base
            else:
                name = fields[3]
            result.add(dll_name(self.string(name)))
        raise PeError('PE import directory has no terminator')

    def forwarders(self):
        rva, size = self.directory(0)
        if not rva:
            return []
        require(size >= 40, 'truncated PE export directory')
        at = self.offset(rva, 40)
        count, table = self.u32(at+20), self.u32(at+28)
        require(count <= 1000000, 'PE export table exceeds count budget')
        result = set()
        if count:
            start = self.offset(table, count*4)
            for i in range(count):
                target = self.u32(start+i*4)
                if rva <= target < rva+size:
                    text = self.string(target, rva+size)
                    module, separator, symbol = text.rpartition('.')
                    require(bool(separator and module and symbol), 'invalid PE export forwarder')
                    require(bool(re.fullmatch(r'#[0-9]+|[A-Za-z_?@$][A-Za-z0-9_?@$]*', symbol)), 'invalid PE forwarded symbol')
                    result.add(dll_name(module if module.lower().endswith('.dll') else module+'.dll'))
        return sorted(result)


def inspect_pe(data):
    image = Image(data)
    return dict(format='PE32+', machine='x86_64', kind='dll' if image.characteristics & 0x2000 else 'exe',
                imports=image.imports(), delay_imports=image.imports(delay=True), export_forwarders=image.forwarders())
