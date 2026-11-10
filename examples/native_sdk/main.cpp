#include <qupathxx/sdk.hpp>
#include <qupathxx/provenance.hpp>
#include <qupathxx/project.hpp>
#include <qupathxx/expression_normalization.hpp>
#include <qupathxx/expression_reduction.hpp>
#include <qupathxx/expression_differential.hpp>
#include <qupathxx/h5ad.hpp>
#include <qupathxx/ome_zarr.hpp>
#include <qupathxx/raw_image.hpp>
#include <qupathxx/classifier.hpp>
#include <qupathxx/interchange.hpp>
#ifdef QPX_SDK_TEST_density_workflow
#include <qupathxx/density_workflow.hpp>
#endif
#ifdef QPX_SDK_TEST_nucls_import
#include <qupathxx/nucls_import.hpp>
#endif
#include <filesystem>
#include <algorithm>
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>

namespace fs=std::filesystem;
using namespace qpx;
void require(bool ok,const char* message){if(!ok)throw std::runtime_error(message);}
void write(const fs::path& path,const std::string& text){
    std::ofstream out(path,std::ios::binary);out<<text;out.close();require(bool(out),"Cannot write SDK example input");
}
int main(int argc,char** argv){try{
    if(argc==4 && std::string(argv[1])=="--jpeg"){
        const auto server=open_tiff(argv[2]);const auto metadata=server->raw_metadata();
        require(bool(metadata),"No native raw TIFF capability");RawRegionRequest request;
        request.region={0,0,0,metadata->width,metadata->height};request.channels=metadata->channels;
        const auto raw=server->read_raw(request);std::ifstream expected(argv[3],std::ios::binary);
        require(bool(expected),"Missing independent expected JPEG bytes");
        const std::vector<std::uint8_t> bytes((std::istreambuf_iterator<char>(expected)),{});
        const auto* pixels=std::get_if<std::vector<std::uint8_t>>(&raw.storage());
        require(pixels && *pixels==bytes && std::all_of(raw.valid().begin(),raw.valid().end(),[](auto v){return v!=0;}),
            "Native JPEG TIFF samples differ from independent decoded bytes");
        server->close();std::cout<<"Native JPEG TIFF samples exact: "<<bytes.size()<<'\n';return 0;
    }
    if(argc!=2)throw std::invalid_argument("Usage: sdk_consumer NEW_RESULT_DIRECTORY");
    const auto root=fs::absolute(fs::u8path(argv[1]));
    require(fs::create_directory(root),"SDK example requires a new result directory");
    const auto environment=build_environment();
    require(environment->source_tree_sha256==sdk::source_sha256,"Installed header/library source identity differs");
    require(has_project()==sdk::project && has_interchange()==sdk::interchange && has_geos()==sdk::geos && has_openslide()==sdk::openslide &&
        has_tiff()==sdk::tiff && has_h5ad()==sdk::h5ad && has_ome_zarr()==sdk::ome_zarr &&
        has_expression_statistics()==sdk::statistics && has_classifier_training()==sdk::classifier &&
        has_expression_reduction()==sdk::linear_algebra,"Installed header/library features differ");
    require(sdk::project && sdk::statistics && sdk::linear_algebra,"This molecular example needs project/statistics/algebra backends");
#ifdef QPX_SDK_TEST_density_workflow
    ProjectDensityRecipe density;density.images={1};density.classes[0]={"window"};density.classes[1]={"tissue"};density.validate();
#endif
#ifdef QPX_SDK_TEST_nucls_import
    NuclsImportLimits limits;limits.validate();
#endif
    const std::vector<std::vector<std::uint64_t>> counts{
        {1,3,2,9,11,12},{20,10,15,5,7,6},{0,0,0,0,0,0},
        {5,5,5,5,5,5},{74,82,78,81,77,77}};
    write(root/"features.tsv","feature_id\nG0\nG1\nG2\nG3\nG4\n");
    write(root/"observations.tsv","observation_id\nO0\nO1\nO2\nO3\nO4\nO5\n");
    std::ostringstream entries;
    for(std::size_t f=0;f<counts.size();++f)for(std::size_t r=0;r<6;++r)
        if(counts[f][r])entries<<f+1<<' '<<r+1<<' '<<counts[f][r]<<'\n';
    write(root/"matrix.mtx","%%MatrixMarket matrix coordinate integer general\n5 6 24\n"+entries.str());
    const auto imported=import_expression_matrix((root/"matrix.mtx").u8string(),(root/"features.tsv").u8string(),
        (root/"observations.tsv").u8string(),(root/"source.qmx").u8string());
    const auto source=open_expression_matrix(imported.path,imported.file_sha256);
    const auto qc=expression_qc(source);
    require(qc->totals()==std::vector<std::uint64_t>(6,100),"Installed native QC changed exact source totals");
    const auto normalization=normalize_expression(source,qc);
    std::vector<ExpressionCohort> cohorts;
    for(std::size_t r=0;r<6;++r){const auto id="mouse"+std::to_string(r);cohorts.push_back(
        expression_observation_cohort(source,"cohort"+std::to_string(r),{"O"+std::to_string(r)},id,id,r<3?"A":"B"));}
    ExpressionAggregationOptions aggregation;aggregation.kind=ExpressionAggregationKind::Pseudobulk;
    const auto bulk=aggregate_expression(source,cohorts,aggregation);
    ExpressionDifferentialOptions comparison;comparison.group_a="A";comparison.group_b="B";
    const auto differential=differential_expression(bulk,comparison);
    require(differential->n_a()==3 && differential->n_b()==3 && differential->genes().size()==5 &&
        differential->genes()[2].status==ExpressionDifferentialStatus::AllZero &&
        differential->genes()[3].status==ExpressionDifferentialStatus::ZeroVariance,
        "Installed native DE associations/degenerate values differ");
    ExpressionReductionOptions reduction;reduction.components=2;reduction.clusters=2;reduction.seed=123;
    reduction.normalization_recipe=normalization.recipe();
    const auto fitted=reduce_expression(source,reduction);
    const auto archive=(root/"fitted.qpr").u8string();save_expression_reduction_archive(*fitted,archive);
    Project project;const auto image=project.add_document(std::make_shared<Document>(dense_image_server(Image::demo())));
    keep_expression_matrix(project,image,source,true);keep_expression_normalization(project,image,normalization);
    keep_expression_aggregation(project,image,*bulk);keep_expression_differential(project,image,*differential);
    keep_expression_reduction(project,image,*fitted,archive,true);
    project.save_new((root/"saved.qpx").u8string());
    const auto saved=Project::open((root/"saved.qpx").u8string());
    const auto replay=expression_differential_open_request(*saved,image,differential->id()).open(64u*1024*1024);
    const auto embedding=expression_reduction_open_request(*saved,image,fitted->id()).open(64u*1024*1024);
    require(replay->sha256()==differential->sha256() && replay->recipe()==differential->recipe() &&
        embedding->id()==fitted->id() && embedding->embedding()==fitted->embedding() &&
        embedding->labels()==fitted->labels(),"Installed native saved identity differs");
    save_expression_differential_csv(*replay,(root/"genes.csv").u8string());
    save_expression_differential_replicates_csv(*replay,(root/"replicates.csv").u8string());
    const auto report="{\"schema\":\"qpx.native-sdk-consumer/1\",\"state\":\"passed\",\"source_sha256\":\""+
        environment->source_tree_sha256+"\",\"de_record\":\""+replay->id()+"\",\"de_result\":\""+
        replay->sha256()+"\",\"pca_id\":\""+embedding->id()+"\",\"features\":5,\"biological_replicates\":6,\"saved_replay\":true}\n";
    write(root/"consumer.json",report);std::cout<<report;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
