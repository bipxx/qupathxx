# Offline ELF dynamic inventory

`tools/ci/elf_dependencies.py` reads bounded ELF64 little-endian x86-64 dynamic
images without extracting or loading them. It uses program headers and virtual
to file mappings, including for sectionless images. Header/program/dynamic
layouts follow the [generic ELF ABI](https://gabi.xinuos.com/elf/07-pheader.html);
GNU tag and version-record layouts follow
[glibc's ELF definitions](https://github.com/bminor/glibc/blob/master/elf/elf.h).

The `inspect_elf(bytes)` result identifies the image format and machine,
executable/shared identity, interpreter, ordered `DT_NEEDED` names, SONAME,
RPATH/RUNPATH components, auxiliary/filter names, audit declarations and GNU
version requirements. Version records retain dependency filename, version name,
hash, flags and index. Names preserve case, order, repeated references and path
syntax. Empty search components remain explicit. This reader inventories those
declarations; a package resolver must apply search-path and prerequisite policy.

The supported input is a dynamic ET_EXEC/ET_DYN image with the System V or GNU
OS ABI and ABI version zero. Other architectures, byte orders, static images,
extended program-header counts and non-ASCII names are rejected explicitly.
This does not establish symbol resolution, exported-version availability,
relocation correctness, arbitrary ELF validity or loader behavior.

Inputs are at most 256 MiB. Program headers are capped at 1,024; dynamic entries
at 65,536; string tables at 8 MiB; each name at 32 KiB; aggregate referenced names
at 4 MiB, including repeated references. Version dependency headers are capped
at 4,096, auxiliaries per header at 4,096 and total auxiliaries at 65,536.
Malformed, overflowing, overlapping, unterminated, ambiguous and non-file-backed
ranges fail. Load-address lookup uses a sorted index; range validation avoids
copying whole load segments. Version chain counts and terminators must agree,
and version headers/auxiliaries must not overlap.

Local validation passes 75 ELF cases and all 55 inherited Windows manifest
cases. An isolated `pyelftools==0.32` oracle independently agrees on all 18 native
images across three accepted Linux wheels: the ordinary Linux CPython 3.12
wheel and manylinux CPython 3.11/3.12 wheels. The comparison includes dependency
order, SONAME, search/interpreter fields and every required-version record.
Neither implementation loads those binaries. The
[local receipt](benchmarks/elf-runtime-reader-local-windows.json) retains file
hashes, parser identity and the complete observations. Input packages come from
accepted cohort source `15ee0e8`; this is not a new engine compilation.

Candidate 147 supplies the binary reader. Complete Linux archive file manifests,
bundled/external resolution, atomic publication/verification, source-bound CI
gates and new hosted validation are still required. Mach-O inventories, dynamic
loading, installed prerequisites, signing, licensing and supported release also
remain open. The full M0-M7 project remains partial.
