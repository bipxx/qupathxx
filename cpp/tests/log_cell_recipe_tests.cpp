#include "qupathxx/cells.hpp"
#include "qupathxx/input_digest.hpp"
#include "../cell_archive.hpp"
#include "../sha256.hpp"
#include <algorithm>
#include <iostream>
using namespace qpx;
namespace {
void require(bool value) {if(!value) throw std::runtime_error("LoG cell recipe contract failed");}
template<class F> void rejects(F f) {try {f();} catch(const std::invalid_argument&) {return;}throw std::runtime_error("missing recipe rejection");}
void sign(std::vector<std::uint8_t>& bytes,const Roi& roi) {
    Sha256 digest;const auto text=roi.wkt();digest.integer(text.size());digest.update(text.data(),text.size());digest.update(bytes.data(),bytes.size()-64);
    const auto hex=digest.finish();std::copy(hex.begin(),hex.end(),bytes.end()-64);
}
}
int main() {
    try {
        std::vector<std::uint8_t> rgb(96*96*3,255);
        for(int y=0;y<96;++y) for(int x=0;x<96;++x) if((x-32)*(x-32)+(y-32)*(y-32)<=64 || (x-64)*(x-64)+(y-64)*(y-64)<=100)
            for(int c=0;c<3;++c) rgb[(y*96+x)*3+c]=32;
        Document doc(Image(96,96,rgb));const auto object=doc.add_rectangle({34.5,24.5,45,55});
        CellParameters p;p.algorithm=CellAlgorithm::LogWatershed;p.threshold=.4;p.min_nucleus_pixels=16;p.max_nucleus_pixels=512;p.tile_size=16;
        StainTransform transform({{1,0,0},{0,1,0}});CellRequest request(doc,object,transform,p);
        require(request.level_bounds().x==2 && request.level_bounds().width==94 && request.total_tiles()==36);
        const auto result=detect_cells(request);require(std::string(result->algorithm())=="qpx.log-watershed/1");
        require(result->features().size()==2 && result->nuclei()->samples()==45*55);
        require(result->nuclei()->input_format()==std::string("qpx.log-rgba-tiles/1"));
        require(input_sha256(request)==result->nuclei()->input_sha256());
        bool clipped_seed=false;
        for(const auto& marker:result->markers()) if(marker.result_label && !result->nuclei()->labels()[marker.pixel_index]) clipped_seed=true;
        require(clipped_seed);
        const auto bytes=CellArchiveCodec::encode(*result);require(bytes[3]==2);
        const auto restored=CellArchiveCodec::decode(bytes.data(),bytes.size(),request.roi(),doc.metadata());
        require(restored->parameters().algorithm==p.algorithm && restored->parameters().context_padding==32);
        require(restored->nuclei()->labels()==result->nuclei()->labels() && restored->cells()->labels()==result->cells()->labels());
        const auto record=doc.keep_cells(request,result);const auto replay=CellRequest::replay(doc,record);
        const auto repeated=detect_cells(replay);require(doc.verify_cells(record,replay,*repeated));
        for(auto version:{0,3}) {auto bad=bytes;bad[3]=static_cast<std::uint8_t>(version);sign(bad,request.roi());rejects([&]{CellArchiveCodec::decode(bad.data(),bad.size(),request.roi(),doc.metadata());});}
        for(auto algorithm:{0,2}) {auto bad=bytes;bad[4]=static_cast<std::uint8_t>(algorithm);sign(bad,request.roi());rejects([&]{CellArchiveCodec::decode(bad.data(),bad.size(),request.roi(),doc.metadata());});}
        auto different=p;different.log_prominence=.75;CellRequest altered(doc,object,transform,different);
        rejects([&]{doc.verify_cells(record,altered,*detect_cells(altered));});
        CellParameters inactive;inactive.context_padding=33;rejects([&]{inactive.validate();});
        CellParameters invalid;invalid.algorithm=static_cast<CellAlgorithm>(9);rejects([&]{invalid.validate();});
        const auto legacy=detect_cells(CellRequest(doc,object,transform,{}));require(CellArchiveCodec::encode(*legacy)[3]==1);
        auto body_parameters=p;body_parameters.algorithm=CellAlgorithm::LogWatershedBodies;
        const CellRequest body_request(doc,object,transform,body_parameters);const auto body_result=detect_cells(body_request);
        require(std::string(body_result->algorithm())=="qpx.log-watershed-bodies/1");
        require(body_result->nuclei()->labels()==result->nuclei()->labels());
        require(input_sha256(body_request)==body_result->nuclei()->input_sha256() && input_sha256(body_request)!=input_sha256(request));
        const auto body_bytes=CellArchiveCodec::encode(*body_result);require(body_bytes[3]==3);
        const auto body_restored=CellArchiveCodec::decode(body_bytes.data(),body_bytes.size(),body_request.roi(),doc.metadata());
        require(body_restored->parameters().algorithm==body_parameters.algorithm && body_restored->cells()->labels()==body_result->cells()->labels());
        const auto body_record=doc.keep_cells(body_request,body_result);const auto body_replay=CellRequest::replay(doc,body_record);
        require(doc.verify_cells(body_record,body_replay,*detect_cells(body_replay)));
        for(auto version:{1,2,4}) {auto bad=body_bytes;bad[3]=static_cast<std::uint8_t>(version);sign(bad,body_request.roi());rejects([&]{CellArchiveCodec::decode(bad.data(),bad.size(),body_request.roi(),doc.metadata());});}
        CellParameters sampled(CellAlgorithm::SampledLogWatershedBodies);
        require(sampled.min_peak_radius==0 && sampled.min_seed_distance==0 && sampled.connectivity==8);
        sampled.threshold=.4;sampled.min_nucleus_pixels=16;sampled.max_nucleus_pixels=512;sampled.tile_size=16;
        const CellRequest sampled_request(doc,object,transform,sampled);const auto sampled_result=detect_cells(sampled_request);
        require(std::string(sampled_result->algorithm())=="qpx.sampled-log-watershed-bodies/1");
        require(sampled_result->features().size()==2 && sampled_result->nuclei()->input_format()==std::string("qpx.sampled-log-rgba-tiles/1"));
        require(input_sha256(sampled_request)==sampled_result->nuclei()->input_sha256() && input_sha256(sampled_request)!=input_sha256(body_request));
        const auto sampled_bytes=CellArchiveCodec::encode(*sampled_result);require(sampled_bytes[3]==4);
        const auto sampled_restored=CellArchiveCodec::decode(sampled_bytes.data(),sampled_bytes.size(),sampled_request.roi(),doc.metadata());
        require(sampled_restored->nuclei()->labels()==sampled_result->nuclei()->labels() && sampled_restored->cells()->labels()==sampled_result->cells()->labels());
        require(std::any_of(sampled_result->markers().begin(),sampled_result->markers().end(),[](const auto& marker){return marker.result_label==0;}));
        const auto sampled_record=doc.keep_cells(sampled_request,sampled_result);const auto sampled_replay=CellRequest::replay(doc,sampled_record);
        require(doc.verify_cells(sampled_record,sampled_replay,*detect_cells(sampled_replay)));
        for(auto version:{1,2,3,5}) {auto bad=sampled_bytes;bad[3]=static_cast<std::uint8_t>(version);sign(bad,sampled_request.roi());rejects([&]{CellArchiveCodec::decode(bad.data(),bad.size(),sampled_request.roi(),doc.metadata());});}
        for(auto algorithm:{0,1,2}) {auto bad=sampled_bytes;bad[4]=static_cast<std::uint8_t>(algorithm);sign(bad,sampled_request.roi());rejects([&]{CellArchiveCodec::decode(bad.data(),bad.size(),sampled_request.roi(),doc.metadata());});}
        {auto bad=sampled_bytes;const auto domain_bytes=(sampled_result->nuclei()->labels().size()+7)/8;bad[bad.size()-64-domain_bytes]^=1;sign(bad,sampled_request.roi());rejects([&]{CellArchiveCodec::decode(bad.data(),bad.size(),sampled_request.roi(),doc.metadata());});}
        Document blank_doc(Image(5,5,std::vector<std::uint8_t>(75,255)));const auto blank_object=blank_doc.add_rectangle({0,0,5,5});auto blank_parameters=sampled;blank_parameters.context_padding=0;
        const CellRequest blank_request(blank_doc,blank_object,transform,blank_parameters);const auto blank_result=detect_cells(blank_request);require(blank_result->features().empty());
        const auto blank_bytes=CellArchiveCodec::encode(*blank_result);require(CellArchiveCodec::encode(*CellArchiveCodec::decode(blank_bytes.data(),blank_bytes.size(),blank_request.roi(),blank_doc.metadata()))==blank_bytes);
        {auto bad=blank_bytes;bad[bad.size()-65]|=128;sign(bad,blank_request.roi());rejects([&]{CellArchiveCodec::decode(bad.data(),bad.size(),blank_request.roi(),blank_doc.metadata());});}
        {auto bad=blank_bytes;bad[bad.size()-64-4-8]^=1;sign(bad,blank_request.roi());rejects([&]{CellArchiveCodec::decode(bad.data(),bad.size(),blank_request.roi(),blank_doc.metadata());});}
        auto unsupported=sampled;unsupported.min_seed_distance=4;rejects([&]{unsupported.validate();});unsupported=sampled;unsupported.min_peak_radius=1;rejects([&]{unsupported.validate();});unsupported=sampled;unsupported.connectivity=4;rejects([&]{unsupported.validate();});
        require(CellArchiveCodec::encode(*restored)==bytes && CellArchiveCodec::encode(*body_restored)==body_bytes && CellArchiveCodec::encode(*sampled_restored)==sampled_bytes);
        std::cout<<"Sampled LoG QCL4, explicit seed preset, typed recipe identities, replay and QCL1/2/3 preservation passed\n";
        std::cout<<"Captured LoG context, fractional ROI clipping, digests, QCL2, replay and legacy QCL1 passed\n";
    } catch(const std::exception& e) {std::cerr<<e.what()<<'\n';return 1;}
}
