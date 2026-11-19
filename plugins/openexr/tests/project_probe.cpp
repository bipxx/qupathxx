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
        if (argc != 6) throw std::invalid_argument("Expected mode, manifest, project, OpenEXR source and profile");
        const std::string mode = argv[1], profile = argv[5];
        if ((mode != "create" && mode != "reopen") || (profile != "rgb" && profile != "rgba"))
            throw std::invalid_argument("Unknown OpenEXR test mode or profile");
        qpx::configure_reader_plugins(argv[2]);
        const bool create = mode == "create";
        auto project = create ? std::make_shared<qpx::Project>() :
                                qpx::Project::open_without_journal_recovery(argv[3]);
        const auto id = create ? project->add_image(argv[4], "org.qupathxx.openexr") : 1;
        const auto reader = project->document(id)->server();
        qpx::RawRegionRequest request;
        const bool alpha = profile == "rgba";
        request.region = {alpha ? 1 : 0, 0, 0, 1, alpha ? 2 : 1};
        request.channels = alpha ? 4 : 3;
        const auto raw = reader->read_raw(request);
        const auto expected = alpha ? std::vector<float>{0.5f, 0.125f, 0.25f, 0.5f, 4, 2, 0.5f, 0} :
                                      std::vector<float>{1.0f / 1024, -1.0f / 2048, 1.0f / 4096};
        if (raw.valid() != std::vector<std::uint8_t>(alpha ? 2 : 1, 1) ||
            std::get<std::vector<float>>(raw.storage()) != expected)
            throw std::runtime_error("OpenEXR linear samples or validity differ");
        const auto meta = reader->raw_metadata();
        const auto source = reader->reader_source();
        std::vector<std::string> names{"Red", "Green", "Blue"};
        if (alpha) names.emplace_back("Alpha");
        if (!meta || meta->channel_names != names ||
            meta->pixel_width_um || meta->pixel_height_um || !source || source->scope != "single_file" ||
            reader->metadata().vendor != (alpha ? "OpenEXR (linear associated RGBA)" : "OpenEXR (linear)"))
            throw std::runtime_error("OpenEXR source identity or raw metadata differs");
        const auto expected_display = alpha ? std::vector<std::uint8_t>{188, 99, 137, 128, 0, 0, 0, 0} :
                                              std::vector<std::uint8_t>{3, 0, 1, 255};
        if (reader->read_region(request.region).bytes() != expected_display)
            throw std::runtime_error("OpenEXR preview mapping differs");
        if (create) project->save_new(argv[3]);
        std::cout << mode << " passed exact linear " << profile << ", preview and source identity\n";
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
