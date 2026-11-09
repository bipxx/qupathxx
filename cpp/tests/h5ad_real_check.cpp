#include "qupathxx/h5ad.hpp"
#include "qupathxx/file_fingerprint.hpp"
#include "qupathxx/spatial_counts.hpp"
#include <nlohmann/json.hpp>
#include <iostream>
#include <map>
#include <chrono>
using namespace qpx;
void check(bool okay,const char* message){if(!okay)throw std::runtime_error(message);}
// Opt-in full-data format comparison; never downloaded or run by ordinary CTest.
int main(int argc,char** argv){try{
    check(argc==5,"Usage: h5ad_real_check input.h5ad original.qmx positions.tsv new.qmx");
    constexpr auto input_sha="ca8515e12ad0b35d4603ee8c13e2ea66d23a983bed1df2091acbdb6ef3d91317";
    constexpr auto original_sha="79f407bf9b0810f417410961dd79db6a6ce1ff8999e085d1ef8f7865eaca7401";
    constexpr auto position_sha="dd313edd021d0f3fa95b7beed61e6e7ed495c9a3b50b24d5582ab6aeef5f9b4c";
    const auto started=std::chrono::steady_clock::now();
    check(fingerprint_file(argv[1])->sha256==input_sha,"Real H5AD bytes differ");
    auto original=open_expression_matrix(argv[2],original_sha);
    auto positions=read_spatial_positions(argv[3]);check(positions.source_sha256==position_sha,"Original positions differ");
    H5adOptions options;options.domain=ExpressionValueDomain::Counts;options.value_domain="raw-counts";
    options.frame.id="10x-retained-hires-pixels";options.frame.source_id=position_sha;options.frame.unit=CoordinateUnit::Pixel;
    options.frame.width=1921;options.frame.height=2000;options.frame.image_level_zero=true;
    const auto result=import_h5ad_matrix(argv[1],argv[4],options);
    auto actual=open_expression_matrix(result.path,result.file_sha256);const auto& m=actual->metadata();
    check(m.features==32285 && m.observations==2702 && m.entries==16031101,"Real matrix axes/entries differ");
    check(m.domain==ExpressionValueDomain::Counts && m.spatial_frame->sha256()==options.frame.sha256(),"Real value domain/frame changed");
    check(result.peak_work_bytes<=256u*1024*1024,"Real import exceeded accounted budget");
    check(actual->observation_page(0,2702)==original->observation_page(0,2702),"Observation axis changed");
    std::uint64_t compared=0;
    for(std::uint64_t first=0;first<m.features;first+=32){
        const auto n=std::size_t(std::min<std::uint64_t>(32,m.features-first));
        const auto a=actual->feature_page(first,n),b=original->feature_page(first,n);ExpressionMatrixSelection selection;
        for(std::size_t i=0;i<n;++i){check(a[i].id==b[i].id && a[i].index==b[i].index,"Feature identity/order changed");selection.features.push_back(first+i);}
        const auto left=actual->read(selection),right=original->read(selection);
        check(left.offsets==right.offsets && left.observation_indices==right.observation_indices && left.counts==right.counts,"Real sparse count/entry/axis changed");compared+=left.counts.size();
    }
    check(compared==m.entries,"Not every stored entry was compared");
    std::map<std::string,Point> by_id;for(const auto& p:positions.observations)by_id.emplace(p.id,p.position);
    const auto spatial=actual->spatial_page(0,2702);check(spatial.size()==by_id.size(),"Coordinate cohort changed");
    for(const auto& p:spatial){const auto found=by_id.find(p.id);check(found!=by_id.end() && p.position.x==found->second.x && p.position.y==found->second.y,"Exact ID-joined source coordinates changed");}
    check(fingerprint_file(argv[1])->sha256==input_sha && fingerprint_file(argv[2])->sha256==original_sha && fingerprint_file(argv[3])->sha256==position_sha,"Real inputs mutated");
    std::cout<<nlohmann::json{{"state","passed"},{"input_sha256",input_sha},{"original_sha256",original_sha},{"position_sha256",position_sha},{"store_sha256",result.file_sha256},{"store_bytes",result.file_bytes},{"features",m.features},{"observations",m.observations},{"entries_compared",compared},{"coordinates_compared",spatial.size()},{"import_ms",result.elapsed_ms},{"peak_accounted_work_bytes",result.peak_work_bytes},{"elapsed_ms",std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-started).count()},{"scope","disclosed lossless current-profile derivative of admitted full10x raw counts; no biology or legacy AnnData compatibility claim"}}.dump(2)<<'\n';
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
