# Navigation qualification of the sampled-detector source

The original [navigation protocol](navigation-performance-protocol.md) and its
failed/ineligible source-2afd observations remain unchanged. A new source
contract, `tests/fixtures/navigation-sampled-source-139.json`, preregisters the
audited main source `ddd3bb981ee9ef1c1ff396fa47c6239dd1dfa97c` for a separate run.
Only the engine and its source-pinned auditor identity change. Hardware,
renderer, slide bytes, benchmark script, trial/view count, workload, CPU-load
ceilings and all acceptance thresholds retain their frozen values.

The qualifier checks the immutable Git source of the registered auditor. It
also checks the source bytes of the original snapshot, hardware eligibility
and view-inspection functions, preventing a source substitution from silently
changing the acceptance rules. Changed-workload, changed-criterion and
wrong-engine controls fail before any viewer trial.

Install the exact audited Windows GUI CI wheel in a fresh environment without
software OpenGL DLLs, then run:

```text
python -I tools/qualify_navigation.py --engine-audit AUDIT.json --image CMU-1.ndpi --source-contract tests/fixtures/navigation-sampled-source-139.json --output-directory NEW_DIRECTORY
```

Use the full Windows/Linux audit for that exact source. The installed native
hash must match its GUI artifact, and the actual renderer must identify the
declared RTX 4090. Run all three independent processes and retain all six views,
including ineligible or slower trials. Do not run another owned GUI, capture,
build or benchmark concurrently. The optional affinity settings used by the
separate cell-workflow profiler do not apply to this qualification.

This can qualify only the named navigation workload on the registered source.
It cannot qualify the later expansion/domain-fix candidate, other research
workloads, general performance, biological accuracy or a supported release.
