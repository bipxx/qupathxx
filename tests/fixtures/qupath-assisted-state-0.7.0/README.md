# Derived assisted-state exchange fixtures

These six bundles were executed with the pinned QuPath 0.7.0 runtime and the
separate exchange/2 producer. `manifest.json` binds exact producer/output bytes,
parent manifests, commands, captured input hashes and the synthetic mixed-header
variant. The parent's native/TIFF bytes are reused in tests, avoiding copies.
Absolute capture paths document provenance; tests relocate source URIs and rebind
only the resulting project hash. The mixed second entry changes the ASCII
producer version to 0.6.0 to exercise GeoJSON compatibility; it is not a capture
from a QuPath 0.6.0 runtime. Parent independent state references remain authoritative.
