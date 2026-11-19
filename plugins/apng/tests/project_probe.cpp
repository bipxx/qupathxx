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
        if (argc != 5) throw std::invalid_argument("Expected mode, manifest, project and APNG source");
        const std::string mode = argv[1];
        if (mode != "create" && mode != "reopen") throw std::invalid_argument("Unknown APNG test mode");
        qpx::configure_reader_plugins(argv[2]);
        const bool create = mode == "create";
        auto project = create ? std::make_shared<qpx::Project>() : qpx::Project::open_without_journal_recovery(argv[3]);
        const auto id = create ? project->add_image(argv[4], "org.qupathxx.apng") : 1;
        const auto server = project->document(id)->server();
        qpx::RawRegionRequest request;
        request.region = {0, 0, 0, 1, 1};
        request.channels = 4;
        const auto raw = server->read_raw(request);
        const auto source = server->reader_source();
        const std::vector<std::uint8_t> red{255, 0, 0, 255};
        if (!source || source->scope != "single_file" || raw.valid() != std::vector<std::uint8_t>{1} ||
            std::get<std::vector<std::uint8_t>>(raw.storage()) != red || server->read_region(request.region).bytes() != red)
            throw std::runtime_error("APNG saved frame or source identity differs");
        if (create) project->save_new(argv[3]);
        std::cout << mode << " retained selected APNG frame and exact pixels\n";
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
