"""Change only synthetic stream bytes, preserving Windows version tokens."""
import os


def mutate_header(path, offset):
    original = path.read_bytes()
    before = path.stat()
    if os.name == 'nt':
        import ctypes
        from ctypes import wintypes
        import msvcrt

        class Basic(ctypes.Structure):
            _fields_ = [(name, ctypes.c_longlong) for name in
                        ('created', 'accessed', 'modified', 'changed')] + [('attributes', wintypes.DWORD)]

        kernel = ctypes.WinDLL('kernel32', use_last_error=True)
        get = kernel.GetFileInformationByHandleEx
        get.argtypes = [wintypes.HANDLE, ctypes.c_int, ctypes.c_void_p, wintypes.DWORD]
        get.restype = wintypes.BOOL
        put = kernel.SetFileInformationByHandle
        put.argtypes = get.argtypes
        put.restype = wintypes.BOOL
        create = kernel.CreateFileW
        create.argtypes = [wintypes.LPCWSTR, wintypes.DWORD, wintypes.DWORD,
                           ctypes.c_void_p, wintypes.DWORD, wintypes.DWORD, wintypes.HANDLE]
        create.restype = wintypes.HANDLE
        close = kernel.CloseHandle
        close.argtypes = [wintypes.HANDLE]
        close.restype = wintypes.BOOL

        def basic(handle):
            result = Basic()
            if not get(handle, 0, ctypes.byref(result), ctypes.sizeof(result)):
                raise ctypes.WinError(ctypes.get_last_error())
            return result

        with path.open('r+b') as writer:
            saved = basic(wintypes.HANDLE(msvcrt.get_osfhandle(writer.fileno())))
            writer.seek(offset)
            writer.write(bytes([original[offset] ^ 64]))
        # An open data-writer is correctly refused by ReaderFileGuard. Close it
        # first, then restore stamps using only an attributes handle. This makes
        # the witness independent of filesystem timestamp resolution/coalescing.
        handle = create(str(path), 0x180, 7, None, 3, 0, None)
        if handle == wintypes.HANDLE(-1).value:
            raise ctypes.WinError(ctypes.get_last_error())
        try:
            if not put(handle, 0, ctypes.byref(saved), ctypes.sizeof(saved)):
                raise ctypes.WinError(ctypes.get_last_error())
        finally:
            assert close(handle)
        handle = create(str(path), 0x80, 7, None, 3, 0, None)
        if handle == wintypes.HANDLE(-1).value:
            raise ctypes.WinError(ctypes.get_last_error())
        try:
            restored = basic(handle)
            assert (restored.modified, restored.changed) == (saved.modified, saved.changed)
        finally:
            assert close(handle)
    else:
        with path.open('r+b') as writer:
            writer.seek(offset)
            writer.write(bytes([original[offset] ^ 64]))
        # POSIX ctime cannot generally be restored; retain the same mtime while
        # still exercising every public refusal boundary on these platforms.
        os.utime(path, ns=(before.st_atime_ns, before.st_mtime_ns))
    after = path.stat()
    assert (before.st_dev, before.st_ino, before.st_size, before.st_mtime_ns) == (
        after.st_dev, after.st_ino, after.st_size, after.st_mtime_ns)
    changed = path.read_bytes()
    assert len(changed) == len(original)
    assert [i for i, pair in enumerate(zip(original, changed)) if pair[0] != pair[1]] == [offset]
