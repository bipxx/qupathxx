// SPDX-License-Identifier: AGPL-3.0-only
#include <qupathxx/image_server.hpp>
#include <qupathxx/project.hpp>
#include <qupathxx/raw_image.hpp>
#include <qupathxx/reader_plugins.hpp>
#include <qupathxx/source_identity.hpp>
#include <fstream>
#include <iostream>
#include <iterator>
#include <numeric>
#include <stdexcept>

int main(int argc, char** argv) {
    try {
        if (argc < 5 || argc > 7)
            throw std::invalid_argument("Expected mode, manifest, project and image, and optional geometry or decoded reference");
        const bool rotated = argc == 6 && std::string(argv[5]) == "rot90";
        const bool decoded = argc == 7 && std::string(argv[5]) == "decoded";
        if (argc > 5 && !rotated && !decoded) throw std::invalid_argument("Unknown JPEG XL test control");
        const std::string mode = argv[1];
        if (mode != "create" && mode != "reopen") throw std::invalid_argument("Unknown JPEG XL test mode");
        qpx::configure_reader_plugins(argv[2]);
        const bool create = mode == "create";
        auto project = create ? std::make_shared<qpx::Project>() :
                                qpx::Project::open_without_journal_recovery(argv[3]);
        const auto id = create ? project->add_image(argv[4], "org.qupathxx.jpegxl") : 1;
        const auto reader = project->document(id)->server();
        qpx::RawRegionRequest request;
        const int width = rotated ? 2 : 4, height = rotated ? 4 : 2;
        request.region = {0, 0, 0, width, height};
        request.channels = 3;
        const auto region = reader->read_raw(request);
        std::vector<std::uint8_t> expected(24);
        std::iota(expected.begin(), expected.end(), std::uint8_t(0));
        if (rotated)
            expected = {12,13,14,0,1,2,15,16,17,3,4,5,18,19,20,6,7,8,21,22,23,9,10,11};
        if (decoded) {
            std::ifstream file(argv[6], std::ios::binary);
            if (!file) throw std::runtime_error("Cannot open the independent decoded reference");
            expected.assign(std::istreambuf_iterator<char>(file), {});
            if (expected.size() != 24 || reader->metadata().vendor != "JPEG XL (decoded sRGB)")
                throw std::runtime_error("JPEG XL decoded output is not identified");
        }
        if (std::get<std::vector<std::uint8_t>>(region.storage()) != expected ||
            region.valid() != std::vector<std::uint8_t>(8, 1))
            throw std::runtime_error("JPEG XL sample codes or validity differ");
        const auto meta = reader->raw_metadata();
        const auto source = reader->reader_source();
        if (!meta || meta->width != width || meta->height != height ||
            meta->channel_names != std::vector<std::string>{"Red","Green","Blue"} ||
            meta->pixel_width_um || meta->pixel_height_um || !source || source->scope != "single_file")
            throw std::runtime_error("JPEG XL metadata or identity differs");
        if (rotated) {
            request.region = {1, 2, 0, 1, 1};
            const auto crop = reader->read_raw(request);
            if (std::get<std::vector<std::uint8_t>>(crop.storage()) != std::vector<std::uint8_t>{6,7,8} ||
                reader->read_region(request.region).bytes() != std::vector<std::uint8_t>{6,7,8,255})
                throw std::runtime_error("Oriented JPEG XL crop differs");
        }
        if (decoded && reader->read_region({0, 0, 0, 1, 1}).bytes() !=
                           std::vector<std::uint8_t>{expected[0], expected[1], expected[2], 255})
            throw std::runtime_error("Decoded JPEG XL raw and display values differ");
        if (create) project->save_new(argv[3]);
        std::cout << mode << " passed exact sample codes and source identity\n";
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
