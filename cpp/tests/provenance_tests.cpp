#include "../provenance_codec.hpp"
#include "qupathxx/cells.hpp"
#include <iostream>
#include <stdexcept>
using namespace qpx;
void check(bool b) {if(!b) throw std::runtime_error("provenance check failed");}
int main() {try {
    Document doc(Image(16,16,std::vector<std::uint8_t>(16*16*3,0)));const auto id=doc.add_rectangle({0,0,16,16});doc.set_calibration(.5,.25);
    AnalysisRequest request(doc,id,StainTransform({{1,0,0},{0,1,0}}));const auto before=request.provenance();doc.set_calibration(2,3);
    const auto result=analyze(request);check(result.provenance==before && before->effective_pixel_width_um==.5 && before->effective_pixel_height_um==.25);
    const auto bytes=ProvenanceCodec::encode(*before);const auto restored=ProvenanceCodec::decode(bytes.data(),bytes.size());check(provenance_json(*restored)==provenance_json(*before));
    auto corrupt=bytes;corrupt[8]^=1;try {ProvenanceCodec::decode(corrupt.data(),corrupt.size());throw std::logic_error("missing corruption rejection");} catch(const std::invalid_argument&) {}
    const auto env=build_environment();check(env->source_tree_sha256.size()==64 && !env->compiler.empty() && !env->configuration.empty());
    TissueRequest tissue(doc,id,StainTransform({{1,0,0},{0,1,0}}));check(detect_tissue(tissue)->provenance()==tissue.provenance());
    CellRequest cells(doc,id,StainTransform({{1,0,0},{0,1,0}}));const auto detected=detect_cells(cells);check(detected->nuclei()->provenance() && detected->nuclei()->provenance()==detected->cells()->provenance());
    std::cout<<"Capture-time source/environment provenance, native codec and kernels passed\n";
} catch(const std::exception& e) {std::cerr<<e.what()<<'\n';return 1;}}
