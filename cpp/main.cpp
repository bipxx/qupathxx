#include "qupathxx/core.hpp"
#include <iostream>
#include <string>
int main(int argc, char** argv) {
    try {
        bool smoke = argc == 2 && std::string(argv[1]) == "--smoke";
        qpx::Document document(argc > 1 && !smoke ? qpx::Image::read_ppm(argv[1]) : qpx::Image::demo());
        qpx::run_gui(document, smoke ? 3 : 0);
    } catch (const std::exception& e) { std::cerr << e.what() << '\n'; return 1; }
}
