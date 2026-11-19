// SPDX-License-Identifier: AGPL-3.0-only
#include <qupathxx/expression_matrix.hpp>
#include <qupathxx/format_import.hpp>
#include <cstring>
#include <iostream>
#include <stdexcept>

int main(int argc, char** argv) {
    try {
        if (argc < 4) throw std::invalid_argument("Expected QMX, file hash and NPZ profile");
        const std::string profile = argv[3];
        const bool continuous = profile == "continuous";
        const bool small = profile == "small";
        if ((!continuous && !small && profile != "wide") || argc != (continuous ? 11 : 4))
            throw std::invalid_argument("Invalid NPZ oracle arguments");
        const auto source = qpx::open_expression_matrix(argv[1], argv[2]);
        const auto& meta = source->metadata();
        const auto expected_profile = continuous ? "external-dense-typed/1" : "external-dense-counts/1";
        if (meta.input_profile != expected_profile || meta.observations != 2 || meta.features != 3 || meta.entries != 6 ||
            !meta.spatial_frame || meta.spatial_frame->width != 8 || meta.spatial_frame->height != 6)
            throw std::runtime_error("Indexed NPZ dimensions or native frame differ");
        const auto features = source->feature_page(0, 3);
        if (source->observation_page(0, 2) != std::vector<std::string>{"0001", "1"} || features.size() != 3 ||
            features[0].id != "G-01" || features[1].id != "G1" || features[2].id != u8"\u03bc-feature")
            throw std::runtime_error("Indexed NPZ axis identity differs");
        qpx::ExpressionMatrixSelection select;
        select.features = {2, 0, 1};
        const auto page = source->read(select);
        if (page.offsets != std::vector<std::uint64_t>{0, 2, 4, 6} ||
            page.observation_indices != std::vector<std::uint64_t>{0, 1, 0, 1, 0, 1})
            throw std::runtime_error("Indexed NPZ entries differ");
        if (continuous) {
            if (meta.domain != qpx::ExpressionValueDomain::Continuous || meta.value_domain != argv[4] ||
                !page.counts.empty() || page.continuous.size() != 6)
                throw std::runtime_error("Indexed NPZ continuous domain differs");
            for (std::size_t i = 0; i < page.continuous.size(); ++i) {
                std::uint64_t bits;
                std::memcpy(&bits, &page.continuous[i], sizeof(bits));
                if (bits != std::stoull(argv[5 + i], nullptr, 16))
                    throw std::runtime_error("Indexed NPZ continuous value bits differ");
            }
        } else {
            const auto expected = small ? std::vector<std::uint64_t>{200, 0, 9, 17, 0, 53} :
                std::vector<std::uint64_t>{UINT64_MAX, 0, UINT64_C(9007199254740993), 17, 0, 53};
            if (page.counts != expected)
                throw std::runtime_error("Indexed NPZ exact counts or zero entries differ");
        }
        const auto positions = source->spatial_page(0, 2);
        if (positions.size() != 2 || positions[0].position.x != 1.25 || positions[0].position.y != 2.5 ||
            positions[1].position.x != 6 || positions[1].position.y != 4)
            throw std::runtime_error("Indexed NPZ position join differs");
        const auto receipt = qpx::external_expression_matrix_receipt(*source);
        if (!receipt || receipt->reader.provider.provider_id !=
            (continuous ? "org.qupathxx.continuous-npz" : "org.qupathxx.counts-npz"))
            throw std::runtime_error("Indexed NPZ provenance is absent");
        std::cout << "Offline QMX retained exact NPZ IDs, values, position join and provenance\n";
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
