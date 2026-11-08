#include "qupathxx/nucls_import.hpp"
#include "qupathxx/project.hpp"
#include "qupathxx/object_properties.hpp"
#include "qupathxx/spatial_analysis.hpp"
#include "nucls_png_fixture.hpp"
#include <nlohmann/json.hpp>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iostream>
#include <thread>
using namespace qpx;
namespace fs=std::filesystem;
using Json=nlohmann::json;
void checked(bool value,int line){if(!value)throw std::runtime_error("NuCLS import check at line "+std::to_string(line));}
#define check(...) checked((__VA_ARGS__),__LINE__)
template<class E,class F>void rejects_at(F action,int line){bool hit=false;try{action();}catch(const E&){hit=true;}checked(hit,line);}
#define rejects(E,...) rejects_at<E>(__VA_ARGS__,__LINE__)
void write(const fs::path& path,const std::string& text){std::ofstream file(path,std::ios::binary);file.write(text.data(),text.size());if(!file)throw std::runtime_error("Fixture write failed");}
std::string read(const fs::path& path){std::ifstream file(path,std::ios::binary);return {std::istreambuf_iterator<char>(file),{}};}
const std::vector<std::string> header={"raw_classification","main_classification","super_classification","type","xmin","ymin","xmax","ymax","coords_x","coords_y"};
using Rows=std::vector<std::vector<std::string>>;
Rows rows(){return {{"tumor",u8"\u80bf\u7624","tumor_any","rectangle","1","2","5","6","1,5,5,1","2,2,6,6"},{"lymphocyte","Lymphocyte","immune","polyline","2","1","8","7","2,8,2,2","1,1,7,1"}};}
std::string csv(const Rows& data,const std::vector<std::string>& columns=header){std::string out;const auto add=[&](const auto& values){bool first=true;for(const auto& value:values){if(!first)out+=',';first=false;if(value.find_first_of(",\"\r\n")!=std::string::npos){out+='"';for(auto c:value){out+=c;if(c=='"')out+='"';}out+='"';}else out+=value;}out+="\r\n";};add(columns);for(const auto& row:data)add(row);return out;}
struct Scene {
    fs::path images,annotations;
    explicit Scene(const fs::path& root,bool missing=false):images(root/"rgb"),annotations(root/"csv") {
        fs::create_directories(images);fs::create_directory(annotations);
        std::vector<std::uint8_t> pixels(12*10*3);for(std::size_t i=0;i<pixels.size();i+=3){pixels[i]=30;pixels[i+1]=60;pixels[i+2]=90;}
        const auto png=nucls_png_fixture::png(12,10,nucls_png_fixture::filtered(pixels,12,10,0));
        for(const std::string name:{"Case A","Case B"}){write(images/(name+".png"),png);if(!missing || name!="Case B")write(annotations/(name+".csv"),csv(rows()));}
    }
    NuclsImportReport run(NuclsImportOptions options={},const fs::path& output={}){if(options.mpp_x==0){options.mpp_x=.4;options.mpp_y=.7;}return import_nucls(images.u8string(),annotations.u8string(),options,output.u8string());}
};
struct Outcome {std::shared_ptr<const NuclsImportReport> result;std::string error;};
Outcome controlled(Scene& scene,const fs::path& output,const std::function<void(const Json&,NuclsImportJob&)>& action) {
    NuclsImportOptions options;options.mpp_x=.4;options.mpp_y=.7;options.pause_after_pair=true;NuclsImportJob job;
    check(job.run(scene.images.u8string(),scene.annotations.u8string(),options,output.u8string()));
    check(!job.run(scene.images.u8string(),scene.annotations.u8string(),options,output.u8string()));
    const auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(20);
    while(job.busy()){for(const auto& event:job.take_pairs()){action(Json::parse(event),job);job.continue_pairs();}job.poll();if(job.busy() && std::chrono::steady_clock::now()>deadline){job.finish();check(false);}std::this_thread::sleep_for(std::chrono::milliseconds(1));}
    return {job.result(),job.error()};
}
int main(){try{
    Project id;const auto root=fs::current_path()/(id.identity()+"-nucls-import");fs::create_directory(root);Scene scene(root/"base");
    const auto image_before=read(scene.images/"Case A.png"),csv_before=read(scene.annotations/"Case A.csv");
    const auto preview=scene.run();check(preview.valid && !preview.written && preview.images.size()==2 && preview.images[0].imported==2);
    check(preview.images[0].classes.at(u8"\u80bf\u7624")==1 && preview.images[0].classes.at("Lymphocyte")==1);
    for(const std::string representation:{"outlines","centroids"}) {
        NuclsImportOptions options;options.representation=representation;const auto output=root/(representation+".qpxproj");const auto result=scene.run(options,output);
        check(result.valid && result.written && Json::parse(result.json)["written"]==true);auto saved=Project::open_without_journal_recovery(output.u8string());
        check(saved->images().size()==2 && saved->images()[0].name=="Case A" && saved->images()[1].name=="Case B");
        for(const auto& image:saved->images()) {
            const auto doc=saved->document(image.id);check(doc->pixel_width_um()==.4 && doc->pixel_height_um()==.7);const auto objects=doc->annotations();
            check(objects.size()==2 && objects[0].id==1 && objects[1].id==2 && objects[0].kind==ObjectKind::Cell && objects[0].nucleus && objects[1].nucleus);
            check(objects[0].class_name==u8"\u80bf\u7624" && objects[1].class_name=="Lymphocyte");
            check(objects[0].geometry.area()==(representation=="outlines"?16:1) && objects[1].geometry.area()==(representation=="outlines"?18:1));
            const auto props=objects[0].properties->metadata();check(props.at("raw_classification")=="tumor" && props.at("source_row")=="1" && props.at("representation")==representation);
            check(saved->image_metadata(image.id).at("image_sha256")==result.images[image.id-1].image_sha256);
            SpatialDistanceRequest request(*doc,{1,2},{1,2});check(request.source_object(0).centroid.x==3 && request.source_object(0).centroid.y==4);
            check(request.source_object(1).centroid.x==4 && request.source_object(1).centroid.y==3);
            const auto pixels=doc->server()->read_region({0,0,0,12,10}).bytes();check(pixels.size()==12*10*4 && pixels[0]==30 && pixels[1]==60 && pixels[2]==90 && pixels[3]==255);
        }
    }
    check(read(scene.images/"Case A.png")==image_before && read(scene.annotations/"Case A.csv")==csv_before);
    NuclsImportOptions subset;subset.names=std::vector<std::string>{"Case B"};subset.class_column="super_classification";const auto selected=scene.run(subset);check(selected.valid && selected.images.size()==1 && selected.images[0].classes.at("tumor_any")==1);
    subset.names=std::vector<std::string>{"case b"};check(!scene.run(subset).valid);
    Scene crowded(root/"crowded");for(unsigned i=0;i<101;++i)write(crowded.images/("unselected-"+std::to_string(i)+".png"),"");
    NuclsImportOptions only_one;only_one.names=std::vector<std::string>{"Case B"};check(crowded.run(only_one).valid);
    rejects(std::invalid_argument,[&]{crowded.run();});
    Scene missing(root/"missing",true);write(missing.images/"orphan.png",image_before);const auto missing_result=missing.run();check(!missing_result.valid && missing_result.images.size()==3 && missing_result.images[1].issue_count==1 && missing_result.images[2].issue_count==1);
    for(const std::string field:{"xmin","xmax","type","main_classification"}) {
        auto bad=rows();const auto at=std::find(header.begin(),header.end(),field)-header.begin();bad[0][at]=field=="xmin"?"nan":field=="xmax"?"1":field=="type"?"ellipse":" padded ";write(scene.annotations/"Case A.csv",csv(bad));
        const auto report=scene.run({},root/(field+"-blocked.qpxproj"));check(!report.valid && !report.written && report.images[0].issues[0].find("annotation 1 (CSV line 2):")==0);
    }
    auto outside=rows();outside[0][6]="13";write(scene.annotations/"Case A.csv",csv(outside));check(!scene.run().valid);
    NuclsImportOptions markers;markers.representation="centroids";const auto markers_report=scene.run(markers,root/"outside-markers.qpxproj");check(markers_report.written && markers_report.images[0].source_bounds_outside==1);
    check(Project::open_without_journal_recovery(markers_report.output)->document(1)->annotation(1).geometry.bounds().x==6.5);
    outside[0][4]="-.4";outside[0][6]=".6";write(scene.annotations/"Case A.csv",csv(outside));check(!scene.run(markers).valid);
    auto invalid_polygon=rows();invalid_polygon[1][8]="2,8,2,8,2";invalid_polygon[1][9]="1,7,7,1,1";write(scene.annotations/"Case A.csv",csv(invalid_polygon));check(!scene.run(markers).valid);
    invalid_polygon=rows();invalid_polygon[1][9]="1,1,7";write(scene.annotations/"Case A.csv",csv(invalid_polygon));check(!scene.run().valid);
    auto duplicate=header;duplicate.push_back("type");write(scene.annotations/"Case A.csv",csv({},duplicate));check(!scene.run().valid);
    auto bad_width=rows();bad_width[0].pop_back();write(scene.annotations/"Case A.csv",csv(bad_width));check(!scene.run().valid);
    auto multiline=rows();multiline[0][0]="tumor\nsource";multiline[0][4]="nan";write(scene.annotations/"Case A.csv",csv(multiline));check(scene.run().images[0].issues[0].find("CSV line 3")!=std::string::npos);
    auto bad=rows()[0];bad[4]="nan";write(scene.annotations/"Case A.csv",csv(Rows(60,bad)));const auto bounded=scene.run();check(bounded.images[0].issue_count==60 && bounded.images[0].issues.size()==50);
    write(scene.annotations/"Case A.csv",csv(rows()));
    NuclsImportOptions limits;limits.limits.pixels=1;const auto pixel_limit=scene.run(limits);check(!pixel_limit.valid && pixel_limit.images.size()==1);
    limits={};limits.limits.rows=1;check(!scene.run(limits).valid);limits={};limits.limits.vertices=3;check(!scene.run(limits).valid);
    NuclsImportOptions report_limit;report_limit.limits.report_bytes=1;
    rejects(std::length_error,[&]{scene.run(report_limit,root/"report-limit.qpxproj");});check(!fs::exists(root/"report-limit.qpxproj"));
    const auto existing=root/"existing.qpxproj";write(existing,"preserve");rejects(WorkflowOutputExists,[&]{scene.run({},existing);});check(read(existing)=="preserve");
    auto invalid=NuclsImportOptions{};invalid.mpp_x=-1;invalid.mpp_y=.7;rejects(std::invalid_argument,[&]{import_nucls(scene.images.u8string(),scene.annotations.u8string(),invalid);});
    invalid.mpp_x=.4;invalid.names=std::vector<std::string>{"../escape"};rejects(std::invalid_argument,[&]{invalid.validate();});invalid.names=std::vector<std::string>{};rejects(std::invalid_argument,[&]{invalid.validate();});
    Scene cancellation(root/"cancel");const auto cancelled=controlled(cancellation,root/"cancelled.qpxproj",[](const Json&,NuclsImportJob& job){job.cancel();});check(cancelled.error.empty() && cancelled.result && cancelled.result->cancelled && !cancelled.result->written && !fs::exists(root/"cancelled.qpxproj"));
    Scene mutation(root/"mutation");const auto changed=controlled(mutation,root/"changed.qpxproj",[&](const Json& pair,NuclsImportJob&){if(pair["name"]=="Case A")write(mutation.annotations/"Case A.csv",read(mutation.annotations/"Case A.csv")+"\n");});check(!changed.result && changed.error.find("changed")!=std::string::npos && !fs::exists(root/"changed.qpxproj"));
    Scene collision(root/"collision");const auto collision_output=root/"collision.qpxproj";const auto collided=controlled(collision,collision_output,[&](const Json& pair,NuclsImportJob&){if(pair["name"]=="Case A")write(collision_output,"preserve");});check(!collided.result && !collided.error.empty() && read(collision_output)=="preserve");
    std::cout<<"Native NuCLS: exact pairing/CSV rows, RGB/calibration/Cell geometry/metadata, centroid markers, bounded issues, limits, source guards, cancellation and atomic no-overwrite passed\n";
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
