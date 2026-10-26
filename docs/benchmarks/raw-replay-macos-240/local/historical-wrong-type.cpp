#include "raw_pixel_archive_checks.hpp"
#include "qupathxx/raw_pixel_contours.hpp"
inline std::shared_ptr<const qpx::RawPixelContours> observer_failure(const qpx::RawPixelPredictionRequest& r,std::shared_ptr<const qpx::RawPixelClassification> p,qpx::TissueContourOptions o={},const std::atomic<bool>* c=nullptr) {
 static int count=0;if(++count==2) throw std::runtime_error("injected wrong exception type");
 return qpx::trace_raw_pixel_prediction(r,std::move(p),o,c);
}
#define trace_raw_pixel_prediction(...) observer_failure(__VA_ARGS__)
#include "/mnt/c/Users/HEQ/Projects/qupathxx/build/ci/scientific-grid-159/cpp/tests/raw_pixel_replay_checks.hpp"
#undef trace_raw_pixel_prediction
int main(){try{raw_replay_checks::run();return 0;}catch(const std::exception& e){std::cerr<<e.what()<<std::endl;return 1;}}
