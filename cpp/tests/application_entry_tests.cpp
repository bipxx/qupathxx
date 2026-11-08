#include "qupathxx/application.hpp"
#include <stdexcept>
int main() {
    if(qpx::run_application({"--help"})!=0)throw std::runtime_error("native help failed");
    if(qpx::run_application({"--overview-source-pixels","0"})!=2)throw std::runtime_error("invalid budget reached startup");
    if(qpx::run_application({"--image-id","0"})!=2)throw std::runtime_error("invalid image ID reached startup");
}
