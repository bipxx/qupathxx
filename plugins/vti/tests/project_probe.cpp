// SPDX-License-Identifier: AGPL-3.0-only
#include <qupathxx/image_server.hpp>
#include <qupathxx/project.hpp>
#include <qupathxx/raw_image.hpp>
#include <qupathxx/reader_plugins.hpp>
#include <qupathxx/source_identity.hpp>
#include <filesystem>
#include <iostream>
#include <stdexcept>

int main(int argc, char** argv) {
    try {
        const bool vector = argc == 6 && std::string_view(argv[5]) == "--vector";
        if (argc != 5 && !vector) throw std::invalid_argument("Expected mode, manifest, project path and VTI path");
        const std::string mode = argv[1];
        if (mode != "create" && mode != "reopen") throw std::invalid_argument("Unknown probe mode");
        qpx::configure_reader_plugins(argv[2]);
        const bool create = mode == "create";
        auto project = create ? std::make_shared<qpx::Project>() :
                                qpx::Project::open_without_journal_recovery(argv[3]);
        const auto id = create ? project->add_image(argv[4], "org.qupathxx.vti") : 1;
        const auto server = project->document(id)->server();
        qpx::RawRegionRequest request;
        request.region = {0, 0, 0, 3, 2};
        request.first_channel = vector ? 1 : 0;
        request.channels = vector ? 2 : 1;
        request.z = 1;
        const auto raw = server->read_raw(request);
        std::vector<std::uint16_t> expected;
        for (std::uint16_t pixel = 6; pixel < 12; ++pixel) {
            if (vector) {
                expected.push_back(static_cast<std::uint16_t>(pixel + 8192));
                expected.push_back(static_cast<std::uint16_t>(60000 - pixel * 17));
            } else expected.push_back(pixel);
        }
        if (raw.valid() != std::vector<std::uint8_t>(6, 1) ||
            std::get<std::vector<std::uint16_t>>(raw.storage()) != expected)
            throw std::runtime_error("VTI raw samples differ");
        const auto meta = server->raw_metadata();
        const auto names = vector ? std::vector<std::string>{"Horizontal", "Research samples [2]", "Depth"} :
                                    std::vector<std::string>{"Research samples"};
        if (!meta || meta->z_planes != 2 || meta->pixel_width_um != 0.5 ||
            meta->pixel_height_um != 1.5 || meta->z_spacing_um != 3.0 ||
            meta->channel_names != names || meta->dimension_order != (vector ? "CXYZ" : "XYZ"))
            throw std::runtime_error("VTI calibration, Z extent or scalar name differs");
        const auto observed = server->reader_source();
        if (!observed || observed->scope != "single_file")
            throw std::runtime_error("Expected one VTI source file");
        if (create) project->save_new(argv[3]);
        std::cout << mode << " passed exact Z1 data and millimetre calibration\n";
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
