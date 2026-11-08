#include <memory>
#include <stdexcept>
volatile int selected_exception = 0;
std::shared_ptr<const int> throwing_api() {
    if(selected_exception == 0) throw std::invalid_argument("invalid marker");
    if(selected_exception == 1) throw std::length_error("length marker");
    if(selected_exception == 2) throw std::runtime_error("runtime marker");
    return std::make_shared<const int>(7);
}
