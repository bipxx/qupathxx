#include "raw_pixel_archive_checks.hpp"
#include "qupathxx/raw_pixel_contours.hpp"
inline std::shared_ptr<const qpx::RawPixelContours> observer_failure(const qpx::RawPixelPredictionRequest&,std::shared_ptr<const qpx::RawPixelClassification>,qpx::TissueContourOptions={},const std::atomic<bool>* =nullptr) {throw std::invalid_argument("injected observer sentinel");}
#define trace_raw_pixel_prediction(...) observer_failure(__VA_ARGS__)
#include "/mnt/c/Users/HEQ/Projects/qupathxx/build/ci/scientific-grid-159/cpp/tests/raw_pixel_replay_checks.hpp"
#undef trace_raw_pixel_prediction
int main(){try{raw_replay_checks::run();return 0;}catch(const std::exception& e){std::cerr<<e.what()<<std::endl;return 1;}}
