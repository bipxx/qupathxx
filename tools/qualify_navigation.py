"""Qualify the frozen RTX 4090 NDPI navigation workload; retain every trial."""
import argparse
from datetime import datetime, timezone
import hashlib
import json
import math
import os
from pathlib import Path
import platform
import subprocess
import sys


ROOT = Path(__file__).resolve().parents[1]
ENGINE = "2afdcb99555bf142211cdfa836a4671a6f54bbf1"
TOOL_SHA = "acb0f034fcc818519cefc20f3bc7222c6b470b30169d76a49a07b0dc235a3bc2"
FIXTURE_SHA = "edf4a1ccf395c7000ae93ad3b44c07d97043810e00be0c1d167dd09bbe436e46"
RENDERER = "NVIDIA GeForce RTX 4090/PCIe/SSE2"


def sha(path):
    with path.open("rb") as stream:
        return hashlib.file_digest(stream, "sha256").hexdigest()


def snapshot():
    command = (
        "$cpu=Get-CimInstance Win32_Processor;"
        "$system=Get-CimInstance Win32_ComputerSystem;"
        "$video=Get-CimInstance Win32_VideoController;"
        "[pscustomobject]@{time=(Get-Date).ToString('o');"
        "cpu=@($cpu|Select-Object Name,NumberOfCores,NumberOfLogicalProcessors,LoadPercentage);"
        "memory_bytes=$system.TotalPhysicalMemory;"
        "video=@($video|Select-Object Name,DriverVersion,CurrentHorizontalResolution,"
        "CurrentVerticalResolution,CurrentRefreshRate)}|ConvertTo-Json -Depth 5"
    )
    return json.loads(subprocess.check_output(
        ["pwsh", "-NoProfile", "-Command", command], encoding="utf-8-sig", timeout=30))


def hardware_issues(context):
    issues = []
    cpus = context["cpu"]
    if (len(cpus) != 1 or "i7-12700K" not in cpus[0]["Name"] or
            cpus[0]["NumberOfCores"] != 12 or cpus[0]["NumberOfLogicalProcessors"] != 20):
        issues.append("CPU differs from the frozen hardware")
    if context["memory_bytes"] != 137167814656:
        issues.append("RAM differs from the frozen hardware")
    if any(not isinstance(c["LoadPercentage"], (int, float)) or
           not 0 <= c["LoadPercentage"] <= 25 for c in cpus):
        issues.append("observed CPU load exceeds 25 percent or is unavailable")
    gpu = [g for g in context["video"] if g["Name"] == "NVIDIA GeForce RTX 4090"]
    if len(gpu) != 1 or gpu[0]["DriverVersion"] != "32.0.16.1088":
        issues.append("GPU or driver differs from the frozen hardware")
    return issues


def inspect_view(view, directory, source_sha):
    issues = []
    expected = dict(framebuffer_size=[2560, 1440], frames=180,
                    rendered_frame_wall_samples=170, replay_complete=True,
                    tiles_settled=True, pending_tiles_at_exit=0, ui_overflow_items=0,
                    font_rebuilds=4, gl_renderer=RENDERER, generations=25)
    for key, value in expected.items():
        if view.get(key) != value:
            issues.append(f"{key} differs from the protocol")
    metrics = [view[k] for k in ("cpu_frame_p95_ms", "max_cpu_frame_ms",
               "rendered_frame_wall_p95_ms", "max_rendered_frame_wall_ms")]
    if not all(isinstance(v, (int, float)) and math.isfinite(v) and v > 0 for v in metrics):
        issues.append("invalid frame timing")
    else:
        cpu, max_cpu, wall, max_wall = metrics
        if not cpu <= wall <= 33 or max_wall < max(max_cpu, wall):
            issues.append("rendered-loop p95 exceeds 33 ms or timing bounds are inconsistent")
    if not 0 < view["peak_gpu_payload_bytes"] <= 64 * 1024 * 1024:
        issues.append("GPU payload exceeds the protocol budget or is empty")
    if (not math.isfinite(view["final_zoom"]) or
            abs(view["final_zoom"] - 5.681747443875566) > 1e-9 or
            len(view["final_center"]) != 2 or
            any(not math.isfinite(a) or abs(a-b) > .01 for a, b in
                zip(view["final_center"], (25579.198625411034, 19048.82822789989)))):
        issues.append("final camera differs from the completed navigation workload")
    name = view["name"]
    if name not in ("first-view", "repeat-view"):
        return issues + ["unexpected view name"]
    path = directory / (name + ".json")
    profile = view["native_phase_profile"]
    if Path(profile["path"]).resolve() != path.resolve() or sha(path) != profile["sha256"]:
        return issues + ["phase trace identity mismatch"]
    trace = json.loads(path.read_text(encoding="utf-8"))
    frames = trace["frames"]
    if (trace["schema"] != "qpx.gui-profile/1" or trace["source_tree_sha256"] != source_sha or
            [f["frame"] for f in frames] != list(range(180))):
        return issues + ["phase trace source or frames differ"]
    phases = ("model_ms", "widgets_ms", "tiles_ms", "cache_ms", "query_ms",
              "geometry_ms", "other_ms", "render_ms")
    if any(not all(math.isfinite(f[k]) and f[k] >= 0 for k in phases) or
           sum(f[k] for k in phases) > f["total_ms"] + .01 for f in frames):
        issues.append("phase durations are inconsistent")
    warm = sorted(f["total_ms"] for f in frames[10:])
    if not all(math.isfinite(v) and v > 0 for v in warm) or abs(warm[160] - metrics[0]) > 1e-6:
        issues.append("phase trace does not reproduce CPU p95")
    return issues


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--engine-audit", type=Path, required=True)
    parser.add_argument("--image", type=Path, required=True)
    parser.add_argument("--output-directory", type=Path, required=True)
    args = parser.parse_args()
    output = args.output_directory.resolve()
    output.mkdir(parents=True, exist_ok=False)
    report = dict(schema="qpx.navigation-qualification/1", engine_commit=ENGINE,
                  started_utc=datetime.now(timezone.utc).isoformat(),
                  navigation_target_accepted=False, overall_project_complete=False,
                  overall_performance_accepted=False, release_ready=False, trials=[],
                  tool_sha256=sha(Path(__file__)),
                  protocol_sha256=sha(ROOT / "docs/navigation-performance-protocol.md"),
                  scope="Frozen RTX 4090 NDPI navigation workload only; three independent processes and six views.")

    def require(condition, message):
        if not condition:
            raise RuntimeError(message)

    try:
        require(os.name == "nt", "qualification requires the declared Windows host")
        audit = json.loads(args.engine_audit.read_text(encoding="utf-8"))
        report["engine_audit_sha256"] = sha(args.engine_audit)
        require(audit["commit"] == audit["engine_commit"] == ENGINE,
                "engine audit does not qualify the frozen source")
        require(audit["windows_linux_engineering_scope_accepted"] is True and
                audit["ndpi_readers_required"] is True and len(audit["jobs"]) == 5 and
                all(j["status"] == "completed" and j["conclusion"] == "success" for j in audit["jobs"]),
                "complete source-specific Windows/Linux and NDPI acceptance is required")
        require(audit["auditor"]["sha256"] == sha(ROOT / "tools/ci/accept_unified_engine.py"),
                "engine audit was not produced by the current source auditor")
        gui = [j for j in audit["jobs"] if j["name"] == "Windows GUI Python 3.12 software GL"]
        require(len(gui) == 1 and gui[0].get("rendered_loop_timing", {}).get("passed") == 4,
                "audited GUI timing contracts are missing")
        require(audit.get("gui_stack_required") is True and gui[0].get("gui_stack", {}).get("passed") == 1,
                "audited default-stack native startup regression is missing")
        require(audit.get("watershed_bodies_required") is True and
                gui[0].get("watershed_bodies", {}).get("passed") == 42,
                "audited production watershed-body workflows are missing")
        import qupathxx as q
        from qupathxx import _native
        native = Path(_native.__file__).resolve()
        workflow = gui[0]["workflow"]
        require(q.has_gui and q.build_environment().git_revision == ENGINE and
                sha(native) == workflow["native_sha256"] and
                q.build_environment().source_tree_sha256 == workflow["source_tree_sha256"],
                "installed native binary is not the audited GUI wheel")
        require("site-packages" in native.parts and not (native.parent / "opengl32.dll").exists(),
                "use a clean installed wheel without software OpenGL DLLs")
        report["native_sha256"] = sha(native)
        report["source_tree_sha256"] = workflow["source_tree_sha256"]
        report["wheel_sha256"] = gui[0]["artifact"]["wheel_sha256"]
        benchmark = ROOT / "tools/benchmark_viewer.py"
        require(sha(benchmark) == TOOL_SHA, "viewer script differs from the frozen protocol")
        require(args.image.stat().st_size == 198030965 and sha(args.image) == FIXTURE_SHA,
                "NDPI fixture differs from the frozen protocol")
        report["benchmark_tool_sha256"] = TOOL_SHA
        report["fixture_sha256"] = FIXTURE_SHA
        report["host"] = dict(platform=platform.platform(), python=platform.python_version())
        env = os.environ.copy()
        for key in ("GALLIUM_DRIVER", "LIBGL_ALWAYS_SOFTWARE", "QUPATHXX_GUI_PROFILE"):
            env.pop(key, None)
        for index in range(1, 4):
            receipt = output / f"trial-{index}.json"
            phases = output / f"trial-{index}-phases"
            trial = dict(number=index, before=snapshot(), issues=[])
            report["trials"].append(trial)
            trial["issues"] += ["before: " + v for v in hardware_issues(trial["before"])]
            print(f"Starting navigation trial {index}", flush=True)
            command = [sys.executable, "-I", str(benchmark), str(args.image.resolve()),
                       "--frames", "180", "--width", "2560", "--height", "1440",
                       "--navigate", "--docked", "--scale-cycle",
                       "--profile-directory", str(phases), "--output", str(receipt)]
            with (output / f"trial-{index}.log").open("w", encoding="utf-8") as stream:
                result = subprocess.run(command, env=env, cwd=ROOT, stdout=stream,
                                        stderr=subprocess.STDOUT, timeout=300)
            trial["exit_code"] = result.returncode
            trial["after"] = snapshot()
            trial["issues"] += ["after: " + v for v in hardware_issues(trial["after"])]
            if result.returncode:
                trial["issues"].append("viewer process failed")
                continue
            trial["receipt_sha256"] = sha(receipt)
            data = json.loads(receipt.read_text(encoding="utf-8"))
            require(data["build_git_revision"] == ENGINE and data["tool_sha256"] == TOOL_SHA and
                    data["native_sha256"] == report["native_sha256"] and
                    data["source_tree_sha256"] == report["source_tree_sha256"] and
                    data["sha256"] == FIXTURE_SHA, "trial identity differs from the protocol")
            require(data["window_logical_size"] == [2560, 1440] and
                    data["scale_cycle"] is True and data["ui_multiplier"] == 1,
                    "trial arguments differ from the frozen workload")
            require([v["name"] for v in data["runs"]] == ["first-view", "repeat-view"],
                    "trial does not contain both expected views")
            for view in data["runs"]:
                trial["issues"] += [view["name"] + ": " + v for v in
                                    inspect_view(view, phases, report["source_tree_sha256"])]
        report["navigation_target_accepted"] = len(report["trials"]) == 3 and all(
            not t["issues"] for t in report["trials"])
    except Exception as error:
        report["failure"] = str(error)
    finally:
        report["finished_utc"] = datetime.now(timezone.utc).isoformat()
        path = output / "qualification.json"
        path.write_bytes((json.dumps(report, indent=2) + "\n").encode("utf-8"))
        print(json.dumps(dict(receipt=str(path), accepted=report["navigation_target_accepted"],
                              failure=report.get("failure"))))
    return 0 if report["navigation_target_accepted"] else 1


if __name__ == "__main__":
    raise SystemExit(main())
