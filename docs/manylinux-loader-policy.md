# x86-64 GLIBC loader qualification

Portable run 39995913896 at `1ff10524cc14665ffe2bee65310db742a6359205`
builds the CPython 3.11 wheel and reaches ABI qualification. Its qualification
report rejects five ELF payloads solely for the dependency
`ld-linux-x86-64.so.2`. It does not produce a qualified wheel or consumer pass.
The GEOS lib64 layout check now passes in this run.

[auditwheel 6.6.0's policy implementation](https://github.com/pypa/auditwheel/blob/6.6.0/src/auditwheel/policy/__init__.py)
excludes the ELF loader from dependencies to vendor. The project scanner already
limits payloads to little-endian 64-bit EM_X86_64 ELF. Its dependency check now
accepts the exact x86-64 GLIBC loader name as a system runtime and continues to
check every recorded version requirement against the requested policy. It does
not exempt arbitrary `ld-linux` names, other architectures, MUSL or private/new
GLIBC symbol requirements. auditwheel show/repair and fresh consumer validation
remain mandatory after this preliminary check.

Twelve focused tests pass on Windows, including accepted GLIBC_2.2.5 loader
requirements and rejection of GLIBC_2.29, GLIBC_PRIVATE and three unrelated loader
names. Replaying the actual failed report against the pinned auditwheel policy
resolves all five loader failures without any other dependency/version errors.
This replay checks the recorded requirements; it is not a new scan of the wheel
or a portable installation result. A new hosted build and both fresh consumers
must still complete before accepting portable distribution.
