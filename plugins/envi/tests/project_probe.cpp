// SPDX-License-Identifier: AGPL-3.0-only
#include <qupathxx/image_server.hpp>
#include <qupathxx/project.hpp>
#include <qupathxx/raw_image.hpp>
#include <qupathxx/reader_plugins.hpp>
#include <qupathxx/source_identity.hpp>
#include <filesystem>
#include <iostream>
#include <stdexcept>

namespace fs = std::filesystem;

void check(bool pass, const char* message) {
    if (!pass) throw std::runtime_error(message);
}

int main(int argc, char** argv) {
    try {
        check(argc == 4, "Expected mode, manifest and fixture directory");
        const std::string mode = argv[1];
        check(mode == "create" || mode == "reopen" || mode == "changed", "Unknown probe mode");
        const auto work = fs::u8path(argv[3]);
        const auto saved = (work / "study.qpxproj").u8string();
        qpx::configure_reader_plugins(argv[2]);
        auto project = mode == "create" ? std::make_shared<qpx::Project>() :
            qpx::Project::open_without_journal_recovery(saved);
        const auto id = mode == "create" ? project->add_image((work / "study.hdr").u8string(), "org.qupathxx.envi") : 1;
        const auto server = project->document(id)->server();
        qpx::RawRegionRequest request;
        request.region = qpx::RegionRequest(/*x=*/1, /*y=*/1, /*level=*/0, 2, 1);
        request.first_channel = 1;
        request.channels = 2;
        if (mode == "changed") {
            try {
                (void)server->read_raw(request);
            } catch (const std::exception& error) {
                const std::string message = error.what();
                check(message.find("source") != std::string::npos && message.find("changed") != std::string::npos,
                      error.what());
                return 0;
            }
            throw std::runtime_error("Reopen accepted a replaced companion");
        }
        const auto pixels = server->read_raw(request);
        check(pixels.valid() == std::vector<std::uint8_t>{1, 1} &&
              std::get<std::vector<std::uint16_t>>(pixels.storage()) ==
                  std::vector<std::uint16_t>{16*137, 17*137, 19*137, 20*137}, "Wrong ENVI crop or channel order");
        request.region = qpx::RegionRequest(/*x=*/-1, /*y=*/1, /*level=*/0, 2, 1);
        const auto clipped = server->read_raw(request);
        check(clipped.valid() == std::vector<std::uint8_t>{0, 1} &&
              std::get<std::vector<std::uint16_t>>(clipped.storage()) ==
                  std::vector<std::uint16_t>{0, 0, 13*137, 14*137}, "Wrong ENVI clipped validity");
        const auto observed = server->reader_source();
        check(observed && observed->scope == "compound_files" && observed->companions.size() == 1 &&
              fs::equivalent(fs::u8path(observed->companions.front().path), work / "study.img"),
              "ENVI companion identity differs");
        if (mode == "create") project->save_new(saved);
        std::cout << mode << " verified ENVI crop and companion identity\n";
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
