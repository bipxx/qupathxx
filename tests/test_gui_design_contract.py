"""Source/asset contracts for the native port of COLMAP WebView's system."""
import hashlib
import json
from pathlib import Path
import re
from gui_command_inventory import code_mask
from gui_contract_calls import has_call, call_position
from gui_raw_imgui_inventory import inventory as raw_inventory, counts, check_ratchet

ROOT = Path(__file__).resolve().parents[1]


# Palette values are compared with the fixture by native gui_design_tests.


def test_pinned_sans_resources_and_license_are_shipped():
    source = (ROOT/'cmake/GuiFonts.cmake').read_text(encoding='utf8')
    provenance = json.loads((ROOT/'assets/fonts/font-source.json').read_text(encoding='utf8'))
    for prefix in ['SANS', 'SEMIBOLD']:
        name = re.search(r'set\(QPX_GUI_'+prefix+r'_NAME "([^"]+)"\)', source).group(1)
        size = int(re.search(r'set\(QPX_GUI_'+prefix+r'_BYTES (\d+)\)', source).group(1))
        digest = re.search(r'set\(QPX_GUI_'+prefix+r'_SHA256 "([^"]+)"\)', source).group(1)
        data = (ROOT/'assets/fonts'/name).read_bytes()
        assert len(data) == size
        assert hashlib.sha256(data).hexdigest() == digest == provenance['outputs'][name]['sha256']
        assert data[:4] == b'\x00\x01\x00\x00', 'Expected native SFNT/TTF'
    assert 'SIL OPEN FONT LICENSE Version 1.1' in (ROOT/'assets/fonts/IBMPlexSans-OFL.txt').read_text(encoding='utf8')


def test_reference_geometry_and_sans_roles_are_used_by_shared_components():
    # Constant geometry is enforced by static_assert in gui_design_tests.cpp.
    fonts = (ROOT/'cpp/gui_fonts.cpp').read_text(encoding='utf8')
    loaders=[name for name in set(re.findall(r'\b(\w+)\s*\(',code_mask(fonts)))
             if has_call(fonts,name,'assets.sans') and has_call(fonts,name,'assets.semibold')]
    assert len(loaders)==1
    assert call_position(fonts,loaders[0],'assets.sans') < call_position(fonts,loaders[0],'assets.semibold')
    assert 'AddFontDefaultBitmap' not in fonts
    assert 'assets.cjk' in fonts and 'assets.emoji' in fonts


def test_workbench_styles_share_tokens_and_command_components():
    css=(ROOT/'assets/ui/workspace.rcss').read_text(encoding='utf8')
    assert '${activity-width}' in css and '${icon-size}' in css and '${dark.foreground}' in css
    assert not re.search(r'#[a-fA-F0-9]{6}\b',css), 'Shared chrome colors belong in the native token owner'
    shell=(ROOT/'cpp/gui_shell.cpp').read_text(encoding='utf8')
    assert 'design::resolve_stylesheet' in shell
    check_ratchet(raw_inventory(ROOT/'cpp'), json.loads(
        (ROOT/'tests/fixtures/gui-raw-imgui-allowlist.json').read_text(encoding='utf8')))


# ComboScope's noncopyable lifetime now enforces adapter pairing in the compiler;
# native combo_scope_contracts checks both open/closed stack restoration.
def test_raw_call_parser_handles_real_bypasses():
    assert counts('ImGui::Button \n(variable); ImGui::TextWrapped\n ("%s",value);') == {'Button':1,'TextWrapped':1}
    assert counts('// ImGui::Button(x)\nconst char* s="ImGui::Text(x)";') == {}
    assert counts('ImGui::Button(/* (not an argument) */ label);') == {'Button':1}
    assert counts('ImGui /* comment */ :: Button \n (label);') == {'Button':1}

def test_path_field_members_are_registered_with_run_gui():
    """Discover owners, then follow paths() aggregation to the central registry."""
    import re
    from pathlib import Path
    cpp = Path(__file__).resolve().parents[1] / "cpp"
    gui = (cpp / "gui.cpp").read_text(encoding="utf8")
    owners = {}
    for path in cpp.glob("gui*.*"):
        if path.suffix not in {".hpp", ".cpp"}:
            continue
        source = path.read_text(encoding="utf8")
        declarations_of_fields = list(re.finditer(r"ui::PathField\s+(\w+)\s*(?:\{|;|=)", source))
        members = [match[1] for match in declarations_of_fields]
        if not members:
            continue
        declarations = list(re.finditer(r"(?:class|struct)\s+(Gui\w+)(?:::\s*Impl)?\s*\{", source[:declarations_of_fields[0].start()]))
        owner = declarations[-1] if declarations else None
        assert owner, path
        kind = owner[1]
        listing = re.search(r"\bpaths\(\)\s*\{", source)
        body = ""
        if listing:
            depth = 1
            end = listing.end()
            while depth and end < len(source):
                depth += (source[end] == "{") - (source[end] == "}")
                end += 1
            body = source[listing.end():end - 1]
        # Every member must occur in an explicit address in its paths() body.
        assert body, path
        for member in members:
            assert re.search(r"&(?:impl_->)?" + member + r"\b", body), (path, member)
        owners[kind] = source
    assert owners
    registered = set()
    for kind, source in owners.items():
        match = re.search(r"auto\s+(\w+)_owner=std::make_unique<detail::" + kind + r">", gui)
        if match:
            alias = re.search(r"auto& (\w+)=\*" + match[1] + r"_owner", gui)
            assert alias, kind
            if re.search(r"(?:all_paths=|field:)" + alias[1] + r"\.paths\(\)", gui):
                registered.add(kind)
    # Composition is explicit: the parent must publish each child's list.
    changed = True
    while changed:
        changed = False
        for parent in list(registered):
            source = owners[parent]
            for child in owners.keys() - registered:
                member = re.search(r"\b" + child + r"\s+(\w+)", source)
                if member and member[1] + ".paths()" in source:
                    registered.add(child)
                    changed = True
    assert registered == owners.keys(), owners.keys() - registered


def test_tool_paths_never_regress_to_plain_inputs():
    import re
    from pathlib import Path
    cpp = Path(__file__).resolve().parents[1] / "cpp"
    labels = {
        "##QPTMA input", "##QPTMA output", "##TMA measurements path", "##TMA metadata path",
        "Training checkpoint", "Resume training from", "Model path", "Prediction file",
        "Resume prediction from", "##output", "Raw model", "##density-previous", "##density-output",
        "Pixel model file", "Pixel stream file", "Saved stream file", "##raw NumPy path",
        "##raw CSV path", "CSV path", "Model file", "Tiled results folder",
        "Dark TIFF path", "Flat TIFF path", "Mixing matrix JSON path", "Rule JSON path",
    }
    for path in cpp.glob("gui*.*"):
        if path.suffix not in {".hpp", ".cpp"}:
            continue
        source = path.read_text(encoding="utf8")
        inputs = set(re.findall(r'(?:InputText(?:WithHint)?|edit|edit_text)\("([^"]+)"', source))
        assert not labels & inputs, (path, labels & inputs)


def test_tool_path_roles_extensions_and_lazy_picker():
    import re
    from pathlib import Path
    cpp = Path(__file__).resolve().parents[1] / "cpp"
    expected = [
        ('gui_tma.hpp', '##TMA metadata path', 'NewFile', ['.csv']),
        ('gui_tma.hpp', '##TMA measurements path', 'OpenFile', ['.csv']),
        ('gui_tma.hpp', '##QPTMA input', 'OpenFile', ['.qptma']),
        ('gui_tma.hpp', '##QPTMA output', 'NewFile', ['.qptma']),
        ('gui_raw.hpp', '##raw NumPy path', 'NewFile', ['.npy']),
        ('gui_raw.hpp', '##raw CSV path', 'NewFile', ['.csv']),
        ('gui_spatial.hpp', 'CSV path', 'NewFile', ['.csv']),
        ('gui_batch.hpp', '##output', 'NewFolder', []),
        ('gui_batch.hpp', 'Raw model', 'OpenFile', ['.qrm']),
        ('gui_project_density.hpp', '##density-output', 'NewFolder', []),
        ('gui_project_density.hpp', '##density-previous', 'OpenFolder', []),
        ('gui_object_classifier.hpp', 'Model file', 'OpenFile', ['.qpxforest']),
        ('gui_object_classifier.hpp', 'Export model file', 'NewFile', ['.qpxforest']),
        ('gui_pixel_classifier.hpp', 'Pixel model file', 'OpenFile', ['.qpc']),
        ('gui_pixel_classifier.hpp', 'Export pixel model file', 'NewFile', ['.qpc']),
        ('gui_pixel_classifier.hpp', 'Pixel stream file', 'NewFile', ['.qps']),
        ('gui_pixel_stream_browser.hpp', 'Saved stream file', 'OpenFile', ['.qps']),
        ('gui_raw_pixel.hpp', 'Model path', 'OpenFile', ['.qrm']),
        ('gui_raw_pixel.hpp', 'Export model path', 'NewFile', ['.qrm']),
        ('gui_raw_pixel.hpp', 'Prediction file', 'NewFile', ['.qws']),
        ('gui_raw_pixel.hpp', 'Open prediction file', 'OpenFile', ['.qws']),
        ('gui_raw_pixel.hpp', 'Training checkpoint', 'NewFile', ['.qtc']),
        ('gui_raw_pixel.hpp', 'Resume training from', 'OpenFile', ['.qtc']),
        ('gui_raw_pixel.hpp', 'Resume prediction from', 'OpenFile', ['.qws']),
        ('gui_fluorescence.cpp', 'Tiled results folder', 'NewFolder', []),
        ('gui_dark_flat.cpp', 'Dark TIFF path', 'OpenFile', ['.tif', '.tiff']),
        ('gui_dark_flat.cpp', 'Flat TIFF path', 'OpenFile', ['.tif', '.tiff']),
        ('gui_linear_unmixing.cpp', 'Mixing matrix JSON path', 'OpenFile', ['.json']),
        ('gui_marker_rules.cpp', 'Rule JSON path', 'OpenFile', ['.json']),
    ]
    for filename, label, role, extensions in expected:
        source = (cpp / filename).read_text(encoding="utf8")
        found = re.search(r'ui::PathField\s+\w+\{\{"' + re.escape(label) + r'",ui::PathRole::(\w+),\{([^}]*)\}', source)
        assert found and found[1] == role, (filename, label, role)
        assert re.findall(r'"([^"]+)"', found[2]) == extensions, (filename, label)
    component = (cpp / "gui_path_field.hpp").read_text(encoding="utf8")
    assert "std::unique_ptr<detail::GuiImportPicker> picker_;" in component
    assert "if(!picker_)picker_=std::make_unique<detail::GuiImportPicker>();" in component
    assert "if(!picker_ || !picker_active_" in component
    assert "return picker_ && picker_active_" in component
    assert "return picker_ && std::exchange(picked_,false)" in component
    assert "pending_edit_=true" in component and "std::exchange(pending_edit_,false)" in component
    for name in ["genes", "landmark_registration", "fluorescence"]:
        gui = (cpp / "gui.cpp").read_text(encoding="utf8")
        assert "auto& " + name + "=*" + name + "_owner" in gui
    raw = (cpp / "gui_raw.hpp").read_text(encoding="utf8")
    job = raw[raw.index("void data_job("):raw.index("void verify_record(")]
    assert "PathContext" not in job and "sync(context)" not in job
    draw = raw[raw.index("    Change draw("):]
    assert "for(auto* field:paths())field->sync(context);" not in draw
    pixel = (cpp / "gui_raw_pixel.hpp").read_text(encoding="utf8")
    assert "resume_training_.state().resolved==checkpoint_path_.state().resolved" in pixel
    assert "resume_stream_.state().resolved==stream_path_.state().resolved" in pixel


def test_bulk_path_fixtures_are_owned_and_retired():
    from pathlib import Path
    cpp = Path(__file__).resolve().parents[1] / "cpp"
    main = (cpp / "tests/gui_organization_tests.cpp").read_text(encoding="utf8")
    assert 'headless(dir,argc<=2 || (argc>2 && std::string(argv[2])=="--path-field"))' in main
    source = (cpp / "tests/gui_path_field_checks.hpp").read_text(encoding="utf8")
    assert source.count("if(bulk) {") == 2
    assert 'for(int i=0;i<1000;++i)' in source and 'for(int i=0;i<16385;++i)' in source
    assert '"page one must scan past 1000 nonmatching files"' in source
    assert '"scan cap preserves continuation"' in source
    for name in ["sparse", "capped"]:
        assert "fs::remove_all(" + name + ")" in source
        assert name + ".parent_path()==root" in source


def test_every_path_member_has_one_draw_site():
    import re
    from pathlib import Path
    cpp = Path(__file__).resolve().parents[1] / "cpp"
    gui = (cpp / "gui.cpp").read_text(encoding="utf8")
    gui += (cpp / "gui_analysis_tasks.hpp").read_text(encoding="utf8")
    for path in cpp.glob("gui*.*"):
        if path.suffix not in {".hpp", ".cpp"}:
            continue
        source = path.read_text(encoding="utf8")
        members = re.findall(r"ui::PathField\s+(\w+)\s*(?:\{|;|=)", source)
        for member in members:
            draws = len(re.findall(r"\b" + member + r"\.draw\s*\(", source))
            if path.name == "gui_core_paths.hpp":
                if member in {"cells", "roi", "tissue"}:
                    assert len(re.findall(r"paths\.output\([^;\n]+\)\.draw\(", gui)) == 1
                    continue
                draws += len(re.findall(r"\bpaths\." + member + r"\.draw\s*\(", gui))
            if path.name == "gui_genes.hpp" and member in {"summary_csv_", "membership_csv_"}:
                # Persistent picker owners pass their fields to the plane-local
                # result view. Count that one explicit draw through its alias.
                assert re.search(r"aggregation\.draw\([^;]+gate,summary_csv_,membership_csv_,path_context\)", source)
                view = (cpp / "gui_expression_aggregation.hpp").read_text(encoding="utf8")
                alias = {"summary_csv_": "csv", "membership_csv_": "memberships"}[member]
                assert f"ui::PathField& {alias}" in view
                draws += len(re.findall(r"\b" + alias + r"\.draw\s*\(", view))
            if path.name == "gui_genes.hpp" and member in {"fit_archive_", "embedding_csv_", "loadings_csv_", "scatter_svg_", "differential_csv_", "replicates_csv_"}:
                if member in {"differential_csv_", "replicates_csv_"}:
                    assert re.search(r"differential\.draw\([^;]+gate,differential_csv_,replicates_csv_,path_context\)", source)
                    view = (cpp / "gui_expression_differential.hpp").read_text(encoding="utf8")
                    alias = {"differential_csv_": "genes", "replicates_csv_": "replicates"}[member]
                    assert f"ui::PathField& {alias}" in view
                    draws += len(re.findall(r"\b" + alias + r"\.draw\s*\(", view))
                else:
                    assert re.search(r"reduction\.draw\([^;]+gate,fit_archive_,embedding_csv_,loadings_csv_,scatter_svg_,path_context\)", source)
                    view = (cpp / "gui_expression_reduction.hpp").read_text(encoding="utf8")
                    alias = {"fit_archive_": "archive", "embedding_csv_": "embedding", "loadings_csv_": "loadings", "scatter_svg_": "plot"}[member]
                    assert f"ui::PathField& {alias}" in view
                    if alias == "archive":
                        draws += len(re.findall(r"\barchive\.draw\s*\(", view))
                    else:
                        assert len(re.findall(r"\bfield\.draw\s*\(", view)) == 1
                        draws += len(re.findall(r"\boutput\(Action::\w+," + alias + r",", view))
            assert draws == 1, (path, member, draws)


def test_shared_analysis_input_consumers_and_edit_routes():
    cpp = ROOT / "cpp"
    inputs = (cpp / "gui_analysis_inputs.hpp").read_text(encoding="utf8")
    gui = (cpp / "gui.cpp").read_text(encoding="utf8")
    tasks = (cpp / "gui_analysis_tasks.hpp").read_text(encoding="utf8")
    assert 'Kind=1,Vectors=2,White=4,Nucleus=8,IntensityStatistics=16,RoiSampling=32' in inputs
    assert 'AnalysisInputs inputs(options)' in gui
    assert 'void edited(unsigned fields)' in inputs and 'if(!fields)return;' in inputs
    assert 'if(!(fields&detail::analysis_input_consumers[i].fields))continue;' in gui
    assert 'input_feedback.record(consumer,generation)' in gui
    assert 'inputs.use_stains(loaded->stains(),loaded->white())' in gui
    assert 'inputs.use_stains(settings->basis(),settings->white())' in gui
    assert 'inputs.use_stains(estimate->stains,estimate->white)' in tasks
    assert 'if(analysis_changed)' not in tasks
    assert 'draw_analysis_inputs_editor(inputs,' in tasks and 'detail::draw_analysis_inputs_editor(inputs,' in gui
    assert 'open=!open;' in gui
    assert 'reveal_analysis_inputs' not in gui and 'reveal_analysis_inputs' not in tasks
    assert 'input_feedback.observe(' in gui
    catalogue = (cpp / 'gui_i18n_catalog.inc').read_text(encoding='utf8')
    keys = re.findall(r'"(Cancelled: [^"\n]+\.)"', inputs)
    assert len(keys) == 5
    for key in keys:
        row = re.search(r'^\{"' + re.escape(key) + r'", \{(.*)\}\},$', catalogue, re.M)
        assert row, key
        values = json.loads('[' + row[1] + ']')
        assert len(values) == 15 and all(values), key


def test_analysis_presenter_keeps_native_owners_and_safe_statuses():
    cpp = Path(__file__).resolve().parents[1] / "cpp"
    gui = (cpp / "gui.cpp").read_text(encoding="utf8")
    tasks = (cpp / "gui_analysis_tasks.hpp").read_text(encoding="utf8")
    panel = (cpp / "gui_result_panel.hpp").read_text(encoding="utf8")
    assert gui.count("detail::analysis_tasks::draw(task_context)") == 1
    assert "AnalysisJob analysis;" in gui and "TissueJob tissue;" in gui and "CellJob cell_job;" in gui
    for function in ["draw_run_status", "draw_keep_row", "draw_kept_block"]:
        assert tasks.count("ui::" + function + "(") == 3
    assert "caption(parse_run_state(model.raw_run),model.raw_run)" in panel
    assert "caption(parse_record_state(model.kept->raw),model.kept->raw)" in panel
    model = (cpp / "gui_stain_model.hpp").read_text(encoding="utf8")
    assert 'ui::TextWrapped("Model file: %s",stain_model_io.caption().c_str())' in tasks
    assert "caption(ui::parse_export_state(raw),raw)" in model and "caption(ui::parse_run_state(raw),raw)" in model
    assert "operation_==Operation::Save?" in model
    assert panel.index('SeparatorText("Kept result")') < panel.index("model.provenance()")
    assert 'AvailableButton(ids.remove,result_availability(model,ids.remove,' in panel
    assert 'design::Action::Secondary,"Remove kept result"' in panel
    assert tasks.count("record_removal.request_record(document,workspace_epoch,") == 3
    assert "std::filesystem::" not in panel and "record_ids(" not in panel and "objects()" not in panel
    for caption in ["Details", "Not started", "Running", "Ready", "Inputs changed", "Cancelled", "Work failed", "Tiles: %llu / %llu", "Cell record: %s", "Model file: %s"]:
        assert '{"' + caption + '", {' in (cpp / "gui_i18n_catalog.inc").read_text(encoding="utf8")


def test_analysis_presenter_feedback_counts_literals_and_retained_trace_contract():
    cpp = ROOT / "cpp"
    panel = (cpp / "gui_result_panel.hpp").read_text(encoding="utf8")
    tasks = (cpp / "gui_analysis_tasks.hpp").read_text(encoding="utf8")
    checks = (cpp / "tests/gui_analysis_tasks_checks.hpp").read_text(encoding="utf8")
    # presenter_contracts renders literal %s/%n steps and kept labels, and
    # checks absent zero-total tiles and exact nonzero progress. The retained
    # 24 presenter traces also compare every kept-state line.
    assert 'state!=RunState::Running && state!=RunState::Cancelling' in panel
    assert 'raw_run="running"' not in tasks
    assert tasks.count('ui::Text("Samples: %llu"') == 3
    assert tasks.count('ui::Text("Markers: %zu"') == 1
    assert tasks.count('ui::Text("Run time: %.3f ms"') == 4
    assert 'mode=="record-presenter"' in checks and 'mode=="presenter"' in checks
    assert 't32-baseline-' not in checks and 'mode=="compare"' not in checks
    assert 'tests/fixtures/gui-analysis-tasks' in checks
    assert 'read(accepted)==observed.comparison()' in checks
    assert 'Missing accepted presenter trace:' in checks and 'Refusing to overwrite accepted presenter trace' in checks
    assert 'while(!server->entered.load()' in checks
    assert 'if(expanded && situation!=' not in checks
    assert 'std::filesystem::path(__FILE__)' in checks and 'open_ome_zarr((root/"tests/fixtures/ome-zarr/plain")' in checks
    assert checks.index('if(self.after)self.after(self.frame);') > checks.index('self.results.clear();')
    catalogue = (cpp / "gui_i18n_catalog.inc").read_text(encoding="utf8")
    keys = ["Smoothing tissue feature...", "Tracing tissue contours...", "Cleaning tissue mask...", "Finding components...", "Reading and thresholding...", "Reading pixels...", "Smoothing cell feature...", "Measuring distances...", "Separating nuclei...", "Expanding cell bodies...", "Tracing cell contours...", "Samples: %llu", "Markers: %zu", "Run time: %.3f ms"]
    for key in keys:
        row = re.search(r'^\{"' + re.escape(key) + r'", \{(.*)\}\},$', catalogue, re.M)
        assert row, key
        translations = json.loads("[" + row.group(1) + "]")
        assert len(translations) == 15 and all(translations), key
        conversions = re.findall(r"%(?:ll|z)?[uf]|%\.3f", key)
        assert all(re.findall(r"%(?:ll|z)?[uf]|%\.3f", value) == conversions for value in translations), key


def test_path_and_picker_messages_have_catalogue_keys():
    import json
    import re
    from pathlib import Path
    cpp = Path(__file__).resolve().parents[1] / "cpp"
    keys = {json.loads(key) for key in re.findall(r'^\{("(?:[^"\\]|\\.)*")\s*,', (cpp / "gui_i18n_catalog.inc").read_text(encoding="utf8"), re.M)}
    field = (cpp / "gui_path_field.hpp").read_text(encoding="utf8")
    messages = set()
    for assignment in re.findall(r'(?:state_\.problem|notice_)\s*=([^;]+);', field):
        messages.update(json.loads(s) for s in re.findall(r'"(?:[^"\\]|\\.)*"', assignment))
    picker = (cpp / "gui_import_picker.hpp").read_text(encoding="utf8")
    # Every explicitly thrown picker error and visible literal is localized.
    for expression in re.findall(r'throw std::\w+\(([^;]+)\);', picker):
        messages.update(json.loads(s) for s in re.findall(r'"(?:[^"\\]|\\.)*"', expression))
    for expression in re.findall(r'\berror_\s*=([^;]+);', picker):
        messages.update(json.loads(s) for s in re.findall(r'"(?:[^"\\]|\\.)*"', expression))
    for call in re.findall(r'ui::(?:Button|Checkbox|DialogInput)\(([^;]+)', picker):
        messages.update(json.loads(s) for s in re.findall(r'"(?:[^"\\]|\\.)*"', call))
    assert messages <= keys, messages - keys


def test_fix1_path_ownership_cost_and_saved_spelling():
    from pathlib import Path
    cpp = Path(__file__).resolve().parents[1] / "cpp"
    field = (cpp / "gui_path_field.hpp").read_text(encoding="utf8")
    sync = field[field.index("    void sync("):field.index("    bool draw(")]
    assert "std::string_view last=" in sync and "current_path()" not in sync
    assert "front->RootWindow->ID==input_root_window_" in field
    assert "ImGuiWindow* input_window_" not in field
    assert "ImGui::IsItemActivated() || (hovered_ && !was_hovered)" in field
    dark = (cpp / "gui_dark_flat.cpp").read_text(encoding="utf8")
    for index in (1, 2):
        assert f"restoring?restoring->sources[{index}].path:" in dark
    raw = (cpp / "gui_raw_pixel.hpp").read_text(encoding="utf8")
    assert "return bool(jobs_.report().link);" not in raw
    assert raw.count('return jobs_.report().state=="saved";') == 3
    forms = (cpp / "gui_project_forms.hpp").read_text(encoding="utf8")
    assert 'if(shortcut && *open_path.text() && !busy)request.open=true;' in forms
    assert 'if(!has_project)' in forms and 'Open a project to add images.' in forms


def _result_inventory_module():
    import importlib.util
    from pathlib import Path
    spec = importlib.util.spec_from_file_location("result_inventory", Path(__file__).with_name("gui_result_state_inventory.py"))
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


def test_result_state_source_inventory():
    inventory = _result_inventory_module()
    rows = inventory.inventory()
    assert rows and all(not state.endswith("::Unknown") for _, _, state in rows)
    known = {(family, token) for family, token, _ in rows}
    assert set(inventory.FAMILY_PARSERS) == {family for family, _, _ in rows}
    missing = [(kind, token, path, line) for kind, token, path, line in inventory.observations() if (kind, token) not in known]
    assert not missing, missing
    # Characterize accessor/field, reversed, whitespace, comments and exact spelling.
    assert list(inventory.compared('job.poll().state == "new_token"; "reverse" != document.cell_status(id); // state=="comment"')) == [("new_token", 1), ("reverse", 1)]
    assert not list(inventory.compared('field.state().resolved == "a.csv"; message == "failed";'))
    assert list(inventory.compared('task_ == "future_raw_task";')) == [("future_raw_task", 1)]
    assert list(inventory.record_returns('std::string status(const ReaderSource& s,const FileFingerprint& f) {return s.scope=="directory_store"?"matching_primary_metadata":"matching_metadata";}')) == [("matching_primary_metadata", 1), ("matching_metadata", 1)]
    assert list(inventory.record_returns('std::string Document::cell_status(int id) const {if(id<0)throw std::runtime_error("not a token");return "future status";}')) == [("future status", 1)]
    gui = (inventory.ROOT / "cpp/gui.cpp").read_text(encoding="utf8")
    route = next(line for line in gui.splitlines() if "route_path_drop(drop," in line)
    assert "&drop_refusal,&drop_reason)" in route and "&project_error)" not in route


def test_result_state_caption_catalogue():
    import json
    import re
    inventory = _result_inventory_module()
    header = (inventory.ROOT / "cpp/gui_result_state.hpp").read_text(encoding="utf8")
    keys = set(re.findall(r'i18n::text\("([^"]+)"\)', header))
    catalogue = (inventory.ROOT / "cpp/gui_i18n_catalog.inc").read_text(encoding="utf8")
    entries = {}
    for match in re.finditer(r'^\{("(?:[^"\\]|\\.)*"),\s*\{(.*)\}\},$', catalogue, re.M):
        key = json.loads(match[1])
        assert key not in entries, key
        entries[key] = json.loads("[" + match[2] + "]")
    assert keys <= entries.keys(), keys - entries.keys()
    for key in keys:
        translated = entries[key]
        assert len(translated) == 15 and all(translated), key
        conversions = re.findall(r'%(?:\d+\$)?[-+ #0]*\d*(?:\.\d+)?(?:ll|l|z)?[a-zA-Z%]', key)
        assert all(re.findall(r'%(?:\d+\$)?[-+ #0]*\d*(?:\.\d+)?(?:ll|l|z)?[a-zA-Z%]', text) == conversions for text in translated), key


def test_record_lists_keep_native_actions_and_clipping():
    import json
    import re
    from pathlib import Path
    cpp = Path(__file__).resolve().parents[1] / "cpp"
    header = (cpp / "gui_record_list.hpp").read_text(encoding="utf8")
    assert 'ImGuiListClipper clip' in header and 'RecordRows' in header
    assert 'owner_==owner && revision_==revision && extra_==extra' in header
    assert 'short_record_id' in header and 'id.substr(0,8)' in header
    assert 'Copy identifier' in header and 'SetClipboardText' in header
    for name in ['gui_raw_pixel.hpp', 'gui_raw.hpp', 'gui_pixel_classifier.hpp', 'gui_object_classifier.hpp', 'gui_spatial.hpp']:
        text = (cpp / name).read_text(encoding="utf8")
        assert 'ui::draw_record_list(' in text, name
    raw = (cpp / 'gui_raw_pixel.hpp').read_text(encoding='utf8')
    assert 'ImGui::InputScalar("Record ID' not in raw
    assert 'if(matched_record_ && record_==matched_record_)record_=0;matched_record_=0;' in raw
    spatial = (cpp / 'gui_spatial.hpp').read_text(encoding='utf8')
    assert 'mark(GuiInputTarget::SpatialRecordId,idle,mark_target)' in spatial
    assert 'if(highlighted!=record_) {record_=highlighted;try {inspect(doc);}' in spatial
    assert 'frontend_.export_csv(*document,path,limit,paths,snapshot)' in spatial
    object_classifier = (cpp / 'gui_object_classifier.hpp').read_text(encoding='utf8')
    assert '&Document::remove_classifier_record' in object_classifier and 'removal->request_record' in object_classifier
    gui = (cpp / 'gui.cpp').read_text(encoding='utf8')
    assert 'if(const auto saved=document.latest_cells(selected))begin_export(ExportOp::SavedCells,{saved->id},false,source);' in gui
    assert 'else if(table_scope==2)begin_export(ExportOp::ImageCells,{},true)' in gui
    assert 'if(table_kind==0 && table_scope!=2)' not in gui
    assert 'ui::caption(ui::parse_export_state(table_status.state),table_status.state)' in gui
    assert 'ui::caption(ui::parse_export_state(measurement_export_status.state),measurement_export_status.state)' in gui
    rows = (cpp / 'gui_cell_record_rows.hpp').read_text(encoding='utf8')
    assert 'document.cell_records()' in rows and 'document.cell_status(r->id)' in rows
    batch = (cpp / 'gui_batch.hpp').read_text(encoding='utf8')
    assert 'ui::open_in_shell(folder)' in batch
    assert 'Add cell objects (in checkpoint projects)###Publish cell objects' in batch
    assert 'save_batch_checkpoint_csv' not in batch
    catalogue = (cpp / 'gui_i18n_catalog.inc').read_text(encoding='utf8')
    for key in ['Dark/flat correction', 'Linear unmixing', 'None / original input']:
        row = re.search(r'^\{"' + re.escape(key) + r'", \{(.*)\}\},$', catalogue, re.M)
        assert row, key
        translations = json.loads('[' + row[1] + ']')
        assert len(translations) == 15 and all(translations), key


def test_all_list_headings_and_fix1_vocabulary_are_translated():
    import json
    import re
    from pathlib import Path
    cpp = Path(__file__).resolve().parents[1] / 'cpp'
    catalogue = (cpp / 'gui_i18n_catalog.inc').read_text(encoding='utf8')
    entries = {json.loads(m[1]): json.loads('['+m[2]+']') for m in re.finditer(r'^\{("(?:[^"\\]|\\.)*"),\s*\{(.*)\}\},$',catalogue,re.M)}
    headings = {'No kept records', 'Reused', 'Choose kept recipe'}
    for p in cpp.glob('gui*.*'):
        if p.suffix not in {'.cpp','.hpp'}: continue
        text=p.read_text(encoding='utf8')
        for heading in re.findall(r'(?:draw_record_list|(?:\w+_rows|record_rows_)\.draw)\("([^"]+)"',text):
            heading=heading.split('##')[0]
            if heading:headings.add(heading)
    assert {'Input correction (explicit)','Unmixing recipe scope'} <= headings
    for heading in headings:
        assert heading in entries,heading
        assert len(entries[heading])==15 and all(entries[heading]),heading
        conversions=re.findall(r'%(?:\d+\$)?[-+ #0]*\d*(?:\.\d+)?(?:ll|l|z)?[a-zA-Z%]',heading)
        assert all(re.findall(r'%(?:\d+\$)?[-+ #0]*\d*(?:\.\d+)?(?:ll|l|z)?[a-zA-Z%]',v)==conversions for v in entries[heading]),heading


def test_status_text_requires_own_caption_family():
    inventory=_result_inventory_module()
    bad=[]
    for p in sorted((inventory.ROOT/'cpp').glob('gui*.*')):
        if p.suffix in {'.cpp','.hpp'}:
            bad.extend((p.name,line,name) for line,name in inventory.uncaptioned_status_text(p.read_text(encoding='utf8')))
    assert not bad,bad
    assert list(inventory.uncaptioned_status_text('ImGui::TextWrapped("Contours: %s",contour_state().c_str());'))
    assert list(inventory.uncaptioned_status_text('ui::Text("%s / %s",caption(parse_run_state(a.state),a.state).c_str(),b.state.c_str());'))
    assert not list(inventory.uncaptioned_status_text('ui::Text("%s",caption(parse_source_metadata_state(a.status),a.status).c_str());'))
    assert not list(inventory.uncaptioned_status_text('ui::TextWrapped("%s",interchange_status.c_str());'))
    assert not list(inventory.uncaptioned_status_text('ui::Text("%llu",status.bytes);'))


def test_fix1_list_native_state_and_cached_presentation_contract():
    from pathlib import Path
    cpp=Path(__file__).resolve().parents[1]/'cpp'
    header=(cpp/'gui_record_list.hpp').read_text(encoding='utf8')
    assert 'std::optional<RecordState> state' in header
    assert 'std::nullopt' in header and '"unverified"' not in header
    for name in ['gui_object_classifier.hpp', 'gui_pixel_classifier.hpp', 'gui_raw_pixel.hpp']:
        assert '"unverified"' not in (cpp/name).read_text(encoding='utf8'), name
    assert 'if(rows.empty())TextWrapped' in header
    cache=header[header.index('class RecordRows'):header.index('inline std::uint64_t draw_record_list')]
    assert 'two_lines=std::any_of' in cache
    draw=header[header.index('inline std::uint64_t draw_record_list'):header.index('inline RecordRow record_row')]
    assert 'std::any_of' not in draw and 'std::vector<std::pair' not in draw
    assert 'std::min<std::size_t>(3,rows.size())' in draw
    for name in ['gui_fluorescence.cpp','gui_dark_flat.cpp','gui_linear_unmixing.cpp','gui_marker_rules.cpp']:
        text=(cpp/name).read_text(encoding='utf8')
        assert 'RecordRemovalDialog' not in text and 'removal.draw' not in text,name
    fluo=(cpp/'gui_fluorescence.cpp').read_text(encoding='utf8')
    assert 'record_identifier_details(chosen_correction_id)' in fluo
    tma=(cpp/'gui_tma.hpp').read_text(encoding='utf8')
    assert 'exchange_exporting_?ui::caption(ui::parse_export_state(exchange_state_),exchange_state_)' in tma
    panel=(cpp/'gui_result_panel.hpp').read_text(encoding='utf8')
    assert 'Add saved cells' not in panel and 'AvailableButton(ids.add,result_availability(model,ids.add,' in panel
    raw=(cpp/'gui_raw.hpp').read_text(encoding='utf8')
    assert 'std::string(text)' not in raw
    assert 'row.detail="Z "' in raw and 'selected_axes(*r->result)' in raw
    gui=(cpp/'gui.cpp').read_text(encoding='utf8')
    assert gui.index('if(table_kind!=0 && table_scope==2)table_scope=0;') < gui.index('ui::Combo("Scope",&table_scope')
    assert 'table_record_rows,0,list_actions,true)' in gui


def test_fix2_unresolved_selection_observation_and_path_guard():
    from pathlib import Path
    cpp=Path(__file__).resolve().parents[1]/'cpp'
    header=(cpp/'gui_record_list.hpp').read_text(encoding='utf8')
    guard=header.index('if(record_list_presentation || record_list_presentations) {')
    assert guard < header.index('const RecordListPresentation presentation{') < header.index('if(rows.empty())TextWrapped')
    assert 'selected.empty()?0:found==handles_.end()?std::numeric_limits<std::uint64_t>::max():found->second' in header
    assert 'next==0 && none_choice?std::string{}:selected' in header
    assert 'table_record_selected' not in (cpp/'gui.cpp').read_text(encoding='utf8')
    check=(cpp/'tests/gui_record_list_checks.hpp').read_text(encoding='utf8')
    assert 'std::filesystem::equivalent(missing.parent_path().parent_path(),std::filesystem::absolute(dir)/"batch-output")' in check
    assert 'missing.parent_path().parent_path()==' not in check
    assert 'folder==std::filesystem::u8path' not in check


def test_historical_model_inspection_and_session_warning_contract():
    pixel = (ROOT / "cpp/gui_pixel_classifier.hpp").read_text(encoding="utf-8")
    raw = (ROOT / "cpp/gui_raw_pixel.hpp").read_text(encoding="utf-8")
    preview = pixel.split('if(button("Preview saved mask",', 1)[1].split('ImGui::SameLine();', 1)[0]
    assert "adopt(" not in preview and "cancel_training(" not in preview
    assert "record->archive->result()" in preview
    assert "record->archive->model()->classes()" in preview
    assert "preview_palette()" in pixel
    assert pixel.count("ui::session_model_warning()") == 2
    assert raw.count("ui::session_model_warning()") == 1
    assert pixel.index('path_.draw(context,blocked || busy,') < pixel.index('save_path_.draw(')
    assert raw.index('model_path_.draw(') < raw.index('model_output_.draw(')
    assert 'if(out.model==model_)model_saved_=true' in pixel
    assert 'after.model==saving_model_' in raw
    assert 'session_model_warning()' in (ROOT / "cpp/gui_result_state.hpp").read_text(encoding="utf-8")
    import re
    catalog = (ROOT / "cpp/gui_i18n_catalog.inc").read_text(encoding="utf-8")
    warning = re.search(r'^\{"The current model has not been kept or saved\.", \{(.*?)\}\},$', catalog, re.M)
    assert warning and len(re.findall(r'"([^"\n]+)"', warning.group(1))) == 15
    frontend = (ROOT / "cpp/raw_pixel_frontend.cpp").read_text(encoding="utf-8")
    opening = frontend.split('void RawPixelFrontend::open(', 1)[1].split('void RawPixelFrontend::load_model', 1)[0]
    assert 'adopt(' not in opening and 's.report.model=' not in opening


def test_milestone3_review_a_exclusive_mask():
    text = (ROOT / "cpp/gui_pixel_classifier.hpp").read_text(encoding="utf8")
    assert 'Close kept mask###Close kept pixel mask' in text
    availability = text.split('const auto availability=', 1)[1].split('auto button=', 1)[0]
    for target, command, key in [('TracePixelContours', 'Trace_pixel_contours', 'Trace pixel contours'),
                                 ('PublishPixelContours', 'Publish_class_annotations', 'Publish class annotations'),
                                 ('KeepPixelPrediction', 'Keep_prediction_mask', 'Keep prediction mask')]:
        case = availability.split('case ui::CommandId::Action_' + command + ':', 1)[1].split('break;', 1)[0]
        assert 'if(historical_)return {false,"Close the kept mask first."};' in case
        action = text.split('if(button("' + key + '",', 1)[1].split('))', 1)[0]
        assert 'GuiInputTarget::' + target in action and 'historical_' not in action
    button = text.split('auto button=', 1)[1].split('if(training_stats_', 1)[0]
    assert 'available=availability(ui::workflow_command(label).id)' in button
    assert 'ui::WorkflowAction(label,action,available)' in button
    # --milestone3-review presses these rendered controls under a kept mask;
    # --reasons-help additionally checks their translated tooltip reasons.
    assert 'if(!historical_ && prediction_stats_.state' in text


def test_milestone3_review_b_safe_legend():
    text = (ROOT / "cpp/gui_pixel_classifier.hpp").read_text(encoding="utf8")
    assert 'std::min(historical_->model().classes().size(),historical_palette_.size())' in text
    assert '##kept-class-color' in text


def test_milestone3_review_c_retention_facts():
    text = (ROOT / "cpp/gui_pixel_classifier.hpp").read_text(encoding="utf8")
    assert 'model_saved_' in text and 'refresh_model_retention' in text
    body = text.split('void refresh_model_retention', 1)[1].split('void scales_text', 1)[0]
    assert 'doc.pixel_classifier_record(model_record_)' in body
    assert 'retention_revision_!=doc.revision()' in body


def test_milestone3_review_d_removed_raw_selection():
    text = (ROOT / "cpp/gui_raw_pixel.hpp").read_text(encoding="utf8")
    update = text.split('void update(Document& doc)', 1)[1].split('bool settled()', 1)[0]
    assert 'doc.raw_pixel_stream_record(record_)' in update
    assert 'record_=0' in update


def test_milestone3_review_e_read_only_table_scope():
    text = (ROOT / "cpp/gui.cpp").read_text(encoding="utf8")
    assert 'cell_table_options.published_objects=false;' not in text
    assert 'if(all_kept_runs)intent.cell_options.published_objects=false;' in text


def test_milestone3_review_f_additive_capture_sweep():
    text = (ROOT / "cpp/tests/gui_tool_path_checks.hpp").read_text(encoding="utf8")
    assert 'o.raw_tab=1' in text
    for entry in ['raw-records', 'raw-pixel-records', 'fluorescence-records']:
        assert '{"' + entry + '"' in text
    assert 'fluo->set_tiling(true)' in text
    assert 'GuiInputTarget::RawPixelFilesTab' in text


def test_milestone3_review_g_kept_vocabulary():
    genes = (ROOT / "cpp/gui_genes.hpp").read_text(encoding="utf8")
    assert 'Apply kept normalization###Apply saved normalization' in genes
    assert 'Remove kept normalization###Remove saved normalization' in genes
    text = (ROOT / "cpp/gui.cpp").read_text(encoding="utf8")
    for old in ['before exporting saved cells.', 'Recheck the saved cell run', 'Historical samples from the saved run.']:
        assert old not in text


def test_milestone3_review_h_two_report_snapshots():
    text = (ROOT / "cpp/gui_raw_pixel.hpp").read_text(encoding="utf8")
    update = text.split('void update(Document& doc)', 1)[1].split('bool settled()', 1)[0]
    assert update.count('jobs_.report()') == 2


def test_milestone3_review2_j_only_matched_raw_selection_falls_back():
    text = (ROOT / 'cpp/gui_raw_pixel.hpp').read_text(encoding='utf8')
    assert 'matched_record_' in text
    assert 'record_==matched_record_' in text


def test_milestone3_review2_k_weak_holder_document():
    text = (ROOT / 'cpp/gui_pixel_classifier.hpp').read_text(encoding='utf8')
    assert 'std::weak_ptr<const char> retention_identity_,holder_identity_' in text
    assert 'holder_identity_.expired()' in text


def test_milestone3_review2_l_historical_opacity():
    text = (ROOT / 'cpp/gui_pixel_classifier.hpp').read_text(encoding='utf8')
    historical = text.split('Close kept mask###Close kept pixel mask', 1)[1]
    assert 'draw_opacity()' in historical
