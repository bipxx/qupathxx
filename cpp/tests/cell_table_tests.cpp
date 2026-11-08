#include "qupathxx/cell_tables.hpp"
#include "qupathxx/project.hpp"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <locale>
#include <random>
#include <stdexcept>
using namespace qpx;
void check_grid_cell_consumers();
void check(bool v) {if(!v) throw std::runtime_error("cell table check failed");}
template<class E,class F> void rejects(F f) {try {f();} catch(const E&) {return;}throw std::logic_error("missing cell table rejection");}
std::string read(const std::filesystem::path& path) {std::ifstream file(path,std::ios::binary);return {std::istreambuf_iterator<char>(file),{}};}
struct Comma:std::numpunct<char> {char do_decimal_point() const override {return ',';}};
int main() {
 try {
    check_grid_cell_consumers();
    std::vector<std::uint8_t> rgb(64*64*3,255);
    for(int y=0;y<64;++y) for(int x=0;x<64;++x) if((x-25)*(x-25)+(y-32)*(y-32)<=100 || (x-39)*(x-39)+(y-32)*(y-32)<=100) for(int c=0;c<3;++c) rgb[(y*64+x)*3+c]=0;
    Document doc(Image(64,64,rgb));const auto obj=doc.add_rectangle({0,0,64,64});CellParameters p;p.sigma=0;
    CellRequest request(doc,obj,StainTransform({{1,0,0},{0,1,0}}),p);const auto result=detect_cells(request);check(result->features().size()==2);
    const auto first=doc.keep_cells(request,result);CellTableRequest captured(doc);check(captured.row_count()==2 && captured.record_count()==1 && captured.document_revision()==doc.revision());
    const auto csv=cell_measurements_csv(captured);check(csv.find("qpx.cell-table/1")!=std::string::npos && csv.find("optical_density_rgb")!=std::string::npos && csv.find("\r\n")!=std::string::npos);
    const auto previous=std::locale::global(std::locale(std::locale::classic(),new Comma));check(cell_measurements_csv(captured)==csv);std::locale::global(previous);
    // Existing three-field C++ aggregate initialization keeps its original meaning.
    CellTableOptions options{false,256*1024*1024,256*1024*1024};check(!options.published_objects);
    options.byte_limit=csv.size();check(cell_measurements_csv(CellTableRequest(doc,{},options))==csv);
    options.byte_limit=csv.size()-1;rejects<std::length_error>([&]{cell_measurements_csv(CellTableRequest(doc,{},options));});
    options={};options.snapshot_budget=captured.snapshot_bytes();check(CellTableRequest(doc,{},options).snapshot_bytes()==captured.snapshot_bytes());
    --options.snapshot_budget;rejects<std::length_error>([&]{CellTableRequest invalid(doc,{},options);});
    auto copy=doc;options={};options.require_current=true;rejects<std::invalid_argument>([&]{CellTableRequest invalid(copy,{},options);});
    rejects<std::out_of_range>([&]{CellTableRequest invalid(doc,{999});});
    auto second=doc.keep_cells(request,result);check(cell_measurements_csv(CellTableRequest(doc,{second,first,second}))==cell_measurements_csv(CellTableRequest(doc,{first,second})));
    check(doc.remove_cells(first) && doc.remove_cells(second));check(cell_measurements_csv(captured)==csv);
    check(CellTableRequest(doc).row_count()==0 && cell_measurements_csv(CellTableRequest(doc)).find("qpx.cell-table/1")==std::string::npos);
    const auto path=std::filesystem::current_path()/("cell-table-"+std::to_string(std::random_device{}())+".csv");
    {std::ofstream file(path,std::ios::binary);file<<"sentinel";}
    std::atomic<bool> cancelled{true};rejects<std::runtime_error>([&]{save_cell_measurements_csv(captured,path.u8string(),&cancelled);});check(read(path)=="sentinel");
    CellTableExportJob job;check(job.request(captured,path.u8string()));auto status=job.wait();check(status.state=="succeeded" && status.rows==2 && status.completed==1 && status.bytes==csv.size() && read(path)==csv);
    const auto third=doc.keep_cells(request,result);options={};options.byte_limit=1;CellTableRequest short_table(doc,{third},options);
    check(job.request(short_table,path.u8string()));status=job.wait();check(status.state=="failed" && status.completed==1 && read(path)==csv);
    check(job.request(captured,path.u8string()) && job.wait().completed==2);job.cancel();
    std::filesystem::remove(path);
    if(has_project()) {
        Project project;const auto image=project.add_document(std::make_shared<Document>(doc),"quoted, image");
        const auto empty=project.add_document(std::make_shared<Document>(Image(2,2,std::vector<std::uint8_t>(12,255))));
        CellTableRequest all(project);check(all.project_scope() && all.image_count()==2 && all.record_count()==1 && all.row_count()==2 && all.document_revision()==0);
        const auto table=cell_measurements_csv(all);check(table.find("project_identity,project_generation,project_revision,image_id,image_name,")==0 && table.find("qpx.project-cell-table/1")!=std::string::npos);
        check(cell_measurements_csv(CellTableRequest(project,{empty,image,image}))==table);
        rejects<std::out_of_range>([&]{CellTableRequest missing(project,{999999});});
        options={};options.snapshot_budget=all.snapshot_bytes();check(CellTableRequest(project,{},options).snapshot_bytes()==all.snapshot_bytes());
        --options.snapshot_budget;rejects<std::length_error>([&]{CellTableRequest short_snapshot(project,{},options);});
        project.remove_image(image);project.remove_image(empty);check(cell_measurements_csv(all)==table && CellTableRequest(project).row_count()==0);
    }
    {
        Document manual(Image(16,16,std::vector<std::uint8_t>(16*16*3,255)));
        manual.add_cell(Roi::rectangle({1,2,4,6}),Roi::rectangle({2,3,1,2}),"manual");manual.set_calibration(.5,.25);
        options={};options.published_objects=true;CellTableRequest captured_objects(manual,{},options);
        auto table=cell_measurements_csv(captured_objects);check(captured_objects.row_count()==1 && captured_objects.record_count()==0);
        check(table.find("qpx.cell-object-table/1")!=std::string::npos && table.find("no_features")!=std::string::npos && table.find("object_area_um2")!=std::string::npos);
        options.require_current=true;rejects<std::invalid_argument>([&]{CellTableRequest invalid(manual,{},options);});
    }
    if(has_geos()) {
        const auto ids=doc.publish_cell_record(third);options={};options.published_objects=true;
        CellTableRequest joined(doc,{},options);const auto table=cell_measurements_csv(joined);
        check(joined.row_count()==ids.size() && joined.record_count()==1 && table.find("qpx.cell-object-table/1")!=std::string::npos);
        check(doc.remove_cells(third) && cell_measurements_csv(joined)==table);
        auto missing=cell_measurements_csv(CellTableRequest(doc,{},options));check(missing.find("archive_missing")!=std::string::npos);
        options.require_current=true;rejects<std::invalid_argument>([&]{CellTableRequest invalid(doc,{},options);});
    }
    std::cout<<"Cell tables, joined objects, locale, budgets, cancellation and atomic files passed\n";
 } catch(const std::exception& e) {std::cerr<<e.what()<<'\n';return 1;}
}
