// SPDX-License-Identifier: AGPL-3.0-only
#include <qupathxx/image_server.hpp>
#include <qupathxx/project.hpp>
#include <qupathxx/raw_image.hpp>
#include <qupathxx/reader_plugins.hpp>
#include <qupathxx/source_identity.hpp>
#include <array>
#include <filesystem>
#include <iostream>
#include <stdexcept>

namespace fs = std::filesystem;

void check(bool pass, const char* message) {
    if (!pass) throw std::runtime_error(message);
}

int main(int argc, char** argv) {
    try {
        const bool wide = argc == 5 && std::string_view(argv[4]) == "--wide-vector";
        const bool vector = wide || (argc == 5 && std::string_view(argv[4]) == "--vector");
        const auto option = argc == 5 ? std::string_view(argv[4]) : std::string_view{};
        std::array<const char*, 2> slice_stems{};
        if (option == "--numbered-slices") slice_stems = {"slice-03", "slice002"};
        if (option == "--slice-list") slice_stems = {"head", "tail"};
        if (option == "--spaced-list") slice_stems = {"first section", "last section"};
        if (option == "--spaced-numbered") slice_stems = {"scan 002 raw", "scan 005 raw"};
        const bool list = slice_stems[0] != nullptr;
        check(argc == 4 || vector || list, "Expected create, reopen or changed, manifest, and fixture directory");
        const std::string mode = argv[1];
        check(mode == "create" || mode == "reopen" || mode == "changed", "Unknown probe mode");
        const auto work = fs::u8path(argv[3]);
        const auto saved = (work / "study.qpxproj").u8string();
        auto project = mode == "create" ? std::make_shared<qpx::Project>() :
            qpx::Project::open_without_journal_recovery(saved);
        qpx::configure_reader_plugins(argv[2]);
        const auto id = mode == "create" ? project->add_image(
            (work / "study.mhd").u8string(), "org.qupathxx.metaimage") : 1;
        const auto server = project->document(id)->server();
        qpx::RawRegionRequest request;
        request.region = {0, 0, 0, 3, 2};
        request.first_channel = vector ? 1 : 0;
        request.channels = vector ? 2 : 1;
        request.z = 1;
        if (mode == "changed") {
            try {
                (void)server->read_raw(request);
            } catch (const std::exception& error) {
                const std::string message = error.what();
                check(message.find("source") != std::string::npos &&
                      message.find("changed") != std::string::npos, error.what());
                std::cout << message << '\n';
                return 0;
            }
            throw std::runtime_error("Reopened project accepted replacement companion");
        }
        const auto raw = server->read_raw(request);
        check(raw.valid() == std::vector<std::uint8_t>(6, 1), "Z1 validity differs");
        if (wide) {
            constexpr std::array<std::uint64_t, 7> values{0, 1, (1ull << 53) + 1, INT64_MAX,
                                                        1ull << 63, UINT64_MAX - 1, UINT64_MAX};
            std::vector<std::uint64_t> expected;
            for (unsigned pixel = 6; pixel < 12; ++pixel)
                for (unsigned component = 1; component <= 2; ++component)
                    expected.push_back(values[(pixel * 4 + component) % values.size()]);
            check(std::get<std::vector<std::uint64_t>>(raw.storage()) == expected, "Z1 wide components differ");
        } else {
            std::vector<std::uint16_t> expected;
            for (std::uint16_t pixel = 6; pixel < 12; ++pixel) {
                if (vector) {
                    expected.push_back(static_cast<std::uint16_t>(pixel * 257 + 8192));
                    expected.push_back(static_cast<std::uint16_t>(pixel * 257 + 32768));
                } else expected.push_back(pixel);
            }
            check(std::get<std::vector<std::uint16_t>>(raw.storage()) == expected, "Z1 samples or component order differ");
        }
        const auto meta = server->raw_metadata();
        check(meta && meta->width == 3 && meta->height == 2 && meta->z_planes == 2 &&
              meta->pixel_width_um == 0.5 && meta->pixel_height_um == 1.5 &&
              meta->z_spacing_um == 3.0, "Raw dimensions or calibration differ");
        const auto names = vector ? std::vector<std::string>{"Component 1", "Component 2", "Component 3", "Component 4"}
                                  : std::vector<std::string>{"Scalar"};
        check(meta->channel_names == names && meta->dimension_order == (vector ? "CXYZ" : "XYZ"),
              "Stored component metadata differs");
        const auto observed = server->reader_source();
        check(observed && observed->scope == "compound_files" &&
              observed->companions.size() == (list ? 2 : 1), "Detached source identity differs");
        if (list) {
            const auto head = fs::u8path(observed->companions[0].path);
            const auto tail = fs::u8path(observed->companions[1].path);
            check(head.parent_path() == work && tail.parent_path() == work &&
                  head.stem() == slice_stems[0] && tail.stem() == slice_stems[1],
                  "Slice companion identities differ");
        } else check(fs::equivalent(fs::u8path(observed->companions.front().path), work / "study.raw"),
                     "Detached source identity differs");
        if (mode == "create") project->save_new(saved);
        std::cout << mode << " verified exact samples, calibration and companion identity\n";
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
