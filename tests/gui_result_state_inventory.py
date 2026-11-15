"""Source-only guard for the GUI result vocabulary; no native execution or fixtures."""
import json
import re
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
# Jobs/frontends the GUI polls, their shared job implementation, and record authorities.
NATIVE = """analysis.cpp tissue.cpp cells.cpp stain_estimation.cpp file_fingerprint.cpp
cell_tables.cpp measurement_tables.cpp project_batch.cpp project_save.cpp project_recovery.cpp
thumbnail.cpp object_classifier.cpp pixel_training.cpp pixel_prediction.cpp pixel_contour_job.cpp
pixel_stream_export_job.cpp pixel_stream_read_job.cpp raw_analysis.cpp raw_tables.cpp
raw_pixel_frontend.cpp raw_pixel_workflows.cpp raw_pixel_window_workflows.cpp raw_pixel_tiled_training.cpp
raw_pixel_contour_job.cpp raw_pixel_stream.cpp raw_pixel_stream_contours.cpp raw_pixel_file_job_internal.hpp
spatial_frontend.cpp spatial_analysis.cpp spatial_neighborhood.cpp spatial_density.cpp density_workflow.cpp
fluorescence_cell_workflow.cpp fluorescence_tiled_workflow.cpp fluorescence_tiled_checkpoint.cpp
source_identity.cpp analysis_records.cpp tissue_records.cpp cell_records.cpp raw_analysis_records.cpp
neighborhood_records.cpp density_records.cpp cell_object_status.hpp""".split()
LEX = re.compile(r'//[^\n]*|/\*[\s\S]*?\*/|"(?:[^"\\]|\\.)*"|[A-Za-z_]\w*|==|!=|->|[^\s]')
STATE = re.compile(r'(?:\w*(?:state|status)_?|task_)')
FAMILY_PARSERS = {"run": "parse_run_state", "record": "parse_record_state",
                  "export": "parse_export_state", "recovery": "parse_recovery_state",
                  "sourcemetadata": "parse_source_metadata_state"}

def inventory():
    text = (ROOT / "cpp/tests/gui_result_state_checks.hpp").read_text(encoding="utf8")
    return [(family.lower(), token, family + "State::" + state)
            for token, family, state in re.findall(r'\{"([^"]+)",(Run|Record|Export|Recovery|SourceMetadata)State::(\w+)\}', text)]

def lex(source):
    return [(m.group(), source.count("\n", 0, m.start()) + 1)
            for m in LEX.finditer(source) if not m.group().startswith(("//", "/*"))]

def compared_fields(source):
    ts = lex(source)
    def name(i, step):
        if i < 0 or i >= len(ts): return ""
        if step < 0 and ts[i][0] == ")":
            depth = 1; i -= 1
            while i >= 0 and depth:
                depth += (ts[i][0] == ")") - (ts[i][0] == "("); i -= 1
        if step > 0:
            while i+2 < len(ts) and ts[i+1][0] in (".", "->"):
                i += 2
        return ts[i][0] if 0 <= i < len(ts) else ""
    for i, (token, line) in enumerate(ts):
        if token not in ("==", "!="): continue
        for si, ei, step in [(i+1, i-1, -1), (i-1, i+1, 1)]:
            if si < 0 or si >= len(ts) or not ts[si][0].startswith('"'): continue
            field = name(ei, step)
            if STATE.fullmatch(field):
                if step < 0 and ei > 0 and ts[ei-1][0] in (".", "->"):
                    field = "." + field
                yield json.loads(ts[si][0]), line, field

def compared(source):
    yield from ((token, line) for token, line, _ in compared_fields(source))

def emitted(source):
    ts = lex(source)
    for i, (field, line) in enumerate(ts[:-1]):
        if not STATE.fullmatch(field) or field in {"interchange_status", "clipboard_status", "status"}: continue
        if ts[i+1][0] not in ("=", "{"): continue
        # Stop at the assignment/initializer boundary, preserving ternary arms.
        depth = 0
        for token, at in ts[i+2:]:
            if depth == 0 and token in (";", ",", "}"): break
            if token in ("(", "{", "["): depth += 1
            if token in (")", "}", "]"): depth -= 1
            if token.startswith('"'): yield json.loads(token), at
    # Raw frontend uses fail(state, error) to finish both successful and failed operations.
    for match in re.finditer(r'\bs\.fail\(([^;]+)', source):
        expression = match[1].split(",", 1)[0]
        for token in re.findall(r'"([^"\\]*)"', expression):
            yield token, source.count("\n", 0, match.start())+1
    # String return of the streaming state accessor (its storage is a native enum).
    for match in re.finditer(r'std::string RawPixelPredictionStream::state\(\) const \{([^\n]+)', source):
        for token in re.findall(r'return "([^"]+)"', match[1]):
            yield token, source.count("\n", 0, match.start())+1

def record_returns(source):
    # Bound the functions so unrelated numeric/JSON/geometry helper returns are excluded.
    signatures = r'(?:std::string Document::(?:\w+_status)|inline std::string cell_geometry_status)\([^\n]*\) const \{|inline std::string cell_geometry_status\([^\n]*\) \{'
    signatures += r'|std::string status\(const ReaderSource&[^\n]*\) \{'
    for match in re.finditer(signatures, source):
        start = match.end(); depth = 1; end = start
        while end < len(source) and depth:
            depth += (source[end] == "{") - (source[end] == "}"); end += 1
        for returned in re.finditer(r'\breturn\s+([^;]+);', source[start:end]):
            expression = returned[1]
            for literal in re.finditer(r'"([^"\\]*)"', expression):
                before = expression[:literal.start()].rstrip()
                after = expression[literal.end():].lstrip()
                if before.endswith(("==", "!=")) or after.startswith(("==", "!=")): continue
                offset = start+returned.start(1)+literal.start()
                yield literal[1], source.count("\n", 0, offset)+1

def observations():
    records = {"analysis_records.cpp", "tissue_records.cpp", "cell_records.cpp", "raw_analysis_records.cpp", "neighborhood_records.cpp", "density_records.cpp", "cell_object_status.hpp", "spatial_analysis.cpp", "source_identity.cpp"}
    sources = [ROOT / "cpp" / name for name in NATIVE]
    sources += [p for p in sorted((ROOT / "cpp/include/qupathxx").glob("*.hpp")) if p.name != "viewer_session.hpp"]
    gui = [p for p in sorted((ROOT / "cpp").glob("gui*.*")) if p.suffix in (".cpp", ".hpp") and p.name != "gui_result_state.hpp"]
    rows = []
    for p in sources + gui:
        source = p.read_text(encoding="utf8")
        for token, line in emitted(source):
            context = source.splitlines()[line-1]
            family = "recovery" if "base_status" in context or (p.name == "project_recovery.cpp" and "unavailable_status" in context) else "record" if "record_status=" in context else "export" if token in {"saving", "writing", "exporting", "clearing"} else "run"
            rows.append((family, token, str(p.relative_to(ROOT)).replace("\\", "/"), line))
        if p.name in records:
            family = "sourcemetadata" if p.name == "source_identity.cpp" else "record"
            for token, line in record_returns(source): rows.append((family, token, str(p.relative_to(ROOT)).replace("\\", "/"), line))
    for p in gui:
        source = p.read_text(encoding="utf8")
        for token, line, field in compared_fields(source):
            context = source.splitlines()[line-1]
            preceding = "\n".join(source.splitlines()[:line])
            declarations = list(re.finditer(r'const auto '+re.escape(field)+r'\s*=([^;]+);', preceding))
            record_local = declarations and re.search(r'\w+_status\(', declarations[-1][1])
            family = "recovery" if field.lstrip(".") == "base_status" else "record" if field.endswith("_status") or record_local else "export" if token in {"saving", "writing", "exporting", "clearing"} else "run"
            rows.append((family, token, str(p.relative_to(ROOT)).replace("\\", "/"), line))
    return rows

# These values are human-authored diagnostic prose, never machine vocabulary.
FREE_TEXT_STATUS_FIELDS = {"interchange_status", "clipboard_status"}
TEXT_CALLS = {"Text", "TextWrapped", "TextUnformatted", "TextDisabled", "TextColored", "AddText"}

def uncaptioned_status_text(source):
    """Inspect each text argument independently, including multiline/qualified calls.

    Caption calls may be inline or assigned to a local caption; receivers such as
    status.bytes and PathField.state().resolved are structured values, not tokens.
    """
    ts = lex(source)
    def close(start):
        depth = 1
        for i in range(start + 1, len(ts)):
            depth += (ts[i][0] == "(") - (ts[i][0] == ")")
            if not depth: return i
        return len(ts)-1
    def split(start, end):
        depth = 0; at = start
        for i in range(start, end):
            t = ts[i][0]
            if t in {"(", "[", "{"}: depth += 1
            if t in {")", "]", "}"}: depth -= 1
            if t == "," and not depth:
                yield at, i; at = i+1
        yield at, end
    def caption_local(name, index):
        for k in range(index-1,-1,-1):
            if ts[k][0] != name or k+1 >= len(ts) or ts[k+1][0] != "=": continue
            j=k+2; expression=[]
            while j < len(ts) and ts[j][0] != ";": expression.append(ts[j][0]);j+=1
            return "caption" in expression or "run_caption" in expression
        return False
    for i, (call, _) in enumerate(ts[:-1]):
        if call not in TEXT_CALLS or ts[i+1][0] != "(": continue
        end=close(i+1)
        for begin, stop in split(i+2,end):
            j=begin
            while j < stop:
                name,line=ts[j]
                if name in {"caption", "run_caption"} and j+1 < stop and ts[j+1][0] == "(":
                    j=close(j+1)+1;continue
                is_token = bool(re.fullmatch(r"\w*(?:state|status)_?",name)) or name == "task_"
                if is_token and name not in FREE_TEXT_STATUS_FIELDS and not caption_local(name,j):
                    # Receiver of .field/.method other than c_str is not a string argument.
                    after=j+1
                    if after < stop and ts[after][0] == "(":after=close(after)+1
                    receiver=after+1 < stop and ts[after][0] in {".","->"} and ts[after+1][0] != "c_str"
                    comparison=after < stop and ts[after][0] in {"==","!=","<",">"}
                    if not receiver and not comparison:yield line,name
                j+=1
