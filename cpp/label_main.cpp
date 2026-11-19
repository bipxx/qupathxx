// SPDX-License-Identifier: AGPL-3.0-only
// Native control entry point; scientific/workflow behavior stays in shared services.
#include "label_cli.hpp"
#include "reader_plugin_cli.hpp"
#include <csignal>
#include <filesystem>
#include <iostream>
#include <utility>
#ifdef _WIN32
#define NOMINMAX
#include <windows.h>
#endif
namespace {
std::atomic<bool> cancelled{false};
void interrupt(int) { cancelled.store(true); }
#ifdef _WIN32
BOOL WINAPI console_interrupt(DWORD event) {
    if (event == CTRL_C_EVENT || event == CTRL_BREAK_EVENT) {
        cancelled.store(true);
        return TRUE;
    }
    return FALSE;
}
#endif
int execute(std::vector<std::string> args) {
    try {
        qpx::detail::consume_reader_plugin_prefix(args);
        if (args.size() == 1 || (args.size() == 2 && args[1] == "--help")) {
            std::cout << qpx::detail::label_cli_help("qupathxx_labels");
            return 0;
        }
        if (args.size() == 2 && args[1] == "--readers") {
            std::cout << qpx::reader_catalog_json() << '\n';
            return 0;
        }
        const auto result = qpx::detail::label_cli(args, cancelled);
        if (!result)
            throw std::invalid_argument("Unknown label command; see --help");
        return *result;
    } catch (const std::exception& e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
} // namespace
#ifdef _WIN32
int wmain(int argc, wchar_t** argv) {
    SetConsoleCtrlHandler(console_interrupt, TRUE);
    std::vector<std::string> args;
    for (int i = 0; i < argc; ++i)
        args.push_back(std::filesystem::path(argv[i]).u8string());
    return execute(std::move(args));
}
#else
int main(int argc, char** argv) {
    std::signal(SIGINT, interrupt);
    std::signal(SIGTERM, interrupt);
    return execute({argv, argv + argc});
}
#endif
