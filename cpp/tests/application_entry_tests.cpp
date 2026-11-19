#include "qupathxx/application.hpp"
#include <stdexcept>
int main() {
    if(qpx::run_application({"--help"})!=0)throw std::runtime_error("native help failed");
    if(qpx::run_application({"--overview-source-pixels","0"})!=2)throw std::runtime_error("invalid budget reached startup");
    if(qpx::run_application({"--image-id","0"})!=2)throw std::runtime_error("invalid image ID reached startup");
    if(qpx::run_application({"target.vti","--reader-options","{}"})!=2)
        throw std::runtime_error("reader options accepted automatic reader selection");
    if(qpx::run_application({"target.ppm","--reader","ppm","--reader-options","{}"})!=2)
        throw std::runtime_error("reader options accepted a built-in reader");
    if(qpx::run_application({"--project","study.qpx","--reader","org.qupathxx.vti","--reader-options","{}"})!=2)
        throw std::runtime_error("reader options changed a saved project selection");
    if(qpx::run_application({"target.vti","--reader","org.qupathxx.vti","--reader-options","{}","--reader-options","{}"})!=2)
        throw std::runtime_error("duplicate reader options reached startup");
}
