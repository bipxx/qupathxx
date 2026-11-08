# RmlUi workspace shell — 2026-11-08

The default docked activity rail and slide toolbar now use RmlUi 6.3, packaged
RML/RCSS, an owned native snapshot and typed commands. Advanced ImGui inspectors,
menus/dialogs and the existing scientific renderer remain in this first slice.
[Architecture and reload guide](../../rmlui.md).

Source: `d58d194d` plus this change, native compiler-input SHA-256
`63d7ebd544903a94e10f4d68a9c41696f6d199733c59e5bc270ad8ca1e6949fb`.
[Artifact and resource identity](receipt.json). Dependencies are pinned to RmlUi
`ba95ffe8bfb6370efb2cdcca927eaad4710c5413` and FreeType
`534ad3456055ee1f65ecde3bcf22a656a31514d1`; fonts retain their authenticated sources.

## Decisive checks

- Four native CTest cases pass: `gui_shell`, `gui_replay`, `gui_design`, `gui_i18n`
  (1.76 seconds for the final combined run). The three source design contracts
  also pass through pytest.
- [Native GL shell test](../../../cpp/tests/gui_shell_tests.cpp) exercises all
  eighteen typed commands, disabled/modal/empty states, logical and framebuffer
  density, Tab/Shift+Tab/Enter/Space, all sixteen locale labels, invalid RML/RCSS
  retention and a successful layout-changing reload. Off-window pointer sentinels
  cannot enter an out-of-range integer conversion.
- Real docked input switches the activity pane and persists its stable ID. Native
  object count, project generation, exact saved project bytes, camera center/zoom
  and an exact scientific RGB sample remain unchanged. All thirteen visible
  toolbar icons have usable contrast.
- Retained CMU-1 project renders in dark English and light Simplified Chinese;
  annotation/image registration and control readability were inspected. Source
  project bytes remain unchanged. [Dark capture](../../images/rmlui-dark-en.png),
  [light Chinese capture](../../images/rmlui-light-zh-CN.png).
- The existing cache configures/builds with Python OFF and Python/nanobind
  discovery disabled. Native install includes the matching UI resources, fonts
  and licenses, with no Python module or dependency SDK headers. Installed docked
  and compact demo startup and CMU-1 startup exit 0 from `C:\Windows\System32`
  with PATH restricted to that directory.
- All 38 existing binding-object SHA-256 values remain identical. Optional Python
  cache settings are restored afterward; no bindings, wheel, release or broad
  scientific campaign was built.

The shared cache remains `build/ci/gridfrontends167-package` in the workspace
root. Candidate runtime, installed app, raw captures, logs, cache configuration
and binding hashes are retained under workspace-root `build/rmlui-shell`.
The installed app is `install-native/qupathxx/qupathxx_app.exe` there. Existing
dataset-demo windows and archived products were preserved.

## Corrections and scope

The initial link met a running demo's executable lock; runtime outputs now use a
separate candidate directory in the same cache. Windows source URLs use a virtual
RmlUi URL, and locale tests explicitly compile UTF-8. Full border boxes reserve
rail/toolbar space; standalone icon tessellation initializes its pixel density.
The font regression now checks valid proportional advances instead of the old
bitmap font's fixed seven pixels. Icon visibility uses actual theme contrast.
Unicode coverage, atlas/density limits, raw RGB and persisted identity assertions
remain intact.

This does not establish a performance advantage, browser deployment, complete
panel migration or new scientific qualification.
