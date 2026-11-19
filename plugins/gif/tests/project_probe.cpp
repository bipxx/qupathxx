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
        if (argc != 5) throw std::invalid_argument("Expected mode, manifest, project and GIF paths");
        const std::string mode = argv[1];
        if (mode != "create" && mode != "reopen") throw std::invalid_argument("Unknown GIF test mode");
        qpx::configure_reader_plugins(argv[2]);
        const bool create = mode == "create";
        auto project = create ? std::make_shared<qpx::Project>() :
                                qpx::Project::open_without_journal_recovery(argv[3]);
        const auto id = create ? project->add_image(argv[4], "org.qupathxx.gif") : 1;
        const auto reader = project->document(id)->server();
        qpx::RawRegionRequest request;
        request.region = {0, 0, 0, 4, 2};
        request.channels = 1;
        const auto pixels = reader->read_raw(request);
        if (pixels.valid() != std::vector<std::uint8_t>(8, 1) ||
            std::get<std::vector<std::uint8_t>>(pixels.storage()) != std::vector<std::uint8_t>{0,1,2,3,3,2,1,0})
            throw std::runtime_error("GIF palette indices or validity differ");
        const auto meta = reader->raw_metadata();
        const auto source = reader->reader_source();
        if (!meta || meta->channel_names != std::vector<std::string>{"Palette index"} ||
            meta->pixel_width_um || meta->pixel_height_um || !source || source->scope != "single_file")
            throw std::runtime_error("GIF raw metadata or identity differs");
        const auto rgba = reader->read_region({2,0,0,1,1});
        if (rgba.bytes() != std::vector<std::uint8_t>{0,255,0,0})
            throw std::runtime_error("GIF transparent palette color differs");
        if (create) project->save_new(argv[3]);
        std::cout << mode << " passed exact indices, transparency and source identity\n";
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
