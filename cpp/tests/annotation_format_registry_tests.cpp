#include "../annotation_format_registry.hpp"
#include "qupathxx/object_properties.hpp"
#include "export_policy_checks.hpp"
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <limits>
#include <random>
#include <stdexcept>
#ifdef QUPATHXX_HAS_INTERCHANGE
namespace qpx::detail {
InterchangeReport import_geojson_class_context(Document&,const std::string&,const std::map<std::string,ObjectClass>&);
}
#endif
namespace fs=std::filesystem;
namespace af=qpx::detail;
using namespace qpx;
namespace {
void check(bool value,const char* message) {if(!value)throw std::runtime_error(message);}
template<class Error=std::exception,class Function>std::string rejects(Function&& function) {
    try {function();}catch(const Error& error) {return error.what();}
    throw std::runtime_error("Missing annotation format refusal");
}
std::string read(const fs::path& path) {std::ifstream in(path,std::ios::binary);check(bool(in),"Read owned annotation fixture");return {std::istreambuf_iterator<char>(in),{}};}
void write(const fs::path& path,const std::string& text) {std::ofstream out(path,std::ios::binary);out.write(text.data(),std::streamsize(text.size()));out.close();check(bool(out),"Write owned annotation fixture");}
void same_report(const InterchangeReport& a,const InterchangeReport& b) {
    check(a.text==b.text && a.features==b.features && a.imported_ids==b.imported_ids && a.source_ids==b.source_ids && a.issues.size()==b.issues.size(),"Direct/catalog interchange reports differ");
    for(std::size_t i=0;i<a.issues.size();++i)check(a.issues[i].code==b.issues[i].code && a.issues[i].message==b.issues[i].message && a.issues[i].count==b.issues[i].count,"Direct/catalog loss reports differ");
}
Image image() {return Image(16,16,std::vector<std::uint8_t>(16*16*3,80));}
std::shared_ptr<Document> source() {
    auto doc=std::make_shared<Document>(image());doc->set_calibration(.25,.5);
    doc->set_class_definition(ObjectClass::definition("Root",0x80123456));
    doc->set_class_definition(ObjectClass::definition("Root: Leaf",std::nullopt,"Root"));
    const auto parent=doc->add_rectangle({0,0,10,10},"Parent, quoted \"name\"");doc->classify_annotation(parent,"Root");
    const auto child=doc->add_cell(Roi::rectangle({1,1,4,4}),Roi::rectangle({2,2,1,1}),"Cell",parent,"Root: Leaf");
    doc->set_object_properties(parent,std::make_shared<const ObjectProperties>(std::map<std::string,std::string>{{"note","native"}},
        std::map<std::string,double>{{"Area",3.5},{"negative zero",-0.0},{"missing",std::numeric_limits<double>::quiet_NaN()},{"infinity",std::numeric_limits<double>::infinity()}}));
    doc->set_object_edit_state(child,true,0x123456,.875);return doc;
}
std::shared_ptr<Document> target() {auto doc=std::make_shared<Document>(image());doc->set_calibration(.125,.75);doc->add_rectangle({12,12,1,1},"Sentinel");return doc;}
void same_document(const Document& a,const Document& b) {
    check(a.classes()==b.classes() && a.object_count()==b.object_count() && a.revision()==b.revision() && a.pixel_width_um()==b.pixel_width_um() && a.pixel_height_um()==b.pixel_height_um(),"Direct/catalog document state differs");
    for(const auto& [id,x]:a.objects()) {
        const auto& y=b.annotation(id);
        check(x.name==y.name && x.parent_id==y.parent_id && x.kind==y.kind && x.class_name==y.class_name && x.plane==y.plane && x.geometry.equals(y.geometry) && bool(x.nucleus)==bool(y.nucleus) && bool(x.properties)==bool(y.properties),"Direct/catalog object identity or geometry differs");
        if(x.nucleus)check(x.nucleus->equals(*y.nucleus),"Direct/catalog nucleus differs");
        if(x.properties)check(x.properties->equals(*y.properties),"Direct/catalog imported facts or editing state differs");
    }
}
void catalog_and_refusals(const Document& doc,const fs::path& root) {
    for(bool geojson:{false,true}) {
        const auto formats=af::annotation_format_catalog({geojson});check(formats.size()==2,"Annotation provider count changed");
        const auto& geo=formats[0];const auto& csv=formats[1];
        check(geo.provider_id=="geojson" && geo.available==geojson && geo.unavailable_reason.empty()==geojson && geo.import_text && geo.import_file && geo.export_text && geo.export_file && geo.clipboard && geo.class_context,"GeoJSON catalog capability/availability differs");
        check(csv.provider_id=="measurements-csv" && csv.available && csv.unavailable_reason.empty() && !csv.import_text && !csv.import_file && csv.export_text && csv.export_file && !csv.clipboard && !csv.class_context,"CSV must remain available export-only");
    }
    check(af::annotation_format_features().geojson==has_interchange() && af::known_annotation_format("geojson") && af::known_annotation_format("measurements-csv") && !af::known_annotation_format("auto") && !af::known_annotation_format("GeoJSON") && !af::known_annotation_format("clipboard"),"Annotation requests must use explicit provider IDs");
    const auto revision=doc.revision();auto destination=target();const auto before=destination->revision();const std::string invalid="{bad}";
    check(rejects<std::invalid_argument>([&]{af::import_annotations(*destination,af::AnnotationTextImportRequest{"missing",std::cref(invalid),{}});})=="unknown annotation format: missing","Unknown annotation provider did not refuse precisely");
    af::AnnotationFileImportRequest input;input.provider_id="measurements-csv";input.path=(root/"never-read").u8string();
    check(rejects<std::invalid_argument>([&]{af::import_annotations(*destination,input);})=="annotation format does not support import: measurements-csv","Export-only provider reached file input");
    af::AnnotationClipboardImportRequest paste;paste.provider_id="measurements-csv";paste.text=invalid;
    check(rejects<std::invalid_argument>([&]{af::import_annotations(*destination,paste);})=="annotation format does not support clipboard: measurements-csv","CSV was admitted as clipboard format");
    af::AnnotationFileExportRequest output;output.path=(root/"never-created").u8string();output.provider_id="measurements-csv";
    check(rejects<std::invalid_argument>([&]{af::export_annotations(doc,output);})=="annotation export options do not match provider" && !fs::exists(fs::u8path(output.path)),"Mismatched export options reached publication");
    output.provider_id="missing";
    check(rejects<std::invalid_argument>([&]{af::export_annotations(doc,output);})=="unknown annotation format: missing" && !fs::exists(fs::u8path(output.path)),"Unknown export provider reached publication");
    check(doc.revision()==revision && destination->revision()==before && destination->object_count()==1 && destination->classes().empty(),"Routing refusal mutated a document");
}
void csv_routes(const Document& doc) {
    const std::vector<std::uint64_t> ids{2,1,2};af::AnnotationMeasurementsCsvOptions options{std::cref(ids),true};
    const auto direct=measurements_csv(doc,ids,true);const auto report=af::export_annotations(doc,af::AnnotationTextExportRequest{"measurements-csv",options});
    check(report.text==direct && report.features==0 && report.issues.empty() && report.imported_ids.empty() && direct.find("metadata:note")!=std::string::npos && direct.find("NaN")!=std::string::npos && direct.find("Infinity")!=std::string::npos,"Object CSV text or honest report changed");
    export_policy_checks::writer([&](const std::string& path,ExportWritePolicy policy,const std::vector<std::string>& protected_paths) {
        af::AnnotationFileExportRequest request{"measurements-csv",path,options,policy,std::cref(protected_paths)};
        const auto saved=af::export_annotations(doc,request);check(saved.text.empty() && saved.features==0 && saved.issues.empty(),"Void native CSV writer invented report facts");
    },direct,"annotation-catalog-csv");
    const auto missing=std::vector<std::uint64_t>{999};options.ids=std::cref(missing);
    check(rejects<std::out_of_range>([&]{af::export_annotations(doc,af::AnnotationTextExportRequest{"measurements-csv",options});})=="export object does not exist","CSV selection refusal changed");
}
void geojson_routes(const Document& doc,const fs::path& root) {
    const std::vector<std::uint64_t> ids{2,1,2};af::AnnotationGeoJsonExportOptions options{std::cref(ids),false,false};
    for(bool nested:{false,true}) {options.nested_hierarchy=nested;same_report(export_geojson(doc,ids,false,nested),af::export_annotations(doc,af::AnnotationTextExportRequest{"geojson",options}));}
    options.nested_hierarchy=false;const auto exported=export_geojson(doc,ids);auto direct=target(),catalog=target();
    same_report(import_geojson(*direct,exported.text),af::import_annotations(*catalog,af::AnnotationTextImportRequest{"geojson",std::cref(exported.text),{}}));same_document(*direct,*catalog);
    check(catalog->objects().size()==3 && catalog->annotation(3).parent_id==2 && catalog->annotation(3).nucleus->area()==1 && catalog->pixel_width_um()==.125 && catalog->pixel_height_um()==.75,"Native object remapping/calibration oracle changed");
    check(catalog->annotation(2).properties->equals(*doc.annotation(1).properties) && catalog->object_class("Root").color_argb()==0x80123456 && !catalog->object_class("Root: Leaf").color_argb(),"Class ARGB/absence or imported properties changed");
    export_policy_checks::writer([&](const std::string& path,ExportWritePolicy policy,const std::vector<std::string>& protected_paths) {
        const auto report=af::export_annotations(doc,af::AnnotationFileExportRequest{"geojson",path,options,policy,std::cref(protected_paths)});same_report(exported,report);
    },exported.text,"annotation-catalog-geojson");
    const auto file=root/"annotations.geojson";write(file,exported.text);const auto input=read(file);auto file_direct=target(),file_catalog=target();
    same_report(load_geojson(*file_direct,file.u8string()),af::import_annotations(*file_catalog,af::AnnotationFileImportRequest{"geojson",file.u8string(),{}}));same_document(*file_direct,*file_catalog);check(read(file)==input,"File import changed input bytes");fs::remove(file);
    const std::string malformed="{bad}";auto untouched=target();const auto revision=untouched->revision();const auto state=export_geojson(*untouched).text;
    const auto error=rejects<std::invalid_argument>([&]{import_geojson(*untouched,malformed);});
    check(rejects<std::invalid_argument>([&]{af::import_annotations(*untouched,af::AnnotationTextImportRequest{"geojson",std::cref(malformed),{}});})==error && untouched->revision()==revision && export_geojson(*untouched).text==state,"Malformed import changed refusal/state");
    untouched->set_class("Root: Leaf",0x123456);const auto collision_revision=untouched->revision();const auto collision=export_geojson(*untouched).text;
    const auto collision_error=rejects<std::invalid_argument>([&]{import_geojson(*untouched,exported.text);});
    check(rejects<std::invalid_argument>([&]{af::import_annotations(*untouched,af::AnnotationTextImportRequest{"geojson",std::cref(exported.text),{}});})==collision_error && untouched->revision()==collision_revision && export_geojson(*untouched).text==collision,"Class collision changed document or refusal");
}
void clipboard_routes(const Document& doc) {
    const std::vector<std::uint64_t> parent{1};af::AnnotationClipboardExportRequest request{"geojson",std::cref(parent),true,false,16u*1024*1024};
    const auto direct=copy_objects(doc,parent,true);same_report(direct,af::export_annotations(doc,request));check(direct.features==2,"Clipboard descendant closure changed");
    auto a=target(),b=target();same_report(paste_objects(*a,direct.text),af::import_annotations(*b,af::AnnotationClipboardImportRequest{"geojson",direct.text}));same_document(*a,*b);
    check(b->undo_label()=="Paste objects" && b->undo() && b->object_count()==1 && b->redo() && b->object_count()==3,"Clipboard target history changed");
    const auto revision=b->revision();const auto state=export_geojson(*b).text;af::AnnotationClipboardImportRequest paste{"geojson",direct.text,std::nullopt,false,4};
    check(rejects<std::length_error>([&]{af::import_annotations(*b,paste);})==rejects<std::length_error>([&]{paste_objects(*b,direct.text,std::nullopt,false,4);}) && b->revision()==revision && export_geojson(*b).text==state,"Clipboard byte refusal mutated target");
    paste.max_bytes=16u*1024*1024;paste.plane=ObjectPlane{-1,1,0};
    check(rejects<std::invalid_argument>([&]{af::import_annotations(*b,paste);})==rejects<std::invalid_argument>([&]{paste_objects(*b,direct.text,paste.plane);}) && b->revision()==revision && export_geojson(*b).text==state,"Clipboard plane validation was bypassed or mutated target");
    request.max_bytes=4;check(rejects<std::length_error>([&]{af::export_annotations(doc,request);})==rejects<std::length_error>([&]{copy_objects(doc,parent,true,false,4);}),"Clipboard export byte refusal changed");
    request.ids=std::cref(af::annotation_empty_selection());check(rejects<std::invalid_argument>([&]{af::export_annotations(doc,request);})=="select objects before copying","Clipboard empty selection widened to all objects");
}
void policy_routes(const Document& doc) {
    const std::string foreign=R"({"type":"FeatureCollection","foreign":"drop","features":[{"type":"Feature","id":"foreign-1","geometry":{"type":"Point","coordinates":[2,3]},"properties":{"classification":{"names":["Root","Leaf"],"color":[0,0,0]}}}]})";
    af::AnnotationGeoJsonImportOptions options{true,true,false};auto a=target(),b=target();
    const auto direct=import_geojson(*a,foreign,true,true);same_report(direct,af::import_annotations(*b,af::AnnotationTextImportRequest{"geojson",std::cref(foreign),options}));same_document(*a,*b);check(!direct.issues.empty() && direct.source_ids==std::vector<std::string>{"foreign-1"},"Foreign loss/source identity oracle changed");
#ifdef QUPATHXX_HAS_INTERCHANGE
    auto context_direct=target(),context_catalog=target();af::AnnotationQuPathClassContext context{std::cref(doc.classes())};
    same_report(af::import_geojson_class_context(*context_direct,foreign,doc.classes()),af::import_annotations(*context_catalog,af::AnnotationTextImportRequest{"geojson",std::cref(foreign),context}));same_document(*context_direct,*context_catalog);
    check(context_catalog->object_class("Root")==doc.object_class("Root") && context_catalog->object_class("Root: Leaf")==doc.object_class("Root: Leaf"),"Project class-context identity/color precedence changed");
#endif
    const std::string ellipse=R"({"type":"Feature","geometry":{"type":"Polygon","isEllipse":true,"coordinates":[[[1.123,2.234],[3.987,2.234],[3.987,5.876],[1.123,5.876],[1.123,2.234]]]},"properties":{}})";
    auto precision_direct=target(),precision_catalog=target();options.qupath_ellipse_precision=true;
    if(has_geos()) {same_report(import_geojson(*precision_direct,ellipse,true,true,true),af::import_annotations(*precision_catalog,af::AnnotationTextImportRequest{"geojson",std::cref(ellipse),options}));same_document(*precision_direct,*precision_catalog);}
    else check(rejects<std::runtime_error>([&]{import_geojson(*precision_direct,ellipse,true,true,true);})==rejects<std::runtime_error>([&]{af::import_annotations(*precision_catalog,af::AnnotationTextImportRequest{"geojson",std::cref(ellipse),options});}) && precision_catalog->object_count()==1,"QuPath precision availability/error behavior changed");
}
void unavailable_routes(const Document& doc,const fs::path& root) {
    const auto revision=doc.revision();auto destination=target();const auto before=destination->revision();const std::string text="{}";
    const auto error=rejects<std::runtime_error>([&]{export_geojson(doc);});
    check(error=="GeoJSON requires QUPATHXX_BUILD_INTERCHANGE=ON" && rejects<std::runtime_error>([&]{af::export_annotations(doc,af::AnnotationTextExportRequest{});})==error,"Known unavailable export lost native error");
    check(rejects<std::runtime_error>([&]{af::import_annotations(*destination,af::AnnotationTextImportRequest{"geojson",std::cref(text),{}});})==error,"Known unavailable text import lost native error");
    af::AnnotationQuPathClassContext context{std::cref(doc.classes())};check(rejects<std::runtime_error>([&]{af::import_annotations(*destination,af::AnnotationTextImportRequest{"geojson",std::cref(text),context});})==error,"Disabled class-context must use native public stub");
    const auto file=root/"disabled.geojson";write(file,text);
    check(rejects<std::runtime_error>([&]{af::import_annotations(*destination,af::AnnotationFileImportRequest{"geojson",file.u8string(),{}});})==rejects<std::runtime_error>([&]{load_geojson(*destination,file.u8string());}) && read(file)==text,"Disabled file import changed native order/input");fs::remove(file);
    const auto output=root/"disabled-output.geojson";af::AnnotationFileExportRequest save;save.path=output.u8string();check(rejects<std::runtime_error>([&]{af::export_annotations(doc,save);})==error && !fs::exists(output),"Disabled export published output");
    check(doc.revision()==revision && destination->revision()==before && destination->object_count()==1,"Disabled native route mutated document");
}
}
int main() {
    try {
        const auto root=fs::temp_directory_path()/("qpx-annotation-catalog-"+std::to_string(std::random_device{}()));check(fs::create_directory(root),"Create unique annotation fixture directory");
        const auto doc=source();catalog_and_refusals(*doc,root);csv_routes(*doc);
        if(has_interchange()) {geojson_routes(*doc,root);clipboard_routes(*doc);policy_routes(*doc);}else unavailable_routes(*doc,root);
        check(fs::remove(root),"Owned annotation fixture files remain");
        std::cout<<"Annotation format catalog: exact native geometry/IDs/classes/reports/CSV, transport policy and atomic refusals passed\n";
    }catch(const std::exception& error) {std::cerr<<error.what()<<'\n';return 1;}
}
