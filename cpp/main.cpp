#include "qupathxx/core.hpp"
#include <iostream>
#include <string>
int main(int argc, char** argv) {
    try {
        bool smoke=false; std::string path;
        for (int i=1;i<argc;++i) { if (std::string(argv[i])=="--smoke") smoke=true; else if (path.empty()) path=argv[i]; else throw std::invalid_argument("Usage: qupathxx_app [image] [--smoke]"); }
        auto server=path.empty() ? qpx::dense_image_server(qpx::Image::demo()) :
            (path.size()>=4 && path.substr(path.size()-4)==".ppm" ? qpx::dense_image_server(qpx::Image::read_ppm(path)) : qpx::open_slide(path));
        qpx::Document document(server);
        auto report=qpx::run_gui(document, smoke ? 90 : 0);
        if (smoke && !report.uploaded_tiles) throw std::runtime_error("smoke run did not upload any image tiles");
    } catch (const std::exception& e) { std::cerr << e.what() << '\n'; return 1; }
}
