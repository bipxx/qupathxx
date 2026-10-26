# Retained arm64 exception observation

Authenticated artifact12695357280 contains the original unchanged test program
(a55c203c...39b9a2). Native observer admission succeeds: two probe throws and two
matched catches, no observer errors, exit0. The real program exits1 both plainly
and under LLDB. Its invalid_argument with source-series message reaches the outer
main catch instead of the intended typed helper.

| Source specialization | Binary address | Same-address specialization | Catch-table type binding |
| --- | --- | --- | --- |
| invalid_argument / window lambda9 | 0x1000611f8 | length_error / lambda6 | 0x100688d90, length_error |
| invalid_argument / window lambda10 | 0x1000612ac | length_error / lambda7 | 0x100688d90, length_error |

The authenticated binary's actual invalid_argument RTTI slot is0x100688da8.
Raw table bytes, symbol aliases, unwind entries, fixup names and disassembly
are preserved under `offline/`; full tool dumps remain locally retained with
recorded hashes. The narrow decoder checks this specific table format rather
than providing a general Mach-O parser. Type-index and PC-relative-indirect
interpretation follows the [LLVM personality implementation](https://github.com/llvm/llvm-project/blob/main/libcxxabi/src/cxa_personality.cpp).

This explains the observed typed-catch mismatch at the binary level. It does
not by itself identify which compilation/link stage introduced it. Small witness254
uses the same objects for default and no-deduplicate links; both execute all
three expected catches and two negative controls successfully. This is a negative
reproduction, not proof of a workaround. The [LLVM Mach-O linker comparison](https://releases.llvm.org/16.0.0/tools/lld/docs/MachO/ld64-vs-lld.html#no-deduplicate-flag)
describes that option; no product flag has been changed.

[Original-object255](../raw-original-object-255/receipt.json) now isolates the
stage: original object helpers are distinct; default linking merges the differing
handlers, while no-deduplicate keeps them distinct with correct catch-table types.
It rebuilds no core/SDK. Unresolved core symbols are intentional in those analysis
products, which are never executed. Darwin policy87b9dac applies the scoped link
change. Complete repaired-product runtime, T5 and release acceptance stay open.
