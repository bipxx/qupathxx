// SPDX-License-Identifier: AGPL-3.0-only
#pragma once
#include "format_import_frame.hpp"
#include "qupathxx/registration.hpp"
#include "sha256.hpp"
#include <string_view>

namespace qpx::detail {
inline constexpr std::uint64_t tma_review_project_bytes=4ull*1024*1024*1024;

inline std::string tma_review_frame(const Document& document,const std::atomic<bool>* cancelled) {
    return image_coordinate_frame(document,indexed_matrix_frame_id(document),ObjectPlane{},cancelled).sha256();
}
inline std::string tma_proposal_review_digest(std::string_view project,std::uint64_t image,
                                             std::string_view frame,std::string_view proposal) {
    Sha256 hash;
    constexpr std::string_view schema="qpx.tma-proposal-review/1";
    hash.update(schema.data(),schema.size());
    hash.update(project.data(),project.size());
    hash.integer(image);
    hash.update(frame.data(),frame.size());
    hash.update(proposal.data(),proposal.size());
    return hash.finish();
}
}
