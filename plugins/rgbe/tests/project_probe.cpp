// SPDX-License-Identifier: AGPL-3.0-only
#include <qupathxx/image_server.hpp>
#include <qupathxx/project.hpp>
#include <qupathxx/raw_image.hpp>
#include <qupathxx/reader_plugins.hpp>
#include <qupathxx/source_identity.hpp>
#include <iostream>
#include <stdexcept>

int main(int argc, char** argv) {
    try {
        if (argc != 5) throw std::invalid_argument("Expected mode, manifest, project and RGBE source");
        const std::string mode = argv[1];
        if (mode != "create" && mode != "reopen") throw std::invalid_argument("Unknown RGBE test mode");
        qpx::configure_reader_plugins(argv[2]);
        const bool create = mode == "create";
        auto project = create ? std::make_shared<qpx::Project>() :
                                qpx::Project::open_without_journal_recovery(argv[3]);
        const auto id = create ? project->add_image(argv[4], "org.qupathxx.rgbe") : 1;
        const auto reader = project->document(id)->server();
        qpx::RawRegionRequest request;
        request.region = {0, 0, 0, 2, 1};
        request.channels = 3;
        const auto raw = reader->read_raw(request);
        const std::vector<float> expected{257.0f / 256, 129.0f / 256, 65.0f / 256, 0, 0, 0};
        if (raw.valid() != std::vector<std::uint8_t>{1, 1} ||
            std::get<std::vector<float>>(raw.storage()) != expected)
            throw std::runtime_error("RGBE linear samples or validity differ");
        const auto meta = reader->raw_metadata();
        const auto source = reader->reader_source();
        if (!meta || meta->channel_names != std::vector<std::string>{"Red", "Green", "Blue"} ||
            meta->pixel_width_um || meta->pixel_height_um || !source || source->scope != "single_file" ||
            reader->metadata().vendor != "Radiance RGBE (linear)")
            throw std::runtime_error("RGBE source identity or raw metadata differs");
        if (reader->read_region(request.region).bytes() != std::vector<std::uint8_t>{188, 137, 100, 255, 0, 0, 0, 255})
            throw std::runtime_error("RGBE preview mapping differs");
        if (create) project->save_new(argv[3]);
        std::cout << mode << " passed exact linear values, preview and source identity\n";
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
