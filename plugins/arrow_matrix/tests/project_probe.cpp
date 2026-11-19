// SPDX-License-Identifier: AGPL-3.0-only
#include <qupathxx/expression_matrix.hpp>
#include <qupathxx/expression_matrix_project.hpp>
#include <qupathxx/format_import.hpp>
#include <qupathxx/project.hpp>
#include <qupathxx/reader_plugins.hpp>
#include <cstring>
#include <iostream>
#include <stdexcept>
#include <string_view>

void check(bool pass, const char* expected) {
    if (!pass)
        throw std::runtime_error(expected);
}

int main(int argc, char** argv) {
    try {
        check(argc == 6 || argc == 7 || argc == 9 || argc == 10,
              "Expected project, association ID, QMX digest, reader manifest and profile");
        qpx::configure_reader_plugins(argv[4]);
        const auto project = qpx::Project::open_without_journal_recovery(argv[1]);
        const auto attachment = qpx::load_expression_matrix_association(*project, 1, argv[2]);
        check(attachment.owned && attachment.file_sha256 == argv[3], "Owned matrix identity differs");
        const auto source = qpx::reopen_expression_matrix(*project, 1, argv[2], 64u * 1024 * 1024);
        attachment.require_source(*source);
        const bool counts = std::string_view(argv[5]) == "counts";
        const auto& metadata = source->metadata();
        check(metadata.observations == 3 && metadata.features == 2 && metadata.entries == 6 &&
                  metadata.value_domain == (counts ? "raw-counts" : "normalized-expression") &&
                  metadata.input_profile == "external-dense-typed/1", "Matrix contract differs");
        const auto receipt = qpx::external_expression_matrix_receipt(*source);
        const char* provider = argc >= 7 ? argv[6] : "org.qupathxx.arrow-matrix";
        check(receipt && receipt->reader.provider.provider_id == provider &&
                  receipt->reader.provider.abi == 2, "Persisted provider identity differs");
        const auto& frame = metadata.spatial_frame;
        check(frame && frame->width == 4 && frame->height == 3, "Saved measurement frame dimensions differ");
        const auto current_frame = qpx::image_coordinate_frame(*project->document(1), frame->id, frame->plane);
        if (current_frame.sha256() != frame->sha256())
            throw std::runtime_error("Saved measurement frame differs: " + frame->id +
                                     " stored=" + frame->sha256() + " current=" + current_frame.sha256());
        const std::vector<std::string> expected_ids = argc == 10
            ? std::vector<std::string>{argv[7], argv[8], argv[9]}
            : std::vector<std::string>{"0001", "1", "μ"};
        check(source->observation_page(0, 3) == expected_ids, "Spot IDs differ");
        const auto features = source->feature_page(0, 2);
        check(features.at(0).id == (argc == 9 ? argv[7] : "Gene.B") &&
                  features.at(1).id == (argc == 9 ? argv[8] : "Gene.A"), "Feature order differs");
        const auto positions = source->spatial_page(0, 3);
        check(positions.at(0).position.x == .5 && positions.at(0).position.y == .75 &&
                  positions.at(1).position.x == 1.25 && positions.at(1).position.y == 1.25 &&
                  positions.at(2).position.x == 3.5 && positions.at(2).position.y == 2.5, "Position join differs");
        qpx::ExpressionMatrixSelection selection;
        selection.features = {0, 1};
        const auto page = source->read(selection);
        check(page.offsets == std::vector<std::uint64_t>{0, 3, 6} &&
                  page.observation_indices == std::vector<std::uint64_t>{0, 1, 2, 0, 1, 2}, "Value order differs");
        if (counts) {
            check(page.counts == std::vector<std::uint64_t>{5, 0, 7, 0, 9007199254740993ull, UINT64_MAX},
                  "Reopened UInt64 samples differ");
        } else {
            const std::vector<double> expected = {0x1p-149, -7.5, 6.5, -0.0, 1.0 + 0x1p-52, 0x1p-1074};
            check(page.continuous.size() == expected.size() &&
                      std::memcmp(expected.data(), page.continuous.data(), expected.size() * sizeof(double)) == 0,
                  "Reopened Float64 sample bits differ");
        }
        std::cout << "Verified owned Arrow matrix values, IDs and saved-image positions\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
