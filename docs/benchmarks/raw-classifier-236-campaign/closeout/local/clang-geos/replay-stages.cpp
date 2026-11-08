#include "replay-stages.hpp"
int main(){try{raw_replay_checks::run();return 0;}catch(const std::exception& e){std::cerr<<"UNCAUGHT "<<e.what()<<std::endl;return 1;}}
