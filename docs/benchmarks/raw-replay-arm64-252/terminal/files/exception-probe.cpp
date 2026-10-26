#include <stdexcept>
#include <cstdio>
int main(){try{throw std::invalid_argument("raw grid source series changed");}catch(const std::invalid_argument&){puts("typed probe caught");}try{throw std::runtime_error("wrong type probe");}catch(const std::exception&){puts("outer probe caught");}return 0;}
