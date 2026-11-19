// SPDX-License-Identifier: AGPL-3.0-only
#include "label_cli.hpp"
#include "qupathxx/file_fingerprint.hpp"
#include "qupathxx/label_workflow.hpp"
#include "qupathxx/project.hpp"
#include "qupathxx/reader_plugins.hpp"
#include <filesystem>
#include <iostream>
#include <nlohmann/json.hpp>
#include <sstream>

namespace fs = std::filesystem;
using namespace qpx;
namespace {
void check(bool condition, const char* message) {
    if (!condition)
        throw std::runtime_error(message);
}
std::string sha(const fs::path& path) {
    return fingerprint_file(path.u8string())->sha256;
}
nlohmann::json cli(const std::vector<std::string>& args) {
    std::ostringstream out, err;
    struct Restore {
        std::streambuf *o, *e;
        ~Restore() {
            std::cout.rdbuf(o);
            std::cerr.rdbuf(e);
        }
    } restore{std::cout.rdbuf(out.rdbuf()), std::cerr.rdbuf(err.rdbuf())};
    const std::atomic<bool> cancelled{false};
    const auto status = detail::label_cli(args, cancelled);
    if (status != 0 || !err.str().empty())
        throw std::runtime_error("Retained label CLI failed: " + err.str());
    return nlohmann::json::parse(out.str());
}
void replay(const fs::path& project_path, const fs::path& output,
            const std::string& id, const std::string& project_sha,
            const std::string& csv_sha, const std::string& raster_sha = {}) {
    check(sha(project_path) == project_sha, "Retained reference project changed");
    const auto project = Project::open_without_journal_recovery(project_path.u8string());
    const auto doc = project->document(1);
    const auto run = run_label_analysis(replay_label_analysis(*doc, id));
    check(run->sha256() == id, "Retained result identity changed");
    require_current_label_analysis_inputs(*run);
    const auto csv = output.u8string() + ".csv";
    save_label_analysis_csv(*run, csv);
    check(sha(csv) == csv_sha, "Retained native CSV bytes changed");
    const auto report = cli({"batch", "--verify-labels", project_path.u8string(),
                             "--image", "1", "--record", id});
    check(report.at("record") == id && report.at("regions") == 2 &&
              report.at("state") == "verified",
          "Retained CLI verification changed");
    const auto cli_csv = output.u8string() + "-cli.csv";
    cli({"batch", "--export-labels", project_path.u8string(), cli_csv,
         "--image", "1", "--record", id});
    check(sha(cli_csv) == csv_sha, "Retained CLI CSV bytes changed");
    if (!raster_sha.empty()) {
        const auto raster = output.u8string() + ".qpl";
        save_label_analysis_raster(*run, raster);
        check(sha(raster) == raster_sha, "Retained QPL archive bytes changed");
    }
    check(sha(project_path) == project_sha, "Replay changed the retained project");
}
} // namespace

int main(int argc, char** argv) {
    try {
        check(argc == 5, "Usage: label_replay_compat_tests TIFF_PROJECT PNG_PROJECT PNG_MANIFEST NEW_DIRECTORY");
        configure_reader_plugins(argv[3]);
        const auto output = fs::absolute(fs::u8path(argv[4]));
        check(!fs::exists(output), "Preserve prior compatibility evidence");
        fs::create_directories(output);
        replay(fs::absolute(fs::u8path(argv[1])), output / "tiff",
               "48058acacffc9a32ce3da2b2498be99d7c05d4aec5a3645c5a1091e6cceb7046",
               "93a879925a12ec6f8c944f81f28a7780ae90fa2ba2e98964dc10337b59fc6dc6",
               "12dba940a9c17894fd7f86394b5538ff9459972f6d264c0948c2cf8679d9de41");
        replay(fs::absolute(fs::u8path(argv[2])), output / "png",
               "d7f4380800a43d090ab8aa053a1f0c32f4bace6f32f5d9744eb8b526bed65f2d",
               "6a7841333bcc0b0341817a3b70eb1345e0b713ec554a8cba6590f45a3051f0cf",
               "331c39f8d80e8360374258143fa6decdcb7e0af0ccd314d212242731dfdf87b3",
               "f416f6d76f6dbb0d936c261756ba4b95ad9cff7c689e06939412a77987d16c72");
        std::cout << "Retained TIFF/PNG exact result IDs, native/CLI CSV and PNG QPL bytes pass\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
