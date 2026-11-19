// SPDX-License-Identifier: AGPL-3.0-only
// Exercise libjxl's own container parser, independent of the plugin.
#include <jxl/decode.h>
#include <fstream>
#include <iostream>
#include <iterator>
#include <memory>
#include <vector>

int main(int argc, char** argv) {
    if (argc != 2 || JxlDecoderVersion() != 13000) return 1;
    std::ifstream file(argv[1], std::ios::binary);
    if (!file) return 1;
    const std::vector<std::uint8_t> input{std::istreambuf_iterator<char>(file), {}};
    std::unique_ptr<JxlDecoder, decltype(&JxlDecoderDestroy)> decoder(JxlDecoderCreate(nullptr), JxlDecoderDestroy);
    if (!decoder || JxlDecoderSetInput(decoder.get(), input.data(), input.size()) != JXL_DEC_SUCCESS ||
        JxlDecoderSubscribeEvents(decoder.get(), JXL_DEC_FULL_IMAGE) != JXL_DEC_SUCCESS)
        return 1;
    JxlDecoderCloseInput(decoder.get());
    const JxlPixelFormat format{3, JXL_TYPE_UINT8, JXL_NATIVE_ENDIAN, 0};
    std::vector<std::uint8_t> output;
    bool full = false;
    for (;;) {
        const auto status = JxlDecoderProcessInput(decoder.get());
        switch (status) {
        case JXL_DEC_NEED_IMAGE_OUT_BUFFER: {
            std::size_t size = 0;
            if (JxlDecoderImageOutBufferSize(decoder.get(), &format, &size) != JXL_DEC_SUCCESS || size != 24)
                return 1;
            output.resize(size);
            if (JxlDecoderSetImageOutBuffer(decoder.get(), &format, output.data(), size) != JXL_DEC_SUCCESS)
                return 1;
            break;
        }
        case JXL_DEC_FULL_IMAGE:
            full = true;
            break;
        case JXL_DEC_SUCCESS:
            if (!full || output.size() != 24) return 1;
            for (std::size_t i = 0; i < output.size(); ++i)
                if (output[i] != i) return 1;
            std::cout << "libjxl 0.13.0 directly decoded all 24 authored RGB samples\n";
            return 0;
        default:
            std::cerr << "libjxl returned " << status << '\n';
            return 1;
        }
    }
}
