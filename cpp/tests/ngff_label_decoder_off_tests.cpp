// SPDX-License-Identifier: AGPL-3.0-only
// Link alongside the unchanged workflow test's main and native assertions.
#include "qupathxx/ome_zarr.hpp"
#include <cstdio>
#include <cstdlib>
namespace {
struct RequireHostDecoderOff {
    RequireHostDecoderOff() {
        if (qpx::has_ome_zarr()) {
            std::fputs("The host builtin NGFF decoder must be disabled for this route\n", stderr);
            std::exit(EXIT_FAILURE);
        }
        std::fputs("Host builtin NGFF decoder OFF; optional provider owns decoding\n", stdout);
    }
} require_host_decoder_off;
} // namespace
