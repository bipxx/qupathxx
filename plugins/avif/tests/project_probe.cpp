// SPDX-License-Identifier: AGPL-3.0-only
#include <qupathxx/image_server.hpp>
#include <qupathxx/project.hpp>
#include <qupathxx/raw_image.hpp>
#include <qupathxx/reader_plugins.hpp>
#include <qupathxx/source_identity.hpp>
#include <fstream>
#include <iostream>
#include <iterator>
#include <stdexcept>

int main(int argc, char** argv) {
    try {
        if (argc != 7) throw std::invalid_argument("Expected mode, manifest, project, image, raw reference and profile");
        const std::string mode = argv[1], profile = argv[6];
        if ((mode != "create" && mode != "reopen") || (profile != "native" && profile != "decoded"))
            throw std::invalid_argument("Unknown AVIF probe mode or profile");
        std::ifstream file(argv[5], std::ios::binary);
        if (!file) throw std::runtime_error("Cannot open AVIF reference");
        const std::vector<std::uint8_t> expected{std::istreambuf_iterator<char>(file), {}};
        if (expected.size() != 60) throw std::runtime_error("Invalid AVIF reference size");
        qpx::configure_reader_plugins(argv[2]);
        const bool create = mode == "create";
        auto project = create ? std::make_shared<qpx::Project>() :
                                qpx::Project::open_without_journal_recovery(argv[3]);
        const auto id = create ? project->add_image(argv[4], "org.qupathxx.avif") : 1;
        const auto server = project->document(id)->server();
        qpx::RawRegionRequest request;
        request.region = {0, 0, 0, 5, 4};
        request.channels = 3;
        const auto raw = server->read_raw(request);
        if (std::get<std::vector<std::uint8_t>>(raw.storage()) != expected ||
            raw.valid() != std::vector<std::uint8_t>(20, 1))
            throw std::runtime_error("AVIF native samples or validity differ");
        const auto meta = server->raw_metadata();
        const auto source = server->reader_source();
        if (!meta || meta->channel_names != std::vector<std::string>{"Red","Green","Blue"} ||
            meta->pixel_width_um || meta->pixel_height_um || !source || source->scope != "single_file" ||
            server->metadata().vendor != (profile == "decoded" ? "AVIF (decoded sRGB)" : "AVIF"))
            throw std::runtime_error("AVIF source identity or profile metadata differs");
        if (create) project->save_new(argv[3]);
        std::cout << mode << " passed exact AVIF output and source identity\n";
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
