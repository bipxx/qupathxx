#include "label_cli.hpp"
#include "qupathxx/label_workflow.hpp"
#include "qupathxx/project.hpp"
#include "export_output.hpp"
#include "json_string.hpp"
#include <charconv>
#include <iostream>
#include <set>

namespace qpx::detail {
namespace {
std::uint64_t integer(const std::string& s, std::uint64_t max = UINT64_MAX) {
    std::uint64_t n = 0;
    const auto p = std::from_chars(s.data(), s.data() + s.size(), n);
    if (s.empty() || p.ec != std::errc{} || p.ptr != s.data() + s.size() || n > max)
        throw std::invalid_argument("Invalid label option integer: " + s);
    return n;
}
double real(const std::string& s) {
    double n = 0;
    const auto p = std::from_chars(s.data(), s.data() + s.size(), n);
    if (s.empty() || p.ec != std::errc{} || p.ptr != s.data() + s.size() || !std::isfinite(n))
        throw std::invalid_argument("Invalid finite label option: " + s);
    return n;
}
const char* method_name(LabelAnalysisMethod method) {
    switch (method) {
    case LabelAnalysisMethod::Superpixels:
        return "slic";
    case LabelAnalysisMethod::Subcellular:
        return "subcellular";
    case LabelAnalysisMethod::ExternalMask:
        return "external-mask";
    }
    throw std::logic_error("Unknown label method");
}
} // namespace
std::optional<int> label_cli(const std::vector<std::string>& args,
                             const std::atomic<bool>& cancelled) {
    if (args.size() < 2)
        return {};
    const auto& command = args[1];
    const bool slic = command == "--slic", spots = command == "--subcellular",
               import = command == "--import-label-mask", input = command == "--label-input";
    const bool list = command == "--list-labels", verify = command == "--verify-labels",
               exporting = command == "--export-labels", creating = slic || spots || import;
    if (!creating && !input && !list && !verify && !exporting)
        return {};
    const std::size_t positional = creating || exporting ? 4 : 3;
    if (args.size() < positional)
        throw std::invalid_argument(
            "Label commands need PROJECT and writing commands need a new OUTPUT; see --help");
    std::uint64_t image = 0, roi = 0;
    std::vector<std::uint64_t> cells;
    RawRegionRequest channels;
    SuperpixelOptions superpixels;
    SubcellularOptions subcellular;
    ExternalLabelMaskOptions external;
    LabelAnalysisLimits limits;
    std::string record, format = "csv";
    bool record_only = false;
    std::size_t csv_bytes = 256u * 1024 * 1024, save_bytes = 512u * 1024 * 1024;
    std::uint64_t project_bytes = 1024u * 1024 * 1024;
    std::set<std::string> seen;
    for (std::size_t i = positional; i < args.size(); ++i) {
        const auto flag = args[i];
        if (!seen.insert(flag).second && !(spots && flag == "--cell"))
            throw std::invalid_argument("Duplicate label option: " + flag);
        if (creating && flag == "--record-only") {
            record_only = true;
            continue;
        }
        const bool common = flag == "--image" || (!list && flag == "--work-bytes") ||
                            ((verify || exporting) && flag == "--record");
        const bool area = (creating || input) && flag == "--roi";
        const bool raw =
            (spots || import || input) && (flag == "--first-channel" || flag == "--channels" ||
                                           flag == "--z" || flag == "--time");
        const bool super =
            slic && (flag == "--region-size" || flag == "--iterations" || flag == "--compactness" ||
                     flag == "--color-space" || flag == "--max-assignment-tests");
        const bool spot =
            spots && (flag == "--cell" || flag == "--channel" || flag == "--threshold" ||
                      flag == "--sigma-x" || flag == "--sigma-y" ||
                      flag == "--background-sigma-x" || flag == "--background-sigma-y" ||
                      flag == "--min-pixels" || flag == "--max-pixels" || flag == "--connectivity");
        const bool mask =
            import && (flag == "--mask" || flag == "--producer" || flag == "--producer-version" ||
                       flag == "--model-sha256" || flag == "--input-sha256");
        const bool cap = creating && (flag == "--max-regions" || flag == "--save-snapshot-bytes" ||
                                      flag == "--project-bytes");
        const bool output = exporting && (flag == "--format" || flag == "--csv-bytes");
        if (!common && !area && !raw && !super && !spot && !mask && !cap && !output)
            throw std::invalid_argument("Unknown option for label command: " + flag);
        if (++i == args.size())
            throw std::invalid_argument("Missing label option value: " + flag);
        const auto& v = args[i];
        if (flag == "--record") {
            record = v;
            continue;
        }
        if (flag == "--format") {
            format = v;
            continue;
        }
        if (mask) {
            if (flag == "--mask")
                external.path = export_path(v);
            else if (flag == "--producer")
                external.producer = v;
            else if (flag == "--producer-version")
                external.producer_version = v;
            else if (flag == "--model-sha256")
                external.model_sha256 = v;
            else
                external.input_sha256 = v;
            continue;
        }
        if (flag == "--color-space") {
            if (v == "srgb-lab")
                superpixels.color_space = SuperpixelColorSpace::LabSRGB;
            else if (v == "encoded-rgb")
                superpixels.color_space = SuperpixelColorSpace::EncodedRGB;
            else
                throw std::invalid_argument("SLIC color space must be srgb-lab or encoded-rgb");
            continue;
        }
        if (flag == "--compactness") {
            superpixels.compactness = real(v);
            continue;
        }
        if (flag == "--threshold" || flag == "--sigma-x" || flag == "--sigma-y" ||
            flag == "--background-sigma-x" || flag == "--background-sigma-y") {
            const auto n = real(v);
            if (flag == "--threshold")
                subcellular.threshold = n;
            else if (flag == "--sigma-x")
                subcellular.sigma_x = n;
            else if (flag == "--sigma-y")
                subcellular.sigma_y = n;
            else if (flag == "--background-sigma-x")
                subcellular.background_sigma_x = n;
            else
                subcellular.background_sigma_y = n;
            continue;
        }
        const auto n = integer(v);
        if (flag == "--image")
            image = n;
        else if (flag == "--roi")
            roi = n;
        else if (flag == "--cell") {
            if (!n || cells.size() == 10000)
                throw std::invalid_argument("Select1..10000 positive distinct cell IDs");
            cells.push_back(n);
        } else if (flag == "--work-bytes") {
            if (n > 1024ull * 1024 * 1024)
                throw std::invalid_argument("Label work limit exceeds1GiB");
            limits.work_bytes = std::size_t(n);
        } else if (flag == "--save-snapshot-bytes" || flag == "--project-bytes" ||
                   flag == "--csv-bytes") {
            if (!n || n > 4ull * 1024 * 1024 * 1024)
                throw std::invalid_argument("Label output limit must be1 byte..4GiB");
            if (flag == "--save-snapshot-bytes")
                save_bytes = std::size_t(n);
            else if (flag == "--project-bytes")
                project_bytes = n;
            else
                csv_bytes = std::size_t(n);
        } else if (flag == "--max-assignment-tests")
            superpixels.max_assignment_tests = n;
        else if (flag == "--min-pixels")
            subcellular.min_pixels = n;
        else if (flag == "--max-pixels")
            subcellular.max_pixels = n;
        else if (flag == "--max-regions") {
            if (n > 1000000)
                throw std::invalid_argument("Label region limit exceeds1000000");
            superpixels.max_regions = subcellular.max_spots = external.max_regions = std::size_t(n);
        } else {
            if (n > UINT32_MAX)
                throw std::invalid_argument("Label axis/algorithm integer exceedsUInt32");
            if (flag == "--first-channel")
                channels.first_channel = std::uint32_t(n);
            else if (flag == "--channels")
                channels.channels = std::uint32_t(n);
            else if (flag == "--z")
                channels.z = std::uint32_t(n);
            else if (flag == "--time")
                channels.t = std::uint32_t(n);
            else if (flag == "--region-size")
                superpixels.region_size = std::uint32_t(n);
            else if (flag == "--iterations")
                superpixels.iterations = std::uint32_t(n);
            else if (flag == "--channel")
                subcellular.channel = std::uint32_t(n);
            else if (flag == "--connectivity") {
                if (n != 4 && n != 8)
                    throw std::invalid_argument("Spot connectivity must be4 or8");
                subcellular.connectivity =
                    n == 4 ? SpotConnectivity::Four : SpotConnectivity::Eight;
            }
        }
    }
    if (!image || ((creating || input) && !roi) || ((verify || exporting) && record.empty()) ||
        (spots && cells.empty()))
        throw std::invalid_argument(
            "Label commands need positive --image and capture needs --roi; spots also need "
            "explicit --cell IDs and replay/export needs --record");
    if (format != "csv" && format != "raster" && format != "counts")
        throw std::invalid_argument("Label export format must be csv, raster or counts");
    limits.validate();
    superpixels.work_bytes = subcellular.work_bytes = limits.work_bytes;
    if (slic)
        superpixels.validate();
    if (spots)
        subcellular.validate();
    if (import)
        external.validate();
    try {
        workflow_checkpoint(&cancelled);
        const auto source = export_path(args[2]);
        ReaderFileGuard guard(source);
        const auto project = Project::open_without_journal_recovery(source);
        const auto doc = project->document(image);
        std::vector<std::string> protected_paths{source, source + "-journal", source + "-wal",
                                                 source + "-shm"};
        for (const auto& entry : project->images())
            if (!entry.embedded && !entry.source_path.empty())
                protected_paths.push_back(entry.source_path);
        if (import)
            protected_paths.push_back(external.path);
        const auto output = creating || exporting ? export_path(args[3]) : std::string{};
        std::shared_ptr<const LabelAnalysisRun> run;
        const auto current = [&] {
            workflow_checkpoint(&cancelled);
            guard.verify();
            if (run)
                require_current_label_analysis_inputs(*run, &cancelled);
            if (!output.empty())
                guard_export_output(output, protected_paths);
        };
        current();
        if (!output.empty())
            require_new_export(output);
        if (list) {
            const auto ids = label_analysis_ids(*doc);
            current();
            std::cout
                << "{\"schema\":\"qpx.label-analysis-cli/1\",\"state\":\"records\",\"records\":[";
            for (std::size_t i = 0; i < ids.size(); ++i) {
                if (i)
                    std::cout << ',';
                std::cout << json_string(ids[i]);
            }
            std::cout << "]}\n";
            return 0;
        }
        if (input) {
            const auto info = label_analysis_input_info(*doc, roi, channels, limits, &cancelled);
            const auto& r = info.raw;
            current();
            std::cout
                << "{\"schema\":\"qpx.label-analysis-cli/1\",\"state\":\"input\",\"input_sha256\":"
                << json_string(info.sha256)
                << ",\"sample_type\":" << json_string(sample_name(info.sample_type))
                << ",\"x\":" << r.region.x << ",\"y\":" << r.region.y
                << ",\"width\":" << r.region.width << ",\"height\":" << r.region.height
                << ",\"first_channel\":" << r.first_channel << ",\"channels\":" << r.channels
                << ",\"z\":" << r.z << ",\"time\":" << r.t << "}\n";
            return 0;
        }
        std::vector<std::uint64_t> published;
        if (verify || exporting) {
            run = run_label_analysis(replay_label_analysis(*doc, record, limits), &cancelled);
            current();
            if (exporting) {
                if (format == "raster")
                    save_label_analysis_raster(*run, output, protected_paths, &cancelled);
                else if (format == "counts")
                    save_subcellular_counts_csv(*run, output, csv_bytes, protected_paths,
                                                &cancelled);
                else
                    save_label_analysis_csv(*run, output, csv_bytes, protected_paths, &cancelled);
            }
        } else {
            const auto request =
                slic ? LabelAnalysisRequest(*doc, roi, superpixels, limits)
                : spots
                    ? LabelAnalysisRequest::subcellular(*doc, roi, std::move(cells), channels,
                                                        subcellular, limits)
                    : LabelAnalysisRequest::external_mask(*doc, roi, channels, external, limits);
            run = run_label_analysis(request, &cancelled);
            current();
            record = keep_label_analysis(*doc, request, *run, &cancelled);
            if (!record_only)
                published = publish_label_analysis(*doc, request, *run, "Label", &cancelled);
            current();
            project->save_new(output, save_bytes, project_bytes, &cancelled, current);
        }
        current();
        const auto& report = run->report();
        std::cout << "{\"schema\":\"qpx.label-analysis-cli/1\",\"state\":"
                  << json_string(verify      ? "verified"
                                 : exporting ? "exported"
                                             : "saved")
                  << ",\"method\":" << json_string(method_name(run->method()))
                  << ",\"record\":" << json_string(record)
                  << ",\"regions\":" << run->raster().regions().size()
                  << ",\"published_objects\":" << published.size()
                  << ",\"parent_cells\":" << run->cell_counts().size()
                  << ",\"input_sha256\":" << json_string(run->input_sha256())
                  << ",\"raster_sha256\":" << json_string(run->raster().sha256())
                  << ",\"value_domain\":" << json_string(run->measurement_domain())
                  << ",\"rejected_small\":" << report.rejected_small
                  << ",\"rejected_large\":" << report.rejected_large
                  << ",\"nonfinite_pixels\":" << report.nonfinite_pixels
                  << ",\"invalid_pixels\":" << report.invalid_pixels << "}\n";
        return 0;
    } catch (const std::exception& e) {
        std::cerr << e.what() << '\n';
        return cancelled.load() ? 130 : 1;
    }
}
} // namespace qpx::detail
