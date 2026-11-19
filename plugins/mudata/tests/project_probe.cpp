// SPDX-License-Identifier: AGPL-3.0-only
#include <qupathxx/expression_matrix.hpp>
#include <qupathxx/expression_matrix_project.hpp>
#include <qupathxx/format_import.hpp>
#include <qupathxx/project.hpp>
#include <qupathxx/reader_plugins.hpp>
#include <cstring>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string_view>

void check(bool pass, const char* message) {
    if (!pass) throw std::runtime_error(message);
}

void scale(const qpx::ExpressionMatrixSource& reader, bool empty) {
    const auto& m = reader.metadata();
    check(m.input_profile == "external-sparse-file/1" && m.value_domain == "raw-counts" &&
          m.features == 12000 && m.observations == 12000 && m.entries == (empty ? 0 : 3),
          "Sparse scale dimensions or stored-entry count differ");
    check(reader.observation_page(6000, 1) == std::vector<std::string>{"spot-06000"},
          "Sparse scale observation identity differs");
    check(reader.feature_page(11999, 1).at(0).id == "gene-11999", "Sparse scale feature identity differs");
    check(reader.spatial_page(11999, 1).at(0).position.x == 1, "Sparse scale positions differ");
    qpx::ExpressionMatrixSelection pick;
    pick.features = {11999, 1, 0};
    const auto page = reader.read(pick);
    if (empty) {
        check(page.offsets == std::vector<std::uint64_t>{0, 0, 0, 0} && page.counts.empty() &&
              page.observation_indices.empty(), "Empty modality gained stored values");
    } else {
        check(page.offsets == std::vector<std::uint64_t>{0, 2, 2, 3} &&
              page.observation_indices == std::vector<std::uint64_t>{0, 11999, 6000} &&
              page.counts == std::vector<std::uint64_t>{0, UINT64_MAX, UINT64_C(9007199254740993)},
              "Sparse scale exact entries, including the explicit zero, differ");
    }
}

void anndata_wide(const qpx::ExpressionMatrixSource& reader) {
    const auto& m = reader.metadata();
    const auto receipt = qpx::external_expression_matrix_store_receipt(reader);
    check(m.input_profile == "external-sparse-store/1" && m.value_domain == "raw-counts" &&
          m.features == 10240 && m.observations == 10240 && m.entries == 10240 &&
          receipt && receipt->primary_key == ".zattrs" &&
          receipt->matrix.reader.provider.provider_id == "org.qupathxx.anndata-zarr",
          "Unexpected AnnData shape, sparse profile or store receipt");
    check(reader.observation_page(10239, 1) == std::vector<std::string>{"axis-10239"} &&
          reader.feature_page(0, 1).at(0).id == "axis-00000", "Unexpected AnnData axes");
    const auto spot = reader.spatial_page(10239, 1).at(0);
    check(spot.position.x == 3 && spot.position.y == 0, "Unexpected AnnData coordinate join");
    qpx::ExpressionMatrixSelection pick;
    pick.features = {10239, 0, 1};
    const auto page = reader.read(pick);
    check(page.offsets == std::vector<std::uint64_t>{0, 1, 2, 3} &&
          page.observation_indices == std::vector<std::uint64_t>{10239, 0, 1} &&
          page.counts == std::vector<std::uint64_t>{10240, 1, 2}, "Unexpected owned AnnData counts");
}

int main(int argc, char** argv) {
    try {
        const std::string_view action = argc > 1 ? argv[1] : "";
        if (argc == 7 && (action == "--owned-scale" || action == "--owned-anndata-wide")) {
            qpx::configure_reader_plugins(argv[6]);
            const auto project = qpx::Project::open_without_journal_recovery(argv[2]);
            const auto image = std::stoull(argv[3]);
            const auto association = qpx::load_expression_matrix_association(*project, image, argv[4]);
            check(association.owned && association.file_sha256 == argv[5],
                  "Attached project matrix does not have the exact owned identity");
            const auto reader = qpx::reopen_expression_matrix(*project, image, argv[4], 64u * 1024 * 1024);
            association.require_source(*reader);
            const auto& frame = reader->metadata().spatial_frame;
            check(frame && frame->width == 4 && frame->height == 3 &&
                  qpx::image_coordinate_frame(*project->document(image), frame->id, frame->plane).sha256() == frame->sha256(),
                  "Attached matrix frame differs from the saved image");
            if (action == "--owned-scale") scale(*reader, false);
            else anndata_wide(*reader);
            std::cout << "Verified attached sparse values and saved ENVI image frame\n";
            return 0;
        }
        check(argc == 4, "Expected QMX, file digest and oracle");
        const auto reader = qpx::open_expression_matrix(argv[1], argv[2]);
        if (std::string_view mode(argv[3]); mode == "scale" || mode == "empty-scale") {
            const auto receipt = qpx::external_expression_matrix_receipt(*reader);
            check(receipt && receipt->reader.provider.provider_id == "org.qupathxx.mudata" &&
                  receipt->reader.provider.abi == 5 && !qpx::external_expression_matrix_store_receipt(*reader),
                  "Sparse scale single-file receipt differs");
            scale(*reader, mode == "empty-scale");
            std::cout << "Verified offline sparse scale QMX\n";
            return 0;
        }
        std::ifstream oracle(argv[3], std::ios::binary);
        check(bool(oracle), "Cannot open oracle");
        std::string expected_options, domain, line;
        std::getline(oracle, expected_options);
        std::getline(oracle, domain);
        std::size_t rows = 0, columns = 0;
        oracle >> rows >> columns;
        std::getline(oracle, line);
        std::string profile;
        std::getline(oracle, profile);
        std::size_t entries = 0;
        oracle >> entries;
        std::getline(oracle, line);
        const auto& metadata = reader->metadata();
        const bool counts = domain == "raw-counts";
        check(metadata.domain == (counts ? qpx::ExpressionValueDomain::Counts : qpx::ExpressionValueDomain::Continuous) &&
              metadata.value_domain == domain && metadata.observations == rows && metadata.features == columns &&
              metadata.entries == entries && metadata.input_profile == profile,
              "Unexpected MuData indexed matrix type or shape");
        const auto receipt = qpx::external_expression_matrix_receipt(*reader);
        check(receipt && receipt->reader.provider.provider_id == "org.qupathxx.mudata" &&
              receipt->reader.options == expected_options, "MuData selection receipt differs");
        const auto observations = reader->observation_page(0, rows);
        const auto features = reader->feature_page(0, columns);
        const auto positions = reader->spatial_page(0, rows);
        check(observations.size() == rows && features.size() == columns && positions.size() == rows,
              "MuData axis pages differ");
        for (const auto& id : observations) {
            std::getline(oracle, line);
            check(line == id, "Observation ID differs");
        }
        for (const auto& f : features) {
            std::getline(oracle, line);
            check(line == f.id, "Feature ID differs");
        }
        for (const auto& spot : positions) {
            double x, y;
            oracle >> x >> y;
            check(bool(oracle) && spot.position.x == x && spot.position.y == y, "Position join differs");
        }
        qpx::ExpressionMatrixSelection selection;
        for (auto f = columns; f; --f) selection.features.push_back(f - 1);
        const auto page = reader->read(selection);
        check((counts ? page.counts.size() : page.continuous.size()) == entries, "Unexpected page size");
        check(page.observation_indices.size() == entries && page.offsets.size() == columns + 1, "Unexpected page layout");
        std::size_t at = 0;
        for (std::size_t f = 0; f < columns; ++f) {
            check(page.offsets[f] == at, "Unexpected feature offset");
            std::size_t feature_entries = 0;
            oracle >> std::dec >> feature_entries;
            check(bool(oracle) && feature_entries <= entries - at, "Invalid oracle entry count");
            for (std::size_t i = 0; i < feature_entries; ++i, ++at) {
                std::size_t o = 0;
                std::uint64_t bits = 0, expected = 0;
                oracle >> std::dec >> o;
                oracle >> std::hex >> expected;
                check(page.observation_indices[at] == o, "Unexpected matrix coordinate");
                if (counts) bits = page.counts[at];
                else std::memcpy(&bits, &page.continuous[at], sizeof(bits));
                check(bool(oracle) && bits == expected, "Unexpected MuData exact sample bits");
            }
        }
        check(page.offsets.back() == entries && at == entries, "Unexpected end of matrix");
        std::cout << "Verified offline MuData samples, modality axes and joined positions\n";
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
