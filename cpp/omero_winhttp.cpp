#include "qupathxx/omero.hpp"
#include "qupathxx/workflow.hpp"
#include <array>
#include <charconv>
#include <chrono>
#include <stdexcept>
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <winhttp.h>
namespace qpx {
namespace {
void checked(BOOL ok, const char* stage = "WinHTTP") {
    if (!ok)
        throw std::runtime_error(std::string("OMERO HTTPS ") + stage + " failed (Windows error " +
                                 std::to_string(GetLastError()) + ")");
}
struct Handle {
    HINTERNET value = nullptr;
    explicit Handle(HINTERNET v) : value(v) {
        checked(v != nullptr);
    }
    ~Handle() {
        WinHttpCloseHandle(value);
    }
    Handle(const Handle&) = delete;
};
std::wstring wide(const std::string& s) {
    const auto n =
        MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, s.data(), int(s.size()), nullptr, 0);
    if (!n)
        throw std::invalid_argument("Invalid HTTPS UTF-8");
    std::wstring w(n, 0);
    checked(MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, s.data(), int(s.size()), w.data(),
                                n) != 0);
    return w;
}
std::string utf8(const std::wstring& w) {
    if (w.empty())
        return {};
    const auto n = WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, w.data(), int(w.size()),
                                       nullptr, 0, nullptr, nullptr);
    if (!n)
        throw std::invalid_argument("Invalid HTTPS header text");
    std::string s(n, 0);
    checked(WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, w.data(), int(w.size()), s.data(), n,
                                nullptr, nullptr) != 0);
    return s;
}
std::string header(HINTERNET request, DWORD id) {
    DWORD size = 0;
    if (!WinHttpQueryHeaders(request, id, WINHTTP_HEADER_NAME_BY_INDEX, nullptr, &size,
                             WINHTTP_NO_HEADER_INDEX)) {
        if (GetLastError() == ERROR_WINHTTP_HEADER_NOT_FOUND)
            return {};
        if (GetLastError() != ERROR_INSUFFICIENT_BUFFER)
            checked(FALSE);
    }
    if (size > 16384)
        throw std::length_error("OMERO response header exceeds limit");
    std::vector<wchar_t> buffer(size / sizeof(wchar_t) + 1);
    checked(WinHttpQueryHeaders(request, id, WINHTTP_HEADER_NAME_BY_INDEX, buffer.data(), &size,
                                WINHTTP_NO_HEADER_INDEX));
    return utf8(std::wstring(buffer.data(), size / sizeof(wchar_t)));
}
class Transport final : public OmeroTransport {
  public:
    OmeroHttpResponse get(const std::string& url, std::size_t limit, std::uint32_t timeout,
                          const std::atomic<bool>* cancelled) override {
        workflow_checkpoint(cancelled);
        if (url.size() > 4096 || !limit || limit > 64u * 1024 * 1024 || timeout < 100 ||
            timeout > 60000)
            throw std::invalid_argument("Invalid native HTTPS request limits");
        const auto started = std::chrono::steady_clock::now();
        const auto checkpoint = [&] {
            workflow_checkpoint(cancelled);
            if (std::chrono::steady_clock::now() - started > std::chrono::milliseconds(timeout))
                throw std::runtime_error("OMERO HTTPS request exceeded its deadline");
        };
        const auto address = wide(url);
        URL_COMPONENTS parts{};
        parts.dwStructSize = sizeof(parts);
        parts.dwHostNameLength = parts.dwUrlPathLength = parts.dwExtraInfoLength =
            parts.dwUserNameLength = parts.dwPasswordLength = DWORD(-1);
        checked(WinHttpCrackUrl(address.c_str(), DWORD(address.size()), 0, &parts));
        if (parts.nScheme != INTERNET_SCHEME_HTTPS || parts.dwUserNameLength ||
            parts.dwPasswordLength || !parts.dwHostNameLength)
            throw std::invalid_argument("OMERO transport requires HTTPS without URL credentials");
        const std::wstring host(parts.lpszHostName, parts.dwHostNameLength);
        auto path = parts.dwUrlPathLength ? std::wstring(parts.lpszUrlPath, parts.dwUrlPathLength)
                                          : std::wstring(L"/");
        if (parts.dwExtraInfoLength)
            path.append(parts.lpszExtraInfo, parts.dwExtraInfoLength);
        Handle session(WinHttpOpen(L"qupathxx-omero/1", WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY,
                                   WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0));
        checked(WinHttpSetTimeouts(session.value, int(timeout), int(timeout), int(timeout),
                                   int(timeout)));
        Handle connection(WinHttpConnect(session.value, host.c_str(), parts.nPort, 0));
        Handle request(WinHttpOpenRequest(connection.value, L"GET", path.c_str(), nullptr,
                                          WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES,
                                          WINHTTP_FLAG_SECURE));
        DWORD disable =
            WINHTTP_DISABLE_REDIRECTS | WINHTTP_DISABLE_AUTHENTICATION | WINHTTP_DISABLE_COOKIES;
        checked(WinHttpSetOption(request.value, WINHTTP_OPTION_DISABLE_FEATURE, &disable,
                                 sizeof(disable)));
        DWORD policy = WINHTTP_AUTOLOGON_SECURITY_LEVEL_HIGH;
        checked(WinHttpSetOption(request.value, WINHTTP_OPTION_AUTOLOGON_POLICY, &policy,
                                 sizeof(policy)));
        checkpoint();
        checked(WinHttpSendRequest(
                    request.value,
                    L"Accept: application/json, image/jpeg\r\nAccept-Encoding: identity\r\n",
                    DWORD(-1), WINHTTP_NO_REQUEST_DATA, 0, 0, 0),
                "SendRequest");
        checkpoint();
        checked(WinHttpReceiveResponse(request.value, nullptr), "ReceiveResponse");
        checkpoint();
        OmeroHttpResponse out;
        DWORD status_bytes = sizeof(DWORD);
        checked(WinHttpQueryHeaders(
            request.value, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
            WINHTTP_HEADER_NAME_BY_INDEX, &out.status, &status_bytes, WINHTTP_NO_HEADER_INDEX));
        out.content_type = header(request.value, WINHTTP_QUERY_CONTENT_TYPE);
        out.location = header(request.value, WINHTTP_QUERY_LOCATION);
        const auto encoding = header(request.value, WINHTTP_QUERY_CONTENT_ENCODING);
        if (!encoding.empty() && encoding != "identity")
            throw std::invalid_argument("Unsupported compressed OMERO HTTP response");
        const auto declared = header(request.value, WINHTTP_QUERY_CONTENT_LENGTH);
        std::optional<std::size_t> expected;
        if (!declared.empty()) {
            std::uint64_t n = 0;
            const auto p = std::from_chars(declared.data(), declared.data() + declared.size(), n);
            if (p.ec != std::errc{} || p.ptr != declared.data() + declared.size() || n > limit)
                throw std::length_error(
                    "OMERO Content-Length is invalid or exceeds the response limit");
            expected = std::size_t(n);
            out.body.reserve(*expected);
        }
        std::array<std::uint8_t, 65536> buffer;
        for (;;) {
            checkpoint();
            DWORD read = 0;
            checked(WinHttpReadData(request.value, buffer.data(), DWORD(buffer.size()), &read),
                    "ReadData");
            checkpoint();
            if (!read)
                break;
            if (read > limit - out.body.size())
                throw std::length_error("OMERO streaming response exceeds byte limit");
            out.body.insert(out.body.end(), buffer.begin(), buffer.begin() + read);
        }
        if (expected && out.body.size() != *expected)
            throw std::runtime_error("Truncated OMERO HTTP response");
        return out;
    }
};
} // namespace
std::shared_ptr<OmeroTransport> native_omero_transport() {
    return std::make_shared<Transport>();
}
} // namespace qpx
#else
namespace qpx {
std::shared_ptr<OmeroTransport> native_omero_transport() {
    throw std::runtime_error("Native OMERO HTTPS transport is currently supported on Windows");
}
} // namespace qpx
#endif
