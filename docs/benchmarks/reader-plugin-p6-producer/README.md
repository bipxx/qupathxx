Exact P6 plugin producer sources
===============================

This capsule preserves the actual source/header/profile bytes used by the independent Windows x64 Release /MD P6 DLL identified in PRODUCER.json. The DLL is packaged separately under its exact reviewed hash. No application core library was linked. The base ABI v1 table has 72 bytes; the current host optional file-set extension is separate.

With the recorded matching MSVC compiler, configure kit/share/qupathxx/reader-plugin/example with -DQUPATHXX_READER_PLUGIN_KIT_PREFIX=<absolute capsule>/kit and build Release. The original generated profile header is retained for comparison. source-inputs.sha256 binds every preserved compiler/build recipe input; LICENSE is in the kit. This is corresponding source, not a compiler distribution or bit-for-bit rebuild guarantee.
