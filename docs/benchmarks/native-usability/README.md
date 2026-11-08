# Native C++ usability follow-up

Owner approved a C++ open/analyze/save example, build/backend guide and one
Python-disabled CI guard. No scientific kernel, binding API or release changed.

- [Example](../../../examples/native_project.cpp), [guide](../../native-build.md)
  and [CI](../../../.github/workflows/native-only.yml).
- [Local tests](tests.log): example and application-entry checks pass. The example
  creates a two-pixel embedded RGB input without Python, opens/analyzes/retains/
  saves it, reopens and verifies native replay. Repeating the output request fails
  while both source and previous output SHA256 remain unchanged.
- [Installed consumer](installed-test.log): the same behavior passes from the
  installed NativeExamples component in the existing development stage.
- [Receipt](receipt.json): source identity, no binding targets with discovery
  disabled, all38 binding objects preserved. The existing Windows GUI/SDK flags
  were retained; this is not a locally executed minimal Linux configuration.
  Python-enabled cache settings were restored without rebuilding the bindings.
- The CI job uses the default OFF setting, disables discovery, builds only four
  small consumer/test targets and their native dependencies, checks CTest and
  the installed consumer, and rejects Python products/imports or unresolved
  Linux dependencies. No Python setup, scientific downloads or full matrix.
  Its shell blocks pass bash syntax checks. [Initial hosted run](https://github.com/bipxx/qupathxx/actions/runs/42513372855)
  passed configure but exposed ambiguous checkpoint lookup under GCC in the
  existing cell-statistics helper ([diagnostics](initial-gcc-failure.log)). Four
  calls now explicitly name the intended intensity_detail helper; algorithm and
  cancellation semantics are unchanged. [Affected local tests](portability-tests.log)
  pass; a changed-source hosted run is required before claiming Linux completion.

Waste review: one existing cache, one optional example component install, two
focused native checks and one installed check. Reused existing science evidence;
no new binding compilation, wheel, release or secondary build cache. The optional
example source is included in the native source manifest so its identity is not
silently omitted. Previously frozen evidence retains its original source scope.
