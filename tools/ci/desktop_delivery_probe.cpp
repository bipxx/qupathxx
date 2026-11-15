// Development-only installed-route check. All interpretation and export use
// the public native services; this is not a second scientific implementation.
#include "qupathxx/expression_differential.hpp"
#include "qupathxx/project.hpp"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#ifdef _WIN32
#define NOMINMAX
#include <windows.h>
#endif

namespace fs = std::filesystem;
using namespace qpx;
namespace {
void check(bool value, const char* message) {
    if (!value) throw std::runtime_error(message);
}
int probe(const fs::path& input, const fs::path& output) {
    check(!fs::exists(output), "Delivery output must be a new directory");
    auto project = Project::open(input.u8string());
    const auto image = project->active_image();
    const auto ids = expression_differential_ids(*project, image);
    check(ids.size() == 1, "Representative project must contain exactly one DE record");
    const auto result = expression_differential_open_request(*project, image, ids.front()).open(64u*1024*1024);
    fs::create_directory(output);
    project->save_new((output/"study.qpx").u8string());
    project.reset();
    const auto reopened = Project::open((output/"study.qpx").u8string());
    check(expression_differential_ids(*reopened, image) == ids, "Saved result identity changed");
    const auto replay = expression_differential_open_request(*reopened, image, ids.front()).open(64u*1024*1024);
    check(replay->recipe() == result->recipe() && replay->sha256() == result->sha256(),
          "Relocated saved project changed the exact scientific result");
    save_expression_differential_csv(*replay, (output/"genes.csv").u8string());
    save_expression_differential_replicates_csv(*replay, (output/"replicates.csv").u8string());
    const auto& matrix = replay->parent()->parent();
    std::ofstream report(output/"result.txt", std::ios::binary);
    check(bool(report), "Cannot write delivery report");
    report << "record=" << replay->id() << '\n'
           << "result_sha256=" << replay->sha256() << '\n'
           << "source=" << matrix->metadata().source_sha256 << '\n'
           << "pseudobulk=" << replay->parent()->id() << '\n'
           << "features=" << replay->genes().size() << '\n'
           << "observations=" << matrix->metadata().observations << '\n'
           << "n_a=" << replay->n_a() << '\n' << "n_b=" << replay->n_b() << '\n'
           << "testable_features=" << replay->correction_features() << '\n'
           << "saved_replay=exact\n";
#ifdef _WIN32
    for (const auto* name : {L"geos.dll", L"geos_c.dll", L"libopenslide-1.dll"}) {
        const auto module = GetModuleHandleW(name);
        check(module != nullptr, "Required runtime module was not loaded");
        wchar_t path[32768];
        const auto length = GetModuleFileNameW(module, path, 32768);
        check(length > 0 && length < 32768, "Cannot resolve runtime module path");
        report << "module:" << fs::path(name).u8string() << '=' << fs::path(path).u8string() << '\n';
    }
#endif
    report.flush();
    check(bool(report), "Delivery report write failed");
    std::cout << "Native project relocation, exact saved replay and both CSV exports passed\n";
    return 0;
}
}
#ifdef _WIN32
int wmain(int argc, wchar_t** argv) {
#else
int main(int argc, char** argv) {
#endif
    try {
        if (argc != 3) throw std::invalid_argument("Expected PROJECT NEW_OUTPUT_DIRECTORY");
#ifdef _WIN32
        return probe(fs::path(argv[1]), fs::path(argv[2]));
#else
        return probe(fs::u8path(argv[1]), fs::u8path(argv[2]));
#endif
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
