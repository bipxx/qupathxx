#include "qupathxx/measurement_tables.hpp"
#include "qupathxx/project.hpp"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <locale>
#include <random>
#include <stdexcept>
using namespace qpx;
void check(bool value) {if(!value) throw std::runtime_error("measurement table check failed");}
template<class E,class F> void rejects(F f) {try {f();} catch(const E&) {return;}throw std::logic_error("missing measurement table rejection");}
std::string read(const std::filesystem::path& path) {std::ifstream f(path,std::ios::binary);return {std::istreambuf_iterator<char>(f),{}};}
struct Comma:std::numpunct<char> {char do_decimal_point() const override {return ',';}};
int main() {
 try {
    Document doc(Image(32,24,std::vector<std::uint8_t>(32*24*3,16)));const auto obj=doc.add_rectangle({1,2,20,16});doc.set_calibration(.5,.25);
    StainTransform transform({{1,0,0},{0,1,0}});AnalysisRequest source(doc,obj,transform);const auto result=analyze(source);const auto id=doc.keep_analysis(source,result);
    MeasurementTableRequest captured(doc,MeasurementTableKind::Roi);const auto csv=measurements_csv(captured);
    check(captured.row_count()==1 && captured.record_count()==1 && captured.image_count()==1 && csv.find("qpx.roi-table/1")!=std::string::npos);
    const auto previous=std::locale::global(std::locale(std::locale::classic(),new Comma));check(measurements_csv(captured)==csv);std::locale::global(previous);
    MeasurementTableOptions options;options.byte_limit=csv.size();check(measurements_csv(MeasurementTableRequest(doc,MeasurementTableKind::Roi,{},options))==csv);
    --options.byte_limit;rejects<std::length_error>([&]{measurements_csv(MeasurementTableRequest(doc,MeasurementTableKind::Roi,{},options));});
    options={};options.snapshot_budget=captured.snapshot_bytes();check(MeasurementTableRequest(doc,MeasurementTableKind::Roi,{},options).snapshot_bytes()==captured.snapshot_bytes());
    --options.snapshot_budget;rejects<std::length_error>([&]{MeasurementTableRequest bad(doc,MeasurementTableKind::Roi,{},options);});
    options={};options.require_current=true;auto copy=doc;rejects<std::invalid_argument>([&]{MeasurementTableRequest bad(copy,MeasurementTableKind::Roi,{},options);});
    rejects<std::out_of_range>([&]{MeasurementTableRequest bad(doc,MeasurementTableKind::Roi,{999999});});
    rejects<std::invalid_argument>([&]{MeasurementTableRequest bad(doc,static_cast<MeasurementTableKind>(55));});
    TissueRequest tissue_source(doc,obj,transform);const auto mask=detect_tissue(tissue_source);doc.keep_tissue(tissue_source,mask);
    MeasurementTableRequest tissue(doc,MeasurementTableKind::Tissue);check(tissue.row_count()==1 && tissue.record_count()==1 && measurements_csv(tissue).find("qpx.tissue-table/1")!=std::string::npos);
    check(doc.remove_analysis(id));check(measurements_csv(captured)==csv);
    const auto path=std::filesystem::current_path()/("measurement-table-"+std::to_string(std::random_device{}())+".csv");
    {std::ofstream f(path);f<<"sentinel";}
    std::atomic<bool> cancelled{true};rejects<std::runtime_error>([&]{save_measurements_csv(captured,path.u8string(),&cancelled);});check(read(path)=="sentinel");
    MeasurementTableExportJob job;check(job.request(captured,path.u8string()));auto status=job.wait();check(status.state=="succeeded" && status.completed==1 && status.rows==1 && read(path)==csv);
    options={};options.byte_limit=1;check(job.request(MeasurementTableRequest(doc,MeasurementTableKind::Tissue,{},options),path.u8string()));check(job.wait().state=="failed" && read(path)==csv);
    check(job.request(captured,path.u8string()) && job.wait().completed==2);std::filesystem::remove(path);
    if(has_project()) {
        Project project;const auto image=project.add_document(std::make_shared<Document>(doc),"quoted, image");
        MeasurementTableRequest all(project,MeasurementTableKind::Tissue);const auto table=measurements_csv(all);check(all.project_scope() && all.row_count()==1 && table.find("qpx.project-tissue-table/1")!=std::string::npos);
        project.remove_image(image);check(measurements_csv(all)==table);
    }
    std::cout<<"Measurement tables, snapshots, locale, bounds, cancellation and atomic jobs passed\n";
 } catch(const std::exception& e) {std::cerr<<e.what()<<'\n';return 1;}
}
