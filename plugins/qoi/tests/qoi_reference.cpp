// SPDX-License-Identifier: AGPL-3.0-only
// Development-only driver. The exact MIT qoi.h is provided separately from the
// pinned reference lock; no reference implementation is linked into the reader.
#define QOI_IMPLEMENTATION
#define QOI_NO_STDIO
#include "qoi.h"
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <limits>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

int main(int argc, char** argv) {
    try {
        if (argc != 4 && argc != 8)
            throw std::invalid_argument(
                "Use decode INPUT OUTPUT or encode RAW WIDTH HEIGHT CHANNELS COLOR OUTPUT");
        const auto input = std::filesystem::u8path(argv[2]);
        std::ifstream f(input, std::ios::binary);
        if (!f)
            throw std::runtime_error("Cannot open reference input");
        const std::vector<unsigned char> bytes{std::istreambuf_iterator<char>(f), {}};
        qoi_desc desc{};
        int size = 0;
        std::unique_ptr<void, decltype(&std::free)> output(nullptr, std::free);
        const bool encode = std::string(argv[1]) == "encode";
        if (encode) {
            if (argc != 8)
                throw std::invalid_argument("Incomplete reference encode request");
            desc.width = unsigned(std::stoul(argv[3]));
            desc.height = unsigned(std::stoul(argv[4]));
            desc.channels = static_cast<unsigned char>(std::stoul(argv[5]));
            desc.colorspace = static_cast<unsigned char>(std::stoul(argv[6]));
            if (std::uint64_t(desc.width) * desc.height * desc.channels != bytes.size())
                throw std::invalid_argument("Reference raw length differs");
            output.reset(qoi_encode(bytes.data(), &desc, &size));
        } else {
            if (std::string(argv[1]) != "decode" || argc != 4 ||
                bytes.size() > std::numeric_limits<int>::max())
                throw std::invalid_argument("Invalid reference decode request");
            output.reset(qoi_decode(bytes.data(), int(bytes.size()), &desc, 4));
            const auto total = std::uint64_t(desc.width) * desc.height * 4;
            if (total > std::numeric_limits<int>::max())
                throw std::invalid_argument("Reference output too large");
            size = int(total);
        }
        if (!output || size <= 0)
            throw std::runtime_error("Reference codec refused input");
        std::ofstream result(std::filesystem::u8path(argv[encode ? 7 : 3]), std::ios::binary);
        result.write(static_cast<const char*>(output.get()), size);
        result.close();
        if (!result)
            throw std::runtime_error("Cannot write reference output");
        std::cout << desc.width << ' ' << desc.height << ' ' << unsigned(desc.channels) << ' '
                  << unsigned(desc.colorspace) << '\n';
    } catch (const std::exception& e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
