#include "qupathxx/application.hpp"
#include <filesystem>
#include <iostream>
#ifdef _WIN32
int wmain(int argc,wchar_t** argv) {
    try {std::vector<std::string> args;for(int i=1;i<argc;++i)args.push_back(std::filesystem::path(argv[i]).u8string());return qpx::run_application(args);}
    catch(const std::exception& error) {std::cerr<<error.what()<<'\n';return 2;}
}
#else
int main(int argc,char** argv) {return qpx::run_application(std::vector<std::string>(argv+1,argv+argc));}
#endif
