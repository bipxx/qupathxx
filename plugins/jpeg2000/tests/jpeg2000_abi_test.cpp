// SPDX-License-Identifier: AGPL-3.0-only
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <qupathxx/reader_plugin.h>
#include <array>
#include <filesystem>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string_view>

namespace {
void check(bool condition, const char* message) {
    if (!condition)
        throw std::runtime_error(message);
}
int QPX_READER_CALL stop(void*) { return 1; }
}

int wmain(int argc, wchar_t** argv) {
    try {
        check(argc == 3 || (argc == 4 && std::wstring_view(argv[3]) == L"--signed"),
              "Expected module and authored 16-bit image paths, with --signed for Int16");
        const bool signed_samples = argc == 4;
        const auto module = LoadLibraryExW(argv[1], nullptr,
                                          LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR | LOAD_LIBRARY_SEARCH_SYSTEM32);
        check(module != nullptr, "Cannot load JPEG 2000 module");
        auto entry = reinterpret_cast<qpx_reader_entry_fn_v1>(
            reinterpret_cast<void*>(GetProcAddress(module, "qpx_reader_entry_v1")));
        qpx_reader_api_v1 api{};
        api.struct_size = sizeof(api);
        char message[512]{};
        qpx_reader_text_v1 error{message, sizeof(message), 0};
        check(entry && entry(1, &api, &error) == QPX_READER_OK, "Cannot load ABI1 table");
        const auto path = std::filesystem::path(argv[2]).u8string();
        qpx_reader_open_v1 request{};
        request.struct_size = sizeof(request);
        request.path = {path.data(), path.size()};
        request.work_bytes = 32768;
        request.cancel = {stop, nullptr};
        void* handle = nullptr;
        check(api.open(&request, &handle, &error) == QPX_READER_CANCELLED && !handle,
              "Cancelled open published an image");
        request.cancel = {};
        request.work_bytes = 100;
        check(api.open(&request, &handle, &error) == QPX_READER_INVALID_REQUEST && !handle,
              "Open accepted an insufficient work allowance");
        request.work_bytes = 16u * 1024 * 1024;
        check(api.open(&request, &handle, &error) == QPX_READER_OK && handle,
              "Open refused authored 16-bit samples");
        std::unique_ptr<void, decltype(api.destroy)> image(handle, api.destroy);
        qpx_reader_region_v1 region{sizeof(region), 0, -1, 0, 3, 1, 0, 1, 0, 0};
        std::array<uint8_t, 6> raw{};
        std::array<uint8_t, 3> validity{};
        qpx_reader_pixels_v1 pixels{sizeof(pixels), signed_samples ? QPX_READER_INT16 : QPX_READER_UINT16,
                                    3, 1, 1, 0, 6, 2, 2,
                                    raw.data(), raw.size(), 0, validity.data(), validity.size(), 0};
        check(api.read_raw(handle, &region, &pixels, {stop, nullptr}, &error) == QPX_READER_CANCELLED &&
                  !pixels.written && !pixels.validity_written,
              "Cancelled raw read reported samples");
        const auto capacity = pixels.capacity;
        pixels.capacity = capacity - 1;
        check(api.read_raw(handle, &region, &pixels, {}, &error) == QPX_READER_INVALID_REQUEST &&
                  !pixels.written && !pixels.validity_written,
              "Short output buffer accepted");
        pixels.capacity = capacity;
        const std::array<uint8_t, 6> expected_raw{0, 0, 0, signed_samples ? uint8_t(128) : uint8_t(0), 255, 255};
        check(api.read_raw(handle, &region, &pixels, {}, &error) == QPX_READER_OK &&
                  pixels.written == raw.size() && pixels.validity_written == validity.size() &&
                  raw == expected_raw &&
                  validity == std::array<uint8_t, 3>{0, 1, 1},
              "Exact 16-bit pixels or validity padding differ");
        std::array<uint8_t, 12> rgba{};
        region.channels = 0;
        pixels = {sizeof(pixels), QPX_READER_UINT8, 3, 1, 4, 0, 12, 4, 1,
                  rgba.data(), rgba.size(), 0, nullptr, 0, 0};
        const auto second = signed_samples ? uint8_t(127) : uint8_t(255);
        check(api.read_rgba(handle, &region, &pixels, &error) == QPX_READER_OK &&
                  pixels.written == rgba.size() && !pixels.validity_written &&
                  rgba == std::array<uint8_t, 12>{0, 0, 0, 0, 0, 0, 0, 255, second, second, second, 255},
              "Gray display or transparency padding differs");
        std::cout << "JPEG 2000 ABI passed exact typed padding, cancellation, work and buffer checks\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
