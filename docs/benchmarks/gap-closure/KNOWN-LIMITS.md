# Candidate6 limits and remaining work

- General speed superiority is unproven. A matched ROI comparison shows native
  305.6ms /QuPath313.1ms with overlapping ranges and material host load.
- Coarse canonical whole-slide tissue is checked; level-zero whole-slide cells,
  dense-scene stress, long-duration leak guarantees and actual total GPU residency
  are not qualified.32 repeats and two240-frame viewer replays are bounded evidence.
- One new anisotropic NDPI crop matches558 cell partitions/RGB and independent
  compartment statistics. This is not independent-specimen/pathologist biological
  validation or context-halo generalization.
- Actual QuPath0.7.0 saved data from that crop contains a self-intersecting ring
  (`Ring Self-intersection[11509 18024]`) and is refused before publication. This
  remains an import compatibility gap. No silent repair/drop or invalid geometry
  admission was introduced. Supported synthetic saved-data geometry, hierarchy,
  explicit workflow-loss admission and unsupported-plane refusal pass installed
  checks. Broader file versions/builders remain outside declared support.
- Support is local Windows x64 CPython3.12 engineering install/run/removal, with
  Microsoft Visual C++ x64 runtime and OpenGL3.2. No fresh-host, signed installer,
  stable release, Linux/macOS or arbitrary vendor-format support is claimed.
- Native provenance retains its historical `a4990899+dirty` label. Exact568-input
  source tree2875ca45645052c72636340337212c0ebdd5dba44057cfd237cefc5be38a2f2e
  identifies these tested binaries; packaging does not rebuild or relabel them.
