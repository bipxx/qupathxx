#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <typeinfo>
extern volatile int selected_exception;
std::shared_ptr<const int> throwing_api();
template<class E, class F> __attribute__((noinline)) void rejects(F f) {
    try {f();} catch(const E&) {return;}
    throw std::runtime_error("missing rejection");
}
int main() {
    int failures = 0;
    selected_exception = 0;
    try {rejects<std::invalid_argument>([]{throwing_api();});std::cout<<"invalid=expected\n";}
    catch(const std::exception& e) {++failures;std::cout<<"invalid=escaped:"<<typeid(e).name()<<':'<<e.what()<<'\n';}
    selected_exception = 1;
    try {rejects<std::length_error>([]{throwing_api();});std::cout<<"length=expected\n";}
    catch(const std::exception& e) {++failures;std::cout<<"length=escaped:"<<typeid(e).name()<<':'<<e.what()<<'\n';}
    selected_exception = 2;
    try {rejects<std::runtime_error>([]{throwing_api();});std::cout<<"runtime=expected\n";}
    catch(const std::exception& e) {++failures;std::cout<<"runtime=escaped:"<<typeid(e).name()<<':'<<e.what()<<'\n';}
    try {rejects<std::invalid_argument>([]{throwing_api();});std::cout<<"wrong_type=accepted\n";return 2;}
    catch(const std::runtime_error& e) {if(std::string(e.what())!="runtime marker")return 3;std::cout<<"wrong_type=escaped\n";}
    selected_exception = 3;
    try {rejects<std::invalid_argument>([]{throwing_api();});std::cout<<"missing=accepted\n";return 4;}
    catch(const std::runtime_error& e) {if(std::string(e.what())!="missing rejection")return 5;std::cout<<"missing=refused\n";}
    return failures ? 1 : 0;
}
