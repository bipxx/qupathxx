# Independently built P6 display reader

This example builds `qpx_p6_plugin.dll` using only the installed C ABI header and
the installed dependency-free P6 decoder source. It neither discovers nor links
the qupathxx core library. Copy this directory outside the application checkout
to build against a relocated kit.

The supported pilot profile is Windows x64 with the MSVC series and compiler
version recorded in `profile.cmake`, Release, and the dynamic CRT (`/MD`). Use a
matching Visual Studio developer shell. CMake and a compile-time guard reject
other compiler, architecture, configuration, and CRT profiles.

The installed kit prefix contains:

```text
include/qupathxx/reader_plugin.h
share/qupathxx/reader-plugin/profile.cmake
share/qupathxx/reader-plugin/decoder/ppm_decoder.hpp
share/qupathxx/reader-plugin/decoder/ppm_decoder.cpp
share/qupathxx/reader-plugin/example/...
```

For example, in PowerShell with the recorded compiler active:

```powershell
$kit = 'C:/qupathxx-installed'
$source = 'C:/p6-plugin-source'
$build = 'C:/p6-plugin-build'
$module = 'C:/p6-plugin-installed'
Copy-Item -LiteralPath "$kit/share/qupathxx/reader-plugin/example" -Destination $source -Recurse
cmake -S $source -B $build -G Ninja -DCMAKE_BUILD_TYPE=Release "-DQUPATHXX_READER_PLUGIN_KIT_PREFIX=$kit"
cmake --build $build --config Release
cmake --install $build --config Release --prefix $module
```

The build writes `reader-plugins.json` beside the DLL, recording its SHA-256 and
the exact provider descriptor. Installation copies both files into `$module`.
Keep them together. Rebuilding the DLL regenerates its manifest; a stale hash
causes the host to reject the module. The pilot has no extra module dependencies;
its MSVC runtime is supplied by the matching installed host/runtime profile.

Use an absolute manifest path with the installed region CLI:

```powershell
$region = "$kit/bin/qupathxx_region.exe"
& $region --reader-plugins "$module/reader-plugins.json" --readers
& $region --reader-plugins "$module/reader-plugins.json" --reader org.qupathxx.p6 'C:/images/input.ppm' 0 0 0 64 64 'C:/images/region.pam'
```

The explicit provider is `org.qupathxx.p6`, display name `P6 display example`,
version `1.0.0`, contract `p6-max255-display/1`. It accepts binary P6 with maximum
255, using the same parser as built-in PPM. It exposes one level of straight-alpha
RGBA8 display pixels; pixels outside the image are transparent black. It supplies
no quantitative samples, calibration, channel names, or view mutations. Open
options must be empty. Projects cannot attach or save plugin sources in this
standalone pilot.

The host supplies a 256 MiB opening work budget. Before invoking the unchanged
parser, the module conservatively refuses an encoded file larger than that
budget, including a small raster followed by excessive trailing data. It also
checks the decoded byte count and cancellation before and after synchronous
decoding. The built-in PPM parser keeps its existing admission behavior. Display
outputs separately fit the host's 256 MiB region limit.

All ABI entry points catch exceptions. Text and pixel results are copied into
host-owned buffers, and only an opaque module-owned reader handle survives an
open call. The module retains no host buffers or cancellation callbacks. The host
serializes calls and destroys handles before shutdown; loaded modules remain
loaded for the lifetime of the process.

These sources are covered by the repository's AGPL-3.0-only license. This example
demonstrates the selected P6 display contract; it does not add format admission or
quantitative authority.
