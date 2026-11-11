#include "../matrix_format_registry.hpp"
#include "qupathxx/workflow.hpp"
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <limits>
#include <stdexcept>
#include <typeinfo>

namespace fs=std::filesystem;
namespace mf=qpx::detail;
using namespace qpx;
namespace {
void check(bool condition,const char* message) {if(!condition)throw std::runtime_error(message);}
struct Failure {std::string type,message;};
template<class Action> Failure failure(Action action) {
    try {action();}catch(const std::exception& error){return {typeid(error).name(),error.what()};}
    throw std::runtime_error("Expected matrix-format failure");
}
template<class Direct,class Dispatched> void same_failure(Direct direct,Dispatched dispatched) {
    const auto a=failure(direct),b=failure(dispatched);
    check(a.type==b.type && a.message==b.message,"Matrix catalog changed native failure type, message or validation order");
}
template<class Action> void invalid(Action action) {
    try {action();}catch(const std::invalid_argument&){return;}throw std::runtime_error("Expected invalid matrix format/request refusal");
}
void write(const fs::path& path,const std::string& value) {
    std::ofstream stream(path,std::ios::binary);stream.write(value.data(),static_cast<std::streamsize>(value.size()));stream.close();
    check(bool(stream),"Write owned matrix-format fixture");
}
std::string bytes(const fs::path& path) {
    std::ifstream stream(path,std::ios::binary);check(bool(stream),"Read owned matrix-format fixture");
    return {std::istreambuf_iterator<char>(stream),std::istreambuf_iterator<char>()};
}
void no_stage(const fs::path& root) {
    for(const auto& entry:fs::directory_iterator(root)) {
        const auto name=entry.path().filename().u8string();
        check(name.rfind(".qpx-expression-",0)!=0 && name.find(".qpx-tmp-")==std::string::npos,
            "Matrix-format failure leaked staging or atomic output");
    }
}
void same_metadata(const ExpressionMatrixMetadata& a,const ExpressionMatrixMetadata& b) {
    check(a.features==b.features && a.observations==b.observations && a.entries==b.entries && a.domain==b.domain &&
        a.value_domain==b.value_domain && a.input_profile==b.input_profile && a.source_sha256==b.source_sha256 &&
        a.input_sha256==b.input_sha256 && a.spatial_sha256==b.spatial_sha256 && a.input_matrix==b.input_matrix && a.input_spatial==b.input_spatial &&
        bool(a.spatial_frame)==bool(b.spatial_frame) && (!a.spatial_frame || a.spatial_frame->sha256()==b.spatial_frame->sha256()),
        "Matrix catalog changed metadata, domain, recipe keys, frame or source hashes");
}
void same_result(const ExpressionMatrixImportResult& a,const ExpressionMatrixImportResult& b,std::size_t work) {
    same_metadata(a.metadata,b.metadata);
    check(a.file_sha256==b.file_sha256 && a.file_bytes==b.file_bytes && a.matrix_bytes==b.matrix_bytes &&
        a.peak_work_bytes<=work && b.peak_work_bytes<=work && bytes(fs::u8path(a.path))==bytes(fs::u8path(b.path)),
        "Matrix dispatch changed QMX bytes/hash or lost native work accounting");
}
void same_block(const ExpressionSparseBlock& a,const ExpressionSparseBlock& b) {
    check(a.features==b.features && a.offsets==b.offsets && a.observation_indices==b.observation_indices && a.counts==b.counts &&
        a.continuous==b.continuous && a.domain==b.domain && a.source_sha256==b.source_sha256 && a.sha256==b.sha256,
        "Matrix dispatch changed exact sparse samples, axes, cohorts or chunk identity");
}
void same_source(const ExpressionMatrixSource& a,const ExpressionMatrixSource& b) {
    same_metadata(a.metadata(),b.metadata());check(a.file_sha256()==b.file_sha256(),"Matrix open changed file identity");
    const auto x=a.feature_page(0,std::size_t(a.metadata().features)),y=b.feature_page(0,std::size_t(b.metadata().features));
    check(x.size()==y.size(),"Matrix feature count changed");
    for(std::size_t i=0;i<x.size();++i)check(x[i].index==y[i].index && x[i].id==y[i].id && x[i].name==y[i].name &&
        x[i].type==y[i].type && x[i].sha256==y[i].sha256,"Matrix feature identity/order/hash changed");
    check(a.observation_page(0,std::size_t(a.metadata().observations))==b.observation_page(0,std::size_t(b.metadata().observations)),
        "Matrix observation axis changed");
}
mf::MatrixFormatSelection select(mf::MatrixFormat format) {return mf::select_matrix_format(format);}
std::shared_ptr<const ExpressionMatrixSource> open(const ExpressionMatrixImportResult& result,std::size_t work=64u*1024*1024) {
    return mf::open_matrix_format(select(mf::MatrixFormat::Qmx),{result.path,result.file_sha256,work,2ull*1024*1024*1024});
}
void catalog() {
    const std::vector<std::string> ids={"qmx","matrix-market","h5ad"};
    for(unsigned mask=0;mask<4;++mask) {
        const mf::MatrixFormatFeatures features{bool(mask&1),bool(mask&2)};const auto entries=mf::matrix_format_catalog(features);
        check(entries.size()==3,"Matrix catalog must retain every built-in format");
        for(std::size_t i=0;i<entries.size();++i) {
            const auto& entry=entries[i];const auto available=features.storage && (entry.format!=mf::MatrixFormat::H5ad || features.h5ad);
            check(entry.provider_id==ids[i] && !entry.display_name.empty() && mf::known_matrix_format(entry.format) &&
                select(entry.format).format==entry.format && entry.available==available && entry.unavailable_reason.empty()==available &&
                entry.can_open==(entry.format==mf::MatrixFormat::Qmx) && entry.can_import==(entry.format!=mf::MatrixFormat::Qmx),
                "Matrix catalog changed known/available distinction or operation bounds");
        }
    }
    const auto actual=mf::matrix_format_features();check(actual.storage==has_expression_matrix_storage() && actual.h5ad==has_h5ad(),
        "Matrix catalog disagrees with native backend availability");
    const auto unknown=static_cast<mf::MatrixFormat>(255);check(!mf::known_matrix_format(unknown),"Unknown matrix enum was admitted");
    invalid([&]{select(unknown);});invalid([&]{mf::open_matrix_format({unknown},{});});
    invalid([&]{mf::import_matrix_format({unknown},mf::MatrixMarketImportRequest{});});
    invalid([&]{mf::open_matrix_format(select(mf::MatrixFormat::H5ad),{});});
    invalid([&]{mf::import_matrix_format(select(mf::MatrixFormat::Qmx),mf::MatrixMarketImportRequest{});});
    invalid([&]{mf::import_matrix_format(select(mf::MatrixFormat::MatrixMarket),mf::H5adMatrixImportRequest{});});
    invalid([&]{mf::import_matrix_format(select(mf::MatrixFormat::H5ad),mf::MatrixMarketImportRequest{});});
}
H5adOptions h5_options(bool continuous=false) {
    H5adOptions options;options.domain=continuous?ExpressionValueDomain::Continuous:ExpressionValueDomain::Counts;
    options.value_domain=continuous?"explicit-processed-X":"raw-counts";
    options.frame.id="authored-spatial";options.frame.source_id="authored-fixture";options.frame.unit=CoordinateUnit::Micrometre;return options;
}
void factory_failures(const fs::path& root) {
    const auto absent=(root/"missing.qmx").u8string();
    for(bool cancel:{false,true}) {
        std::atomic<bool> cancelled{cancel};
        same_failure([&]{open_expression_matrix(absent,"",64u*1024*1024,4096,&cancelled);},
            [&]{mf::open_matrix_format(select(mf::MatrixFormat::Qmx),{absent,"",64u*1024*1024,4096},&cancelled);});
        mf::MatrixMarketImportRequest market{(root/"missing.mtx").u8string(),(root/"missing-features").u8string(),
            (root/"missing-observations").u8string(),(root/"failed-market.qmx").u8string(),{}};
        same_failure([&]{import_expression_matrix(market.matrix,market.features,market.observations,market.store,market.options,&cancelled);},
            [&]{mf::import_matrix_format(select(mf::MatrixFormat::MatrixMarket),market,&cancelled);});
        mf::H5adMatrixImportRequest h5{(root/"missing.h5ad").u8string(),(root/"failed-h5ad.qmx").u8string(),h5_options(),{}};
        same_failure([&]{import_h5ad_matrix(h5.input,h5.store,h5.options,h5.limits,&cancelled);},
            [&]{mf::import_matrix_format(select(mf::MatrixFormat::H5ad),h5,&cancelled);});
        check(!fs::exists(fs::u8path(market.store)) && !fs::exists(fs::u8path(h5.store)),"Failed matrix factory published output");
    }
    no_stage(root);
}
constexpr char tiny[]="%%MatrixMarket matrix coordinate integer general\n% unsorted original entries\n3 4 6\n3 4 7\n1 2 9007199254740993\n2 1 18446744073709551615\n1 1 5\n2 3 0\n3 1 11\n";
void matrix_market(const fs::path& root) {
    const auto matrix=root/"tiny.mtx",features=root/"features.tsv",observations=root/"observations.tsv";
    write(matrix,tiny);write(features,"feature_id\nG1\nG2\nG3\n");write(observations,"observation_id\nA\nB\nC\nD\n");
    ExpressionMatrixImportOptions options;options.work_bytes=8u*1024*1024;
    const auto direct=import_expression_matrix(matrix.u8string(),features.u8string(),observations.u8string(),(root/"direct.qmx").u8string(),options);
    mf::MatrixMarketImportRequest request{matrix.u8string(),features.u8string(),observations.u8string(),(root/"selected.qmx").u8string(),options};
    const auto selected=mf::import_matrix_format(select(mf::MatrixFormat::MatrixMarket),request);same_result(direct,selected,options.work_bytes);
    check(selected.file_sha256=="174b4ea68d6022fbab3a488b0a070d0a550853e42e410880707e6f5d8b0b6018" &&
        selected.metadata.domain==ExpressionValueDomain::Counts && selected.metadata.value_domain=="raw-counts" && selected.metadata.input_profile=="qpx-tsv",
        "Matrix registry changed the independently frozen QMX bytes/domain/profile");
    const auto a=open_expression_matrix(direct.path,direct.file_sha256),b=open(selected);same_source(*a,*b);
    const ExpressionMatrixSelection order{{2,0},std::nullopt};const auto block=b->read(order);same_block(a->read(order),block);
    check(block.offsets==std::vector<std::uint64_t>({0,2,4}) && block.observation_indices==std::vector<std::uint64_t>({0,3,0,1}) &&
        block.counts==std::vector<std::uint64_t>({11,7,5,UINT64_C(9007199254740993)}),"Independent feature-order/global-observation/uint64 oracle changed");
    const auto zeros=b->read({{1},std::nullopt});same_block(a->read({{1},std::nullopt}),zeros);
    check(zeros.counts==std::vector<std::uint64_t>({UINT64_MAX,0}) && zeros.observation_indices==std::vector<std::uint64_t>({0,2}),
        "Explicit stored zero or uint64 maximum was converted/dropped");
    const ExpressionMatrixSelection cohort{{0,1,2},std::vector<std::uint64_t>{1,3}};const auto subset=b->read(cohort);same_block(a->read(cohort),subset);
    check(subset.offsets==std::vector<std::uint64_t>({0,1,1,2}) && subset.observation_indices==std::vector<std::uint64_t>({1,3}) &&
        subset.counts==std::vector<std::uint64_t>({UINT64_C(9007199254740993),7}),"Cohort read confused global IDs or implicit zeros");
    const ExpressionMatrixSelection empty{{2,0},std::vector<std::uint64_t>{}};const auto none=b->read(empty);same_block(a->read(empty),none);
    check(none.counts.empty() && none.offsets==std::vector<std::uint64_t>({0,0,0}) && none.sha256!=block.sha256,
        "Empty cohort was treated as all observations or reused its identity");
    same_failure([&]{open_expression_matrix(selected.path,std::string(64,'a'));},
        [&]{mf::open_matrix_format(select(mf::MatrixFormat::Qmx),{selected.path,std::string(64,'a')});});
    same_failure([&]{open_expression_matrix(selected.path,"",1);},
        [&]{mf::open_matrix_format(select(mf::MatrixFormat::Qmx),{selected.path,"",1});});
    const auto original=bytes(fs::u8path(selected.path));
    same_failure([&]{import_expression_matrix(request.matrix,request.features,request.observations,request.store,request.options);},
        [&]{mf::import_matrix_format(select(mf::MatrixFormat::MatrixMarket),request);});
    check(bytes(fs::u8path(selected.path))==original,"Matrix registry overwrote an existing QMX output");
    for(unsigned mode=0;mode<4;++mode) {
        auto failed=request;failed.store=(root/("refused-"+std::to_string(mode)+".qmx")).u8string();
        std::atomic<bool> cancelled{mode==0};
        if(mode==1)failed.options.max_entries=5;
        if(mode==2)failed.options.file_bytes=1;
        if(mode==3)failed.options.work_bytes=0;
        same_failure([&]{import_expression_matrix(failed.matrix,failed.features,failed.observations,failed.store,failed.options,&cancelled);},
            [&]{mf::import_matrix_format(select(mf::MatrixFormat::MatrixMarket),failed,&cancelled);});
        check(!fs::exists(fs::u8path(failed.store)),"Cancelled/over-budget import published partial output");no_stage(root);
    }
    auto tenx=request;tenx.options.format=ExpressionCatalogueFormat::Tenx;tenx.features=(root/"headerless-features.tsv").u8string();
    tenx.observations=(root/"barcodes.tsv").u8string();tenx.store=(root/"tenx-selected.qmx").u8string();
    write(fs::u8path(tenx.features),"G1\tGene one\tGene Expression\nG2\tGene two\tGene Expression\nG3\tGene three\tGene Expression\n");write(fs::u8path(tenx.observations),"A\nB\nC\nD\n");
    const auto tenx_direct=import_expression_matrix(tenx.matrix,tenx.features,tenx.observations,(root/"tenx-direct.qmx").u8string(),tenx.options);
    const auto tenx_selected=mf::import_matrix_format(select(mf::MatrixFormat::MatrixMarket),tenx);same_result(tenx_direct,tenx_selected,tenx.options.work_bytes);
    const auto tenx_source=open(tenx_selected);check(tenx_source->metadata().input_profile=="tenx-tsv" && tenx_source->feature_page(0,1).front().name=="Gene one",
        "Matrix Market registry inferred or dropped the explicit catalogue profile");
    auto bad=request;bad.matrix=(root/"duplicate.mtx").u8string();bad.store=(root/"malformed.qmx").u8string();
    write(fs::u8path(bad.matrix),"%%MatrixMarket matrix coordinate integer general\n3 4 2\n1 1 5\n1 1 6\n");
    same_failure([&]{import_expression_matrix(bad.matrix,bad.features,bad.observations,bad.store,bad.options);},
        [&]{mf::import_matrix_format(select(mf::MatrixFormat::MatrixMarket),bad);});
    check(!fs::exists(fs::u8path(bad.store)),"Malformed matrix import published output");no_stage(root);
    const auto stale=root/"stale.qmx";fs::copy_file(fs::u8path(selected.path),stale);
    const auto before=open_expression_matrix(stale.u8string(),selected.file_sha256);
    const auto after=mf::open_matrix_format(select(mf::MatrixFormat::Qmx),{stale.u8string(),selected.file_sha256});
    {std::ofstream stream(stale,std::ios::binary|std::ios::app);stream<<'x';}
    same_failure([&]{before->require_current();},[&]{after->require_current();});
    fs::rename(matrix,root/"input-offline.mtx");fs::rename(features,root/"features-offline.tsv");fs::rename(observations,root/"observations-offline.tsv");
    check(open(selected)->read(order).sha256==block.sha256,"Opening QMX through registry needed original Matrix Market inputs");
}
void h5_oracle(const ExpressionSparseBlock& block,bool continuous,const std::vector<std::uint64_t>& observations) {
    const std::uint64_t counts[3][4]={{0,7,0,UINT64_C(9007199254740993)},{3,0,5,0},{0,11,0,0}};
    const double reals[3][4]={{0,-1.25,0,2.5},{.125,0,3.75,0},{0,11,0,0}};
    check(block.domain==(continuous?ExpressionValueDomain::Continuous:ExpressionValueDomain::Counts) &&
        (continuous?block.counts.empty():block.continuous.empty()),"H5AD registry confused declared domains");
    for(std::size_t f=0;f<block.features.size();++f) {
        auto at=block.offsets[f];for(auto observation:observations) {
            const bool present=at<block.offsets[f+1] && block.observation_indices[at]==observation;
            if(continuous)check((present?block.continuous[at]:0)==reals[observation][block.features[f]],"Independent H5AD continuous oracle changed");
            else check((present?block.counts[at]:0)==counts[observation][block.features[f]],"Independent H5AD uint64 oracle changed");
            if(present)++at;
        }
        check(at==block.offsets[f+1],"H5AD registry retained an unselected observation");
    }
}
unsigned h5ad(const fs::path& root) {
#ifndef QPX_H5AD_FIXTURES
    (void)root;throw std::runtime_error("Enabled H5AD matrix registry tests require QPX_H5AD_FIXTURES; assertions must not be skipped");
#else
    const fs::path fixtures(QPX_H5AD_FIXTURES);unsigned cases=0;
    ExpressionMatrixImportOptions limits;limits.work_bytes=64u*1024*1024;
    for(const std::string encoding:{"array","csr_matrix","csc_matrix"})for(bool continuous:{false,true}) {
        const auto name=encoding+(continuous?"-continuous":"");const auto input=root/(name+".h5ad");fs::copy_file(fixtures/(name+".h5ad"),input);
        const auto input_bytes=bytes(input);const auto options=h5_options(continuous);
        const auto direct=import_h5ad_matrix(input.u8string(),(root/(name+"-direct.qmx")).u8string(),options,limits);
        const auto selected=mf::import_matrix_format(select(mf::MatrixFormat::H5ad),mf::H5adMatrixImportRequest{
            input.u8string(),(root/(name+"-selected.qmx")).u8string(),options,limits});same_result(direct,selected,limits.work_bytes);
        const auto a=open_expression_matrix(direct.path,direct.file_sha256,limits.work_bytes),b=open(selected,limits.work_bytes);same_source(*a,*b);
        check(b->metadata().features==4 && b->metadata().observations==3 && b->metadata().entries==(encoding=="array"?12u:5u) &&
            b->metadata().domain==*options.domain && b->metadata().value_domain==options.value_domain && b->metadata().input_matrix=="X" &&
            b->metadata().input_spatial=="obsm/spatial" && b->metadata().spatial_frame && b->metadata().spatial_frame->sha256()==options.frame.sha256(),
            "H5AD registry changed explicit domain/frame/units/keys or source axes");
        for(const ExpressionMatrixSelection selection: {ExpressionMatrixSelection{{3,1,0,2},std::nullopt},
            ExpressionMatrixSelection{{3,1},std::vector<std::uint64_t>{0,2}}}) {
            const auto selected_block=b->read(selection);same_block(a->read(selection),selected_block);
            h5_oracle(selected_block,continuous,selection.observations?*selection.observations:std::vector<std::uint64_t>{0,1,2});
        }
        const auto positions=b->spatial_page(0,3);check(positions[0].id=="spot_c" && positions[0].position.x==100.25 &&
            positions[0].position.y==-.5 && positions[2].position.y==12.5,"H5AD registry changed embedded coordinate identity/order");
        check(bytes(input)==input_bytes,"H5AD dispatch mutated its original input");fs::remove(input);
        check(open(selected,limits.work_bytes)->spatial_page(0,1).front().position.y==-.5,"Indexed H5AD reopen needed original input");++cases;
    }
    const auto input=(fixtures/"csr_matrix.h5ad").u8string();
    for(unsigned mode=0;mode<4;++mode) {
        mf::H5adMatrixImportRequest request{input,(root/("h5ad-refused-"+std::to_string(mode)+".qmx")).u8string(),h5_options(),limits};
        std::atomic<bool> cancelled{mode==0};if(mode==1)request.options.domain.reset();
        if(mode==2)request.options.matrix="X/guess";if(mode==3)request.limits.max_features=1;
        same_failure([&]{import_h5ad_matrix(request.input,request.store,request.options,request.limits,&cancelled);},
            [&]{mf::import_matrix_format(select(mf::MatrixFormat::H5ad),request,&cancelled);});
        check(!fs::exists(fs::u8path(request.store)),"Refused/cancelled H5AD dispatch published output");no_stage(root);
    }
    auto options=h5_options();options.matrix="layers/counts";
    const auto direct=import_h5ad_matrix((fixtures/"selected-layer.h5ad").u8string(),(root/"layer-direct.qmx").u8string(),options,limits);
    const auto selected=mf::import_matrix_format(select(mf::MatrixFormat::H5ad),mf::H5adMatrixImportRequest{
        (fixtures/"selected-layer.h5ad").u8string(),(root/"layer-selected.qmx").u8string(),options,limits});same_result(direct,selected,limits.work_bytes);
    check(open(selected,limits.work_bytes)->metadata().input_matrix=="layers/counts","H5AD dispatch lost explicit layer selection");return cases+1;
#endif
}
}
int main() {
    try {
        catalog();const auto root=fs::temp_directory_path()/("qpx-matrix-formats-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
        check(fs::create_directory(root),"Create unique owned matrix-format test root");factory_failures(root);
        unsigned h5_cases=0;if(has_expression_matrix_storage()) {matrix_market(root);if(has_h5ad())h5_cases=h5ad(root);}
        std::cout<<"Matrix catalog cases=4; native storage assertions="<<has_expression_matrix_storage()<<"; H5AD oracle cases="<<h5_cases<<'\n';
    }catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}
}
