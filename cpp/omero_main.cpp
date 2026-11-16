#include "omero_cli.hpp"
#include <csignal>
#include <iostream>

namespace {
std::atomic<bool> cancelled{false};
void stop(int) {
    cancelled.store(true);
}
} // namespace

int main(int argc, char** argv) {
    std::signal(SIGINT, stop);
    std::signal(SIGTERM, stop);
    try {
        return qpx::detail::omero_cli({argv + 1, argv + argc}, cancelled);
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 2;
    }
}
