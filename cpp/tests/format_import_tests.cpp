#include "qupathxx/format_import.hpp"
#include "qupathxx/file_fingerprint.hpp"
#include "qupathxx/object_properties.hpp"
#include "qupathxx/ome_zarr.hpp"
#include "qupathxx/project.hpp"
#include "qupathxx/raw_image.hpp"
#include "qupathxx/reader_plugins.hpp"
#include "qupathxx/workflow.hpp"
#include "format_import_internal.hpp"
#include "reader_registry.hpp"
#include <algorithm>
#include <atomic>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <future>
#include <iostream>
#include <set>
#include <stdexcept>
#include <thread>
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif
namespace fs=std::filesystem;
using namespace qpx;
namespace {
void check(bool b,const std::string& s){if(!b)throw std::runtime_error(s);}
template<class E=std::exception,class F>std::string refuses(F&& f){try{f();}catch(const E& e){return e.what();}throw std::runtime_error("Missing native publication refusal");}
std::string bytes(const fs::path& p){std::ifstream in(p,std::ios::binary);check(bool(in),"Read owned fixture");return {std::istreambuf_iterator<char>(in),{}};}
void write(const fs::path& p,const std::string& s){std::ofstream out(p,std::ios::binary);out<<s;out.close();check(bool(out),"Write owned fixture");}
void no_stage(const fs::path& root){for(const auto& p:fs::directory_iterator(root))check(p.path().filename().u8string().rfind(".qpx-expression-",0)!=0,"Private QMX stage leaked");}
std::string sha(const fs::path& p){return fingerprint_file(p.u8string())->sha256;}
MatrixPluginImportOptions matrix_options(){MatrixPluginImportOptions o;o.frame.id="literal-positions";o.frame.source_id="authored-position-frame";o.frame.unit=CoordinateUnit::Pixel;o.frame.plane={-1,0,0};o.limits.work_bytes=32u*1024*1024;o.plugin_work_bytes=1u*1024*1024;return o;}
std::shared_ptr<Document> target(const fs::path& fixtures){auto server=open_tiff((fixtures/"image.tif").u8string());const auto metadata=server->raw_metadata();check(metadata&&metadata->width==16&&metadata->height==16&&metadata->channels==3&&metadata->z_planes==1&&metadata->timepoints==1&&metadata->sample_type==SampleType::UInt8&&server->raw_view(),"Authored quantitative TIFF metadata differs");RawRegionRequest control;control.region={0,0,0,16,16};control.channels=3;const auto raw=server->read_raw(control);check(std::get<std::vector<std::uint8_t>>(raw.storage())==std::vector<std::uint8_t>(768,80)&&raw.valid()==std::vector<std::uint8_t>(256,1),"Authored quantitative TIFF raw samples differ");auto d=std::make_shared<Document>(std::move(server));d->set_calibration(.25,.5);d->set_class_definition(ObjectClass::definition("Native Tumor",0x123456));d->set_class_definition(ObjectClass::definition("Native Stroma",0x654321));const auto sentinel=d->add_rectangle({12,12,1,1},"Sentinel");d->set_object_properties(sentinel,std::make_shared<const ObjectProperties>(std::map<std::string,std::string>{{"note","unaltered"}}));d->clear_history();return d;}
AnnotationPluginImportOptions annotation_options(){AnnotationPluginImportOptions o;o.class_map={{"T","Native Tumor"},{"S","Native Stroma"}};o.work_bytes=32u*1024*1024;o.plugin_work_bytes=1u*1024*1024;return o;}
struct Snapshot{std::uint64_t revision;std::map<std::uint64_t,Annotation> objects;std::map<std::string,ObjectClass> classes;bool undo,redo;std::string undo_label,redo_label;};
Snapshot snapshot(const Document& d){return {d.revision(),d.objects(),d.classes(),d.can_undo(),d.can_redo(),d.undo_label(),d.redo_label()};}
void unchanged(const Document& d,const Snapshot& s){check(d.revision()==s.revision&&d.object_count()==s.objects.size()&&d.classes()==s.classes&&d.can_undo()==s.undo&&d.can_redo()==s.redo&&d.undo_label()==s.undo_label&&d.redo_label()==s.redo_label,"Refusal changed document/revision/classes/undo");for(const auto& [id,a]:s.objects){const auto b=d.annotation(id);check(a.id==b.id&&a.name==b.name&&a.parent_id==b.parent_id&&a.class_name==b.class_name&&a.kind==b.kind&&a.plane==b.plane&&a.revision==b.revision&&a.geometry.equals(b.geometry)&&bool(a.properties)==bool(b.properties),"Refusal changed existing native object");if(a.properties)check(a.properties->equals(*b.properties),"Refusal changed object properties");}}
void check_matrix(const ExpressionMatrixSource& s){const auto& m=s.metadata();check(m.features==3&&m.observations==3&&m.entries==9&&m.domain==ExpressionValueDomain::Counts&&m.value_domain=="raw-counts"&&m.input_profile=="external-dense-counts/1"&&m.spatial_frame&&m.spatial_frame->id=="literal-positions","External matrix schema/profile/frame changed");const auto f=s.feature_page(0,3);check(f.size()==3&&f[0].id=="0002"&&f[1].id=="2"&&f[2].id=="zero-only","Feature IDs normalized or reordered");for(const auto& x:f)check(x.name==x.id&&x.type=="feature"&&x.sha256.size()==64,"Feature name fallback or generic type changed");check(s.observation_page(0,3)==std::vector<std::string>{"0001","1","18446744073709551615"},"Observation IDs lost precision/leading zeros");const auto p=s.spatial_page(0,3);check(p.size()==3,"Coordinate count");for(unsigned i=0;i<3;++i)check(p[i].position.x==10.25+10*i&&p[i].position.y==20.5+10*i,"Positions were joined by row order instead of exact ID");ExpressionMatrixSelection q;q.features={0,1,2};const auto block=s.read(q);check(block.offsets==std::vector<std::uint64_t>{0,3,6,9}&&block.observation_indices==std::vector<std::uint64_t>{0,1,2,0,1,2,0,1,2}&&block.counts==std::vector<std::uint64_t>{UINT64_MAX,0,11,UINT64_C(9007199254740993),7,0,0,0,0}&&block.continuous.empty(),"Exact UInt64 sparse projection/explicit zeros changed");}
void matrix_case(const fs::path& f,const fs::path& w){const auto options=matrix_options();const auto r=import_expression_matrix_plugin("org.qupathxx.counts-tsv",(f/"counts-main.tsv").u8string(),(f/"positions.tsv").u8string(),(w/"counts.qmx").u8string(),options);auto s=open_expression_matrix(r.path,r.file_sha256);check_matrix(*s);const auto receipt=external_expression_matrix_receipt(*s);check(receipt&&receipt->reader.provider.provider_id=="org.qupathxx.counts-tsv"&&receipt->reader.provider.contract=="dense-observation-feature-uint64-tsv/1"&&receipt->reader.options.empty()&&receipt->reader.source_sha256==sha(f/"counts-main.tsv")&&receipt->positions_sha256==sha(f/"positions.tsv")&&receipt->position_columns==std::array<std::string,3>{"observation_id","x_px","y_px"},"External native receipt is not bound to exact inputs");const auto before=bytes(w/"counts.qmx");refuses<WorkflowOutputExists>([&]{import_expression_matrix_plugin("org.qupathxx.counts-tsv",(f/"counts-main.tsv").u8string(),(f/"positions.tsv").u8string(),r.path,options);});check(bytes(w/"counts.qmx")==before,"Existing QMX overwritten");auto changed=options;changed.frame.id="different-explicit-frame";const auto other=import_expression_matrix_plugin("org.qupathxx.counts-tsv",(f/"counts-main.tsv").u8string(),(f/"positions.tsv").u8string(),(w/"other-frame.qmx").u8string(),changed);check(other.metadata.source_sha256!=r.metadata.source_sha256&&other.file_sha256!=r.file_sha256,"Explicit frame change aliases old scientific source");no_stage(w);}
void check_point(const Annotation& object,unsigned row,const FormatPluginReceipt& receipt,const std::string& frame,const std::string& preview){const std::array<std::string,3> ids={"0001","1","18446744073709551615"},labels={"T","","S"},classes={"Native Tumor","","Native Stroma"};check(object.name==ids[row]&&object.parent_id==0&&object.kind==ObjectKind::Annotation&&object.geometry.equals(Roi::points({{1.25+row,2.5+row}}))&&object.class_name==classes[row]&&object.plane==ObjectPlane(row==1?0:-1,0,0)&&object.properties,"Point coordinates/plane/class/source ID changed");const auto& props=object.properties->metadata();const std::string p="qpx.annotation-plugin.";check(props.at(p+"schema")=="qpx.annotation-plugin-object/1"&&props.at(p+"provider_id")==receipt.provider.provider_id&&props.at(p+"format_id")==receipt.provider.format_id&&props.at(p+"implementation_sha256")==receipt.provider.implementation_sha256&&props.at(p+"module_sha256")==receipt.provider.module_sha256&&props.at(p+"options")==receipt.options&&props.at(p+"source_sha256")==receipt.source_sha256&&props.at(p+"receipt_sha256")==format_plugin_receipt_identity(receipt)&&props.at(p+"source_id")==ids[row]&&props.at(p+"source_class")==labels[row]&&props.at(p+"native_class")==classes[row]&&props.at(p+"source_row")==std::to_string(row)&&props.at(p+"frame_sha256")==frame&&props.at(p+"recipe_sha256").size()==64&&props.at(p+"row_sha256").size()==64&&preview.size()==64,"Exact saved annotation receipt/source mapping differs");}
void annotation_case(const fs::path& f,const fs::path& w){auto d=target(f);const auto before=snapshot(*d);auto p=preview_annotation_plugin(*d,"org.qupathxx.point-tsv",(f/"points-main.tsv").u8string(),annotation_options());unchanged(*d,before);check(!p.consumed()&&p.objects().size()==3&&p.sha256().size()==64&&p.payload_bytes()>0&&p.frame().sha256()==annotation_plugin_target_frame(*d).sha256(),"Preview is not owned/reviewable/current");for(unsigned i=0;i<3;++i)check_point(p.objects()[i],i,p.receipt(),p.frame().sha256(),p.sha256());const auto preview_sha=p.sha256(),frame_sha=p.frame().sha256();const auto receipt=p.receipt();const auto applied=apply_annotation_plugin(*d,p);check(p.consumed()&&applied.objects.size()==3&&applied.preview_sha256==preview_sha&&format_plugin_receipt_identity(applied.receipt)==format_plugin_receipt_identity(receipt)&&d->object_count()==4&&d->can_undo()&&d->undo_label()=="Import plugin annotations","Apply did not publish one guarded transaction");std::set<std::uint64_t> native_ids;for(unsigned i=0;i<3;++i){check(applied.objects[i].source_id==p.objects()[i].name&&applied.objects[i].object_id!=1&&native_ids.insert(applied.objects[i].object_id).second,"Source IDs did not map to fresh unique native IDs");check_point(d->annotation(applied.objects[i].object_id),i,receipt,frame_sha,preview_sha);}check(d->annotation(1).properties->equals(*before.objects.at(1).properties),"Existing object properties changed");const auto committed=snapshot(*d);refuses<std::logic_error>([&]{apply_annotation_plugin(*d,p);});unchanged(*d,committed);check(d->undo()&&d->object_count()==1&&d->classes()==before.classes&&!d->can_undo()&&d->can_redo(),"One undo did not remove exactly the imported batch");check(d->redo()&&d->object_count()==4&&!d->can_redo(),"One redo did not restore batch");for(unsigned i=0;i<3;++i)check_point(d->annotation(applied.objects[i].object_id),i,receipt,frame_sha,preview_sha);Project project;const auto image=project.add_document(d,"Typed plugin points");project.save((w/"points.qpproj").u8string());const auto reopened=Project::open((w/"points.qpproj").u8string())->document(image);for(unsigned i=0;i<3;++i)check_point(reopened->annotation(applied.objects[i].object_id),i,receipt,frame_sha,preview_sha);
    auto outside=target(f);const auto outside_before=snapshot(*outside);auto out=preview_annotation_plugin(*outside,"org.qupathxx.point-tsv",(f/"points-outside.tsv").u8string(),{});unchanged(*outside,outside_before);check(out.objects().size()==1&&out.objects()[0].geometry.equals(Roi::points({{1000.25,2000.5}})),"Reviewable preview changed literal outside-image coordinates");check(refuses<std::invalid_argument>([&]{apply_annotation_plugin(*outside,out);})=="ROI must lie inside the image","Native outside-image publication did not reach exact append bounds refusal");unchanged(*outside,outside_before);check(!out.consumed(),"Refused outside-image publication consumed preview");auto valid=preview_annotation_plugin(*outside,"org.qupathxx.point-tsv",(f/"points-empty-final-class.tsv").u8string(),{});const auto valid_result=apply_annotation_plugin(*outside,valid);check(valid_result.objects.size()==1&&valid_result.objects[0].object_id==2,"Refused outside-image append consumed native object IDs");
}
void stale_case(const fs::path& f,const fs::path& w){
    for(const std::string mode:{"document","revision","properties","registry","calibration","closed-server","source"}){
        auto d=target(f);auto other=target(f);const auto input=w/(mode+".tsv");write(input,bytes(f/"points-main.tsv"));auto p=preview_annotation_plugin(*d,"org.qupathxx.point-tsv",input.u8string(),annotation_options());Document* destination=d.get();
        if(mode=="document")destination=other.get();else if(mode=="revision")d->rename_annotation(1,"Changed");else if(mode=="properties")d->set_object_properties(1,std::make_shared<const ObjectProperties>(std::map<std::string,std::string>{{"note","changed"}}));else if(mode=="registry")d->set_class_definition(ObjectClass::definition("Native Tumor",0x999999));else if(mode=="calibration")d->set_calibration(.5,.5);else if(mode=="closed-server")d->server()->close();else if(mode=="source"){auto s=bytes(input);s.replace(s.find("0001"),4,"0002");write(input,s);}
        const auto before=snapshot(*destination);refuses<WorkflowInputChanged>([&]{apply_annotation_plugin(*destination,p);});unchanged(*destination,before);check(!p.consumed(),"Stale refusal consumed preview");
    }
    auto d=target(f);const auto before=snapshot(*d);auto opts=annotation_options();opts.frame_id="invented-frame";refuses<std::invalid_argument>([&]{preview_annotation_plugin(*d,"org.qupathxx.point-tsv",(f/"points-main.tsv").u8string(),opts);});unchanged(*d,before);opts=annotation_options();opts.class_map["T"]="Absent native class";refuses<std::invalid_argument>([&]{preview_annotation_plugin(*d,"org.qupathxx.point-tsv",(f/"points-main.tsv").u8string(),opts);});unchanged(*d,before);opts=annotation_options();opts.class_map.erase("S");refuses<std::invalid_argument>([&]{preview_annotation_plugin(*d,"org.qupathxx.point-tsv",(f/"points-main.tsv").u8string(),opts);});unchanged(*d,before);
    auto text=bytes(f/"points-main.tsv");const std::string valid_plane="1.25\t2.5\t-1\t0\t0";text.replace(text.find(valid_plane),valid_plane.size(),"1.25\t2.5\t-1\t1\t0");write(w/"bad-plane.tsv",text);refuses([&]{preview_annotation_plugin(*d,"org.qupathxx.point-tsv",(w/"bad-plane.tsv").u8string(),annotation_options());});unchanged(*d,before);
}
void cancel_case(const fs::path& f,const fs::path& w){auto d=target(f);const auto before=snapshot(*d);std::atomic<bool> cancelled{true};refuses<WorkflowCancelled>([&]{preview_annotation_plugin(*d,"org.qupathxx.point-tsv",(f/"points-main.tsv").u8string(),annotation_options(),std::nullopt,&cancelled);});unchanged(*d,before);cancelled=false;auto p=preview_annotation_plugin(*d,"org.qupathxx.point-tsv",(f/"points-main.tsv").u8string(),annotation_options());cancelled=true;refuses<WorkflowCancelled>([&]{apply_annotation_plugin(*d,p,&cancelled);});unchanged(*d,before);check(!p.consumed(),"Cancelled apply consumed preview");cancelled=false;const auto out=apply_annotation_plugin(*d,p,&cancelled);check(p.consumed()&&out.objects.size()==3,"Valid retry after cancellation failed");cancelled=true;refuses<WorkflowCancelled>([&]{import_expression_matrix_plugin("org.qupathxx.counts-tsv",(f/"counts-main.tsv").u8string(),(f/"positions.tsv").u8string(),(w/"cancelled.qmx").u8string(),matrix_options(),std::nullopt,&cancelled);});check(!fs::exists(w/"cancelled.qmx"),"Cancelled matrix published output");no_stage(w);}
void position_case(const fs::path& f,const fs::path& w){for(const auto* name:{"positions-missing.tsv","positions-extra.tsv","positions-duplicate.tsv"}){const auto output=w/(std::string(name)+".qmx");refuses<std::invalid_argument>([&]{import_expression_matrix_plugin("org.qupathxx.counts-tsv",(f/"counts-main.tsv").u8string(),(f/name).u8string(),output.u8string(),matrix_options());});check(!fs::exists(output),"Incomplete ID join published QMX");no_stage(w);}}
void limit_case(const fs::path& f,const fs::path& w){AnnotationPluginImportOptions baseline;baseline.plugin_work_bytes=65536;baseline.work_bytes=4u*1024*1024+baseline.plugin_work_bytes;baseline.validate();--baseline.work_bytes;refuses<WorkflowCaptureLimit>([&]{baseline.validate();});auto d=target(f);auto before=snapshot(*d);auto opts=annotation_options();opts.max_points=2;refuses<WorkflowCaptureLimit>([&]{preview_annotation_plugin(*d,"org.qupathxx.point-tsv",(f/"points-main.tsv").u8string(),opts);});unchanged(*d,before);opts.max_points=3;auto p=preview_annotation_plugin(*d,"org.qupathxx.point-tsv",(f/"points-main.tsv").u8string(),opts);check(p.objects().size()==3,"Exact point-count limit refused");unchanged(*d,before);
    for(const std::string field:{"features","observations","entries","source-bytes","store-bytes","work"}){auto m=matrix_options();if(field=="features")m.limits.max_features=2;else if(field=="observations")m.limits.max_observations=2;else if(field=="entries")m.limits.max_entries=8;else if(field=="source-bytes")m.limits.file_bytes=fs::file_size(f/"counts-main.tsv")-1;else if(field=="store-bytes")m.limits.store_bytes=4096;else m.limits.work_bytes=m.plugin_work_bytes+4u*1024*1024;const auto dest=w/(field+".qmx");refuses([&]{import_expression_matrix_plugin("org.qupathxx.counts-tsv",(f/"counts-main.tsv").u8string(),(f/"positions.tsv").u8string(),dest.u8string(),m);});check(!fs::exists(dest),"Quota refusal published QMX");no_stage(w);}
    auto exact=matrix_options();exact.limits.max_features=3;exact.limits.max_observations=3;exact.limits.max_entries=9;exact.limits.file_bytes=fs::file_size(f/"counts-main.tsv");const auto result=import_expression_matrix_plugin("org.qupathxx.counts-tsv",(f/"counts-main.tsv").u8string(),(f/"positions.tsv").u8string(),(w/"exact.qmx").u8string(),exact);check_matrix(*open_expression_matrix(result.path,result.file_sha256));
}
void native_id_limits(const fs::path& f,const fs::path& w){
    for(unsigned n:{256u,257u}){const auto input=f/("counts-native-id"+std::to_string(n)+".tsv"),dest=w/("id"+std::to_string(n)+".qmx");auto call=[&]{return import_expression_matrix_plugin("org.qupathxx.counts-tsv",input.u8string(),(f/"positions-one.tsv").u8string(),dest.u8string(),matrix_options());};if(n==256){const auto r=call();const auto source=open_expression_matrix(r.path,r.file_sha256);check(source->feature_page(0,1).at(0).id==std::string(256,'F'),"Native 256-byte feature ID changed");}else{refuses<std::invalid_argument>(call);check(!fs::exists(dest),"257-byte feature ID published QMX");}no_stage(w);auto d=target(f);const auto before=snapshot(*d);auto capture=[&]{return preview_annotation_plugin(*d,"org.qupathxx.point-tsv",(f/("points-native-id"+std::to_string(n)+".tsv")).u8string());};if(n==256){auto preview=capture();const auto r=apply_annotation_plugin(*d,preview);check(r.objects.size()==1&&r.objects[0].source_id==std::string(256,'P')&&d->annotation(r.objects[0].object_id).name==std::string(256,'P'),"Native 256-byte point source ID changed");}else{refuses<std::invalid_argument>(capture);unchanged(*d,before);}}
}
void legacy_case(const fs::path& w){write(w/"tiny.mtx","%%MatrixMarket matrix coordinate integer general\n% unsorted original entries\n3 4 6\n3 4 7\n1 2 9007199254740993\n2 1 18446744073709551615\n1 1 5\n2 3 0\n3 1 11\n");write(w/"features.tsv","feature_id\nG1\nG2\nG3\n");write(w/"observations.tsv","observation_id\nA\nB\nC\nD\n");ExpressionMatrixImportOptions o;o.work_bytes=8u*1024*1024;auto r=import_expression_matrix((w/"tiny.mtx").u8string(),(w/"features.tsv").u8string(),(w/"observations.tsv").u8string(),(w/"legacy.qmx").u8string(),o);check(r.file_sha256=="174b4ea68d6022fbab3a488b0a070d0a550853e42e410880707e6f5d8b0b6018","Legacy schema1 exact hash changed");check(!external_expression_matrix_receipt(*open_expression_matrix(r.path,r.file_sha256)),"Legacy source acquired invented external provenance");}
void boundary_case(const std::string& mode,const fs::path& f,const fs::path& w){const auto input=w/"boundary.control";write(input,mode+"\n");const bool matrix=mode=="axis-duplicate"||mode=="observation-name"||mode=="mutate-source"||mode=="wait-values";const auto destination=w/"boundary.qmx";auto d=target(f);const auto before=snapshot(*d);std::atomic<bool> cancelled{false},done{false},observed{false};std::future<void> observer;
    if(mode.rfind("wait-",0)==0)observer=std::async(std::launch::async,[&]{const auto until=std::chrono::steady_clock::now()+std::chrono::seconds(8);while(!done.load()){const auto events=fs::u8path(input.u8string()+".events");if(fs::exists(events)&&bytes(events).find("entered\n")!=std::string::npos){observed=true;cancelled=true;return;}if(std::chrono::steady_clock::now()>until)throw std::runtime_error("Controlled native cancellation seam was not reached");std::this_thread::sleep_for(std::chrono::milliseconds(1));}});
    bool refused=false;try{if(matrix){const auto call=[&]{return import_expression_matrix_plugin("org.qupathxx.test.bad-matrix",input.u8string(),(f/"positions.tsv").u8string(),destination.u8string(),matrix_options(),std::nullopt,&cancelled);};if(mode=="wait-values"){refuses<WorkflowCancelled>(call);refused=true;}else if(mode=="mutate-source"){try{const auto r=call();check(bytes(input)==mode+"\n","Changed input incorrectly published QMX");check_matrix(*open_expression_matrix(r.path,r.file_sha256));std::cout<<"OS source guard blocked attempted mutation; stable snapshot published\n";}catch(const WorkflowInputChanged&){refused=true;}}else{refuses<std::invalid_argument>(call);refused=true;}}else{const auto call=[&]{return preview_annotation_plugin(*d,"org.qupathxx.test.bad-annotation",input.u8string(),annotation_options(),std::nullopt,&cancelled);};if(mode=="wait-points")refuses<WorkflowCancelled>(call);else if(mode=="point-huge")refuses<WorkflowCaptureLimit>(call);else refuses<std::invalid_argument>(call);refused=true;}done=true;if(observer.valid())observer.get();}catch(...){done=true;if(observer.valid())observer.wait();throw;}
    if(mode.rfind("wait-",0)==0)check(observed,"Cancellation test missed actual provider call");if(refused)check(!fs::exists(destination),"Boundary refusal left published QMX");unchanged(*d,before);no_stage(w);check(bytes(fs::u8path(input.u8string()+".events"))==(mode.rfind("wait-",0)==0?"open\nentered\ndestroy\n":"open\ndestroy\n"),"Boundary context destruction/events differ");
}
void stale_file_target_case(const fs::path& w) {
    const auto image=w/"image.ppm", replacement=w/"replacement.ppm", original=w/"original.ppm";
    const auto points=w/"points.tsv";
    const std::string header="P6\n2 2\n255\n";
    write(image,header+std::string(12,'a'));
    write(replacement,header+std::string(12,'b'));
    write(points,"id\tx\ty\tc\tz\tt\tclass\ncell\t1\t1\t-1\t0\t0\t\n");
    const auto read_image=[&]{return detail::open_reader(image.u8string(),detail::select_reader(image.u8string(),"ppm"));};
    Document opened(read_image());
    const auto before=snapshot(opened);
    fs::rename(image,original);
    fs::rename(replacement,image);
    refuses<WorkflowInputChanged>([&] {
        detail::FormatImportImageGuard guard(opened.server(),nullptr);
        guard.verify(nullptr);
    });
    refuses<WorkflowInputChanged>([&] {
        preview_annotation_plugin(opened,"org.qupathxx.point-tsv",points.u8string());
    });
    unchanged(opened,before);
    Document current(read_image());
    auto preview=preview_annotation_plugin(current,"org.qupathxx.point-tsv",points.u8string());
    check(apply_annotation_plugin(current,preview).objects.size()==1,
          "A fresh reader must accept the current image");
    check(bytes(original)==header+std::string(12,'a') && bytes(image)==header+std::string(12,'b'),
          "The stale or fresh annotation import changed an input");
}
void stale_compound_target_case(const fs::path& manifest,const fs::path& w) {
    configure_reader_plugins(manifest.u8string());
    const auto image=w/"study.mhd", companion=w/"scan 005 raw.zraw";
    const auto read_image=[&]{
        return detail::open_reader(image.u8string(),detail::select_reader(image.u8string(),"org.qupathxx.metaimage"));
    };
    auto opened=read_image();
    check(opened->reader_source()->companions.size()==2,"Expected two declared MetaImage companions");
    {
        detail::FormatImportImageGuard guard(opened,nullptr);
        guard.verify(nullptr);
    }
    const auto original=w/"original-slice.zraw";
    fs::rename(companion,original);
    fs::copy_file(original,companion);
    refuses<WorkflowInputChanged>([&] {
        detail::FormatImportImageGuard guard(opened,nullptr);
        guard.verify(nullptr);
    });
    detail::FormatImportImageGuard fresh(read_image(),nullptr);
    fresh.verify(nullptr);
    check(bytes(original)==bytes(companion),"Expected a byte-identical replacement companion");
}
void directory_target_case(const fs::path& w) {
    check(has_ome_zarr(), "Directory target regression requires native OME-Zarr");
    const auto image = w/"image.zarr", chunk = image/"0"/"0.0";
    fs::create_directories(image/"0");
    write(image/".zgroup", R"({"zarr_format":2})");
    write(image/".zattrs", R"({"multiscales":[{"version":"0.4","axes":[{"name":"y","type":"space"},{"name":"x","type":"space"}],"datasets":[{"path":"0","coordinateTransformations":[{"type":"scale","scale":[1,1]}]}]}]})");
    write(image/"0"/".zarray", R"({"zarr_format":2,"shape":[2,2],"chunks":[2,2],"dtype":"|u1","compressor":null,"fill_value":0,"order":"C","filters":null})");
    const auto points=w/"points.tsv", counts=w/"counts.tsv", positions=w/"positions.tsv", output=w/"matrix.qmx";
    write(points,"id\tx\ty\tc\tz\tt\tclass\ncell\t1\t1\t-1\t0\t0\t\n");
    write(counts,"observation_id\tgene\ncell\t9007199254740993\n");
    write(positions,"observation_id\tx_px\ty_px\ncell\t1\t1\n");
    Document old(open_ome_zarr(image.u8string()));
    auto preview=preview_annotation_plugin(old,"org.qupathxx.point-tsv",points.u8string());
    const auto before=snapshot(old);
    write(chunk,"ABCD");
    refuses<WorkflowInputChanged>([&]{apply_annotation_plugin(old,preview);});
    unchanged(old,before);

    auto server=open_ome_zarr(image.u8string());
    Document current(server);
    detail::FormatImportImageGuard guard(server,nullptr);
    auto options=matrix_options();
    options.frame=image_coordinate_frame(current,"directory-target",ObjectPlane{});
    unsigned verification=0;
    refuses<WorkflowInputChanged>([&] {
        detail::import_matrix_file_guarded("org.qupathxx.counts-tsv",counts.u8string(),
            positions.u8string(),output.u8string(),options,std::nullopt,[&] {
                if(++verification==3) write(chunk,"ABCDE");
                guard.verify(nullptr);
            },nullptr);
    });
    check(verification==3 && !fs::exists(output), "Changed directory at final publication left a QMX");
    no_stage(w);
    for(const auto& entry:fs::directory_iterator(w))
        check(entry.path().filename().u8string().find(".qpx-tmp-")==std::string::npos,
            "Changed directory left a temporary publication file");
}
int dispatch(int argc,char** argv){try{if(argc!=5)throw std::invalid_argument("Usage: CASE MANIFEST FIXTURES WORK");const std::string mode=argv[1];const auto f=fs::absolute(fs::u8path(argv[3])),w=fs::absolute(fs::u8path(argv[4]));configure_format_plugins(fs::absolute(fs::u8path(argv[2])).u8string());check(format_plugin_infos().size()==2&&format_plugin_diagnostics().empty(),"Admit exact typed provider catalog");if(mode=="stale-file-target")stale_file_target_case(w);else if(mode=="stale-compound-target")stale_compound_target_case(f,w);else if(mode=="directory-target")directory_target_case(w);else if(mode=="matrix")matrix_case(f,w);else if(mode=="annotation")annotation_case(f,w);else if(mode=="annotation-stale")stale_case(f,w);else if(mode=="annotation-cancel")cancel_case(f,w);else if(mode=="positions")position_case(f,w);else if(mode=="limits")limit_case(f,w);else if(mode=="legacy")legacy_case(w);else if(mode=="native-id-limits")native_id_limits(f,w);else boundary_case(mode,f,w);std::cout<<"Native import case "<<mode<<" passed\n";return 0;}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
}
#ifdef _WIN32
int wmain(int argc,wchar_t** argv){std::vector<std::string> owned;std::vector<char*> utf8;for(int i=0;i<argc;++i){const int n=WideCharToMultiByte(CP_UTF8,WC_ERR_INVALID_CHARS,argv[i],-1,nullptr,0,nullptr,nullptr);if(n<=0)return 2;std::string s(static_cast<std::size_t>(n),'\0');if(!WideCharToMultiByte(CP_UTF8,WC_ERR_INVALID_CHARS,argv[i],-1,s.data(),n,nullptr,nullptr))return 2;s.pop_back();owned.push_back(std::move(s));}for(auto& s:owned)utf8.push_back(s.data());return dispatch(argc,utf8.data());}
#else
int main(int argc,char** argv){return dispatch(argc,argv);}
#endif
