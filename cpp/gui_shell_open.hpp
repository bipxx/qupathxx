#pragma once
#include "gui_notice_message.hpp"
#include <filesystem>
#include <functional>
#include <stdexcept>
#include <string>
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <shellapi.h>
#endif
namespace qpx::ui {
// Tests replace the launch, never the path/availability checks.
inline thread_local std::function<void(const std::string&)> shell_open_override;
inline void open_in_shell(const std::string& path) {
    if(path.empty() || !std::filesystem::exists(std::filesystem::u8path(path)))throw ui::NoticeException<std::runtime_error>("Result path is unavailable");
    if(shell_open_override) {shell_open_override(path);return;}
#ifdef _WIN32
    if(reinterpret_cast<std::intptr_t>(ShellExecuteW(nullptr,L"open",std::filesystem::u8path(path).c_str(),nullptr,nullptr,SW_SHOWNORMAL))<=32)
        throw ui::NoticeException<std::runtime_error>("Windows could not open the result path");
#else
    throw ui::NoticeException<std::runtime_error>("Open the displayed result path in your file manager");
#endif
}
}
