# COLMAP WebView design-system validation

This is a Windows GUI-only port of the reference at
`121bd73ae77686e42093918944cec1baa1164aa5`, based on local main605bba55.
The [design guide](../../design-system.md) describes the shared owners and shows
dark/light/Chinese screens over the retained CMU-1 research ROI.

`pytest-final.log` records90 passing GUI/source-contract cases, including exact
synthetic source pixels, camera geometry, unchanged stored project bytes,
theme/reset preferences, localized save at200%, both live language chooser paths,
docking, thumbnail completeness and compact Spatial action routing.
Five unchanged installed-subprocess font/launcher cases are deselected; the new
IBM font bytes, hashes and license are included in the passing contracts.
`native-tests.log` covers design tokens, explicit/system theme resolution,
text/action contrast, target sizes and131 translations in16 languages.

Only gui.cpp and gui_fonts.cpp were compiled. Retained native libraries and38
binding objects were reused for a separate executable and development Python
runtime. This does not qualify a new scientific engine, wheel or public release.
`build.log` retains the pre-existing LNK4098 default-library warning.
`retained-native-inputs.json` records verified library snapshots used for linking.
`source-inputs.json` records raw Windows inputs and canonical LF hashes; ordinary
Git EOL normalization does not change the compiled C++ meaning. `receipt.json`
binds this evidence to the resulting binaries, generated manifest and screenshots.

Earlier failed attempts remain in the other pytest logs. The old fixed overview
width assumed bitmap-font padding; the final assertion retains exact native
thumbnail data and checks complete visible area/aspect after excluding gray text
pixels. Russian200% controls required compact utility icons. Language tests use
the native chooser shortcut instead of font-dependent menu coordinates.
Native desktop inspection was stopped by physical Escape during the final menu
check; no further desktop input was issued.

The retained local preview, input snapshots, failed outputs, recovery files,
scripts and private tools are under main `build/design-system-preview`.
`build_preview.py` and `capture_preview.py` are development receipts with local
paths; they are not runtime dependencies or a replacement build system.
The usual CMake GUI build packages the authenticated IBM fonts and complete OFL.
The separate scientific integration checkout and published products are untouched.
