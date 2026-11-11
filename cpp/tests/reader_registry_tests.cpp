#include "../reader_registry.hpp"
#include "../reader_file_guard.hpp"
#include "../reader_provenance.hpp"
#include "qupathxx/channel_source.hpp"
#include "qupathxx/core.hpp"
#ifdef QUPATHXX_HAS_PROJECT
#include "../project_source_internal.hpp"
#include "../project_zarr.hpp"
#include "qupathxx/project.hpp"
#include "qupathxx/project_source.hpp"
#include <sqlite3.h>
#endif
#include "qupathxx/rgb_preview.hpp"
#include "quantitative_tiff_fixture.hpp"
#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>

using namespace qpx;
namespace fs=std::filesystem;
namespace rd=qpx::detail;
namespace {
void check(bool condition,const std::string& message) {if(!condition)throw std::runtime_error(message);}
template<class F> void rejects_invalid(F action,const std::string& message) {
    try {action();}catch(const std::invalid_argument&){return;}throw std::runtime_error(message);
}
struct Failure {std::string type,message;};
template<class F> Failure failure(F action) {
    try {action();}
    catch(const std::invalid_argument& e){return {"invalid_argument",e.what()};}
    catch(const std::out_of_range& e){return {"out_of_range",e.what()};}
    catch(const std::runtime_error& e){return {"runtime_error",e.what()};}
    throw std::runtime_error("Expected the original factory to refuse the source");
}

// Frozen from the pre-registry callers, including their intentional differences.
// Paths need not exist: resolving a saved missing source must not probe the disk.
void selection_baseline() {
    struct Case {const char* path;const char* project_tiff;const char* project_no_tiff;
        const char* region_tiff;const char* region_no_tiff;const char* rgb;const char* spatial;const char* qupath;};
    const Case cases[]={
        {"a.ppm","ppm","ppm","ppm","ppm","ppm","ppm","ppm"},
        {"A.PPM","ppm","ppm","ppm","ppm","ppm","ppm","ppm"},
        {"s.zarr","ome-zarr","ome-zarr","openslide","openslide","ome-zarr",nullptr,"openslide"},
        {"S.ZARR","ome-zarr","ome-zarr","openslide","openslide","ome-zarr",nullptr,"openslide"},
        {"x.ome.tif","tiff","openslide","tiff","openslide","tiff","tiff","tiff"},
        {"x.OME.TIFF","tiff","openslide","tiff","openslide","tiff","tiff","tiff"},
        {"x.ome.btf","tiff","openslide","tiff","openslide","openslide",nullptr,"tiff"},
        {"x.ome.tf2","tiff","openslide","tiff","openslide","openslide",nullptr,"tiff"},
        {"x.ome.tf8","tiff","openslide","tiff","openslide","openslide",nullptr,"tiff"},
        {"x.tif","openslide","openslide","openslide","openslide","tiff","tiff","tiff"},
        {"x.tiff","openslide","openslide","openslide","openslide","tiff","tiff","tiff"},
        {"x.svs","openslide","openslide","openslide","openslide","openslide",nullptr,"openslide"},
        {"noext","openslide","openslide","openslide","openslide","openslide",nullptr,"openslide"},
        {"dir.ome/x.tif","openslide","openslide","openslide","openslide","tiff","tiff","tiff"},
        {"x.ome.tif.bak","openslide","openslide","openslide","openslide","openslide",nullptr,"openslide"},
        {u8"missing-\u539f\u59cb/IMAGE.OME.TIF","tiff","openslide","tiff","openslide","tiff","tiff","tiff"},
    };
    const std::vector<std::string> ids={"ppm","openslide","tiff","ome-zarr"};
    for(unsigned mask=0;mask<8;++mask) {
        const rd::ReaderFeatures features{bool(mask&1),bool(mask&2),bool(mask&4)};
        for(const auto& c:cases) {
            const std::pair<rd::ReaderPolicy,const char*> expectations[]={
                {rd::ReaderPolicy::Project,features.tiff?c.project_tiff:c.project_no_tiff},
                {rd::ReaderPolicy::RegionCli,features.tiff?c.region_tiff:c.region_no_tiff},
                {rd::ReaderPolicy::RgbNominal,c.rgb},{rd::ReaderPolicy::SpatialImport,c.spatial},
                {rd::ReaderPolicy::QuPathImport,c.qupath}};
            for(const auto& [policy,expected]:expectations) {
                if(!expected) {rejects_invalid([&]{rd::select_reader(c.path,"auto",policy,features);},"Spatial profile admitted an unsupported suffix");continue;}
                const auto selected=rd::select_reader(c.path,"auto",policy,features);
                check(selected.provider_id==expected && selected.policy==policy,"Pre-registry path policy changed for "+std::string(c.path));
                const auto memory=policy==rd::ReaderPolicy::RgbNominal && std::string(expected)=="ppm";
                check(selected.adapter==(memory?rd::ReaderAdapter::MemoryRgb:rd::ReaderAdapter::FileObserved),"PPM source identity route changed");
            }
        }
        const auto catalog=rd::reader_catalog(features);check(catalog.size()==4,"Catalog must retain all four built-ins");
        std::vector<std::string> listed;
        for(const auto& descriptor:catalog) {
            listed.push_back(descriptor.provider_id);
            const auto enabled=descriptor.provider_id=="ppm" || (descriptor.provider_id=="tiff" && features.tiff) ||
                (descriptor.provider_id=="openslide" && features.openslide) || (descriptor.provider_id=="ome-zarr" && features.ome_zarr);
            check(descriptor.available==enabled,"Known and available were conflated");
            check(!descriptor.display_name.empty() && descriptor.display_rgba,"Missing built-in catalog description");
            check(descriptor.unavailable_reason.empty()==enabled,"Availability reason contradicts the feature set");
            check(descriptor.raw_samples==(descriptor.provider_id=="tiff" || descriptor.provider_id=="ome-zarr"),"Catalog changed quantitative upper bounds");
        }
        check(listed==ids,"Built-in provider IDs/order changed");
        for(const auto& id:ids) {
            check(rd::known_reader_id(id) && rd::valid_reader_request(id),"Disabled built-in became unknown");
            for(const auto policy:{rd::ReaderPolicy::Project,rd::ReaderPolicy::RegionCli,rd::ReaderPolicy::RgbNominal}) {
                const auto selected=rd::select_reader("missing/contrary.svs",id,policy,features);
                check(selected.provider_id==id,"Explicit reader choice changed with availability/path");
            }
        }
    }
    check(rd::valid_reader_request("auto") && !rd::known_reader_id("auto"),"auto is a request, not a provider");
    for(const auto* id:{"embedded","external","qpx.dense-rgb/1","unknown","TIFF","","auto "}) {
        check(!rd::known_reader_id(id) && !rd::valid_reader_request(id),"Reserved/unknown ID admitted as a provider");
        rejects_invalid([&]{rd::select_reader("missing.ppm",id);},"Unknown explicit reader did not refuse");
    }
    const auto actual=rd::reader_features();
    check(actual.openslide==has_openslide() && actual.tiff==has_tiff() && actual.ome_zarr==has_ome_zarr(),"Feature snapshot differs from existing factories");
    check(rd::reader_catalog_json().find("ppm")!=std::string::npos,"Owned reader listing is empty");
}

void desktop_route_baseline() {
    struct Case {const char* path;const char* request;const char* desktop;bool desktop_raw;const char* startup;bool startup_raw;};
    const Case fixed[]={
        {"missing.zarr","tiff","ome-zarr",true,"tiff",true},
        {"missing.zarr","ome-zarr","ome-zarr",true,"ome-zarr",false},
        {"missing.zarr","ppm","ppm",false,"ppm",false},
        {"missing.zarr","openslide","openslide",false,"openslide",false},
        {"missing.zarr","auto","ome-zarr",true,"ome-zarr",false},
        {"missing.tif","auto","openslide",false,"openslide",false},
        {"missing.ppm","tiff","tiff",true,"tiff",true},
        {"missing.ppm","ome-zarr","ome-zarr",true,"ome-zarr",false}};
    for(unsigned mask=0;mask<8;++mask) {
        const rd::ReaderFeatures features{bool(mask&1),bool(mask&2),bool(mask&4)};
        for(const auto& c:fixed) {
            const auto desktop=rd::desktop_add_selection(c.path,c.request,features),startup=rd::desktop_startup_selection(c.path,c.request,features);
            check(desktop.selection.provider_id==c.desktop && desktop.raw_route==c.desktop_raw,"Desktop attachment exception changed");
            check(startup.selection.provider_id==c.startup && startup.raw_route==c.startup_raw,"Startup attachment exception changed");
        }
        for(const auto* path:{"missing.ome.tif","missing.OME.TF8"}) {
            const auto desktop=rd::desktop_add_selection(path,"auto",features),startup=rd::desktop_startup_selection(path,"auto",features);
            const auto expected=features.tiff?"tiff":"openslide";
            check(desktop.selection.provider_id==expected && desktop.raw_route==features.tiff,"Desktop OME attachment changed");
            check(startup.selection.provider_id==expected && startup.raw_route==features.tiff,"Startup OME attachment changed");
        }
    }
}

void same_metadata(const ImageMetadata& a,const ImageMetadata& b) {
    check(a.width==b.width && a.height==b.height && a.vendor==b.vendor && a.pixel_type==b.pixel_type && a.layout==b.layout &&
        a.pixel_width_um==b.pixel_width_um && a.pixel_height_um==b.pixel_height_um && a.levels.size()==b.levels.size(),"Registry changed image metadata");
    for(std::size_t i=0;i<a.levels.size();++i)check(a.levels[i].width==b.levels[i].width && a.levels[i].height==b.levels[i].height && a.levels[i].downsample==b.levels[i].downsample,"Registry changed pyramid metadata");
}
void same_source(const ImageServer& a,const ImageServer& b) {
    const auto x=a.reader_source(),y=b.reader_source();check(x && y && x->reader==y->reader && a.reader_id()==b.reader_id() && a.source_locator()==b.source_locator(),"Registry changed reader identity");
    check(x->generation!=y->generation,"Independent opens must retain independent generations");
    if(x->scope=="memory")check(y->scope=="memory" && !x->primary && !y->primary && x->locator.empty() && y->locator.empty() && x->companions.empty() && y->companions.empty(),"RGB nominal PPM became file-observed");
    else check(rd::same_reader_files(*x,*y) && x->primary->path==y->primary->path,"Registry changed stable observed source facts");
}
void same_raw(const std::shared_ptr<ImageServer>& a,const std::shared_ptr<ImageServer>& b,const RawRegionRequest& request) {
    const auto x=a->raw_metadata(),y=b->raw_metadata();check(x && y,"Raw samples disappeared");
    check(x->width==y->width && x->height==y->height && x->channels==y->channels && x->sample_type==y->sample_type && x->z_planes==y->z_planes && x->timepoints==y->timepoints && x->series==y->series && x->series_count==y->series_count && x->channel_names==y->channel_names && x->dimension_order==y->dimension_order && x->pixel_width_um==y->pixel_width_um && x->pixel_height_um==y->pixel_height_um && x->z_spacing_um==y->z_spacing_um && x->time_interval_seconds==y->time_interval_seconds,"Registry changed raw axes/calibration");
    const auto left=a->read_raw(request),right=b->read_raw(request);
    check(left.width()==right.width() && left.height()==right.height() && left.channels()==right.channels() && left.sample_type()==right.sample_type() && left.valid()==right.valid() && left.storage()==right.storage(),"Registry changed original raw bytes/validity");
    const auto source_a=original_channel_source(a),source_b=original_channel_source(b);
    check(source_a->domain()==source_b->domain() && source_a->sha256()==source_b->sha256(),"Registry changed the original scientific source digest");
}
fs::path ppm_fixture(const fs::path& root) {
    const auto path=root/"source.ppm";std::ofstream out(path,std::ios::binary);out<<"P6\n2 2\n255\n";
    const unsigned char rgb[]={255,0,0,0,255,0,0,0,255,7,11,13};out.write(reinterpret_cast<const char*>(rgb),sizeof(rgb));out.close();check(bool(out),"Write independent PPM fixture");return path;
}
std::shared_ptr<ImageServer> registry_open(const fs::path& path,const std::string& id,rd::ReaderPolicy policy=rd::ReaderPolicy::Project,const rd::ReaderOpenOptions& options={}) {
    return rd::open_reader(path.u8string(),rd::select_reader(path.u8string(),id,policy),options);
}
void ppm_reads(const fs::path& path) {
    const auto direct=ppm_image_server(path.u8string()),selected=registry_open(path,"ppm");same_metadata(direct->metadata(),selected->metadata());same_source(*direct,*selected);
    const std::vector<std::uint8_t> expected={255,0,0,255,0,255,0,255,0,0,255,255,7,11,13,255};
    check(direct->read_region({0,0,0,2,2}).bytes()==expected && selected->read_region({0,0,0,2,2}).bytes()==expected,"PPM fixture bytes changed");
    check(direct->read_region({-1,-1,0,4,4}).bytes()==selected->read_region({-1,-1,0,4,4}).bytes(),"PPM registry changed transparent padding");
    const auto memory=dense_image_server(Image::read_ppm(path.u8string())),nominal=registry_open(path,"auto",rd::ReaderPolicy::RgbNominal),consumer=open_rgb_nominal_source(path.u8string());
    same_metadata(memory->metadata(),nominal->metadata());same_source(*memory,*nominal);same_source(*memory,*consumer);
    check(nominal->reader_id()=="qpx.dense-rgb/1" && nominal->read_region({0,0,0,2,2}).bytes()==expected && consumer->read_region({0,0,0,2,2}).bytes()==expected,"Nominal PPM adapter/bytes changed");
}

#ifdef QUPATHXX_HAS_PROJECT
struct Database {sqlite3* handle=nullptr;explicit Database(const fs::path& path,int flags){const auto rc=sqlite3_open_v2(path.u8string().c_str(),&handle,flags,nullptr);if(rc!=SQLITE_OK){const auto message=handle?sqlite3_errmsg(handle):"allocation";const std::string error=message;if(handle)sqlite3_close(handle);handle=nullptr;throw std::runtime_error("Test SQLite open: "+error);}}~Database(){sqlite3_close(handle);}};
using Rows=std::vector<std::vector<std::string>>;
Rows rows(const fs::path& path,const char* query) {
    Database db(path,SQLITE_OPEN_READONLY);sqlite3_stmt* raw=nullptr;check(sqlite3_prepare_v2(db.handle,query,-1,&raw,nullptr)==SQLITE_OK,"Prepare project payload query");
    const auto stmt=std::unique_ptr<sqlite3_stmt,decltype(&sqlite3_finalize)>(raw,sqlite3_finalize);Rows result;int status;
    while((status=sqlite3_step(raw))==SQLITE_ROW){std::vector<std::string> row;for(int i=0;i<sqlite3_column_count(raw);++i){const auto* value=sqlite3_column_text(raw,i);check(value,"Unexpected null fixture field");row.emplace_back(reinterpret_cast<const char*>(value),std::size_t(sqlite3_column_bytes(raw,i)));}result.push_back(std::move(row));}
    check(status==SQLITE_DONE,"Read project payload query");return result;
}
void sql(const fs::path& path,const std::string& command) {Database db(path,SQLITE_OPEN_READWRITE);check(sqlite3_exec(db.handle,command.c_str(),nullptr,nullptr,nullptr)==SQLITE_OK,"Mutate own saved-reader fixture");}
std::string hex(const std::vector<std::uint8_t>& bytes) {constexpr char digits[]="0123456789ABCDEF";std::string value;for(auto byte:bytes){value+=digits[byte>>4];value+=digits[byte&15];}return value;}
void persistence_roundtrip(const fs::path& root,const std::string& name,const std::shared_ptr<ImageServer>& source,const fs::path& fingerprint_path) {
    if(!has_project())return;
    Project project;const auto id=project.add_document(std::make_shared<Document>(source),name,source->source_locator(),source->reader_id());
    project.apply_source_review(project.review_source_verification(id,*fingerprint_file(fingerprint_path.u8string())));
    const auto association=project.source_verification(id);check(bool(association),"Source verification did not attach");
    const auto expected=hex(rd::encode_source_verification(*association));
    const auto first=root/(name+"-first.qpx"),second=root/(name+"-second.qpx");project.save_new(first.u8string());
    const auto descriptor=rows(first,"SELECT id,name,reader,source,embedded FROM images ORDER BY id");
    check(descriptor.size()==1 && descriptor[0][2]==source->reader_id() && descriptor[0][4]=="0","Saved linked reader ID changed");
    check(rows(first,"SELECT hex(payload) FROM source_verifications")==Rows{{expected}},"Saved source verification payload changed");
    const auto views=rows(first,"SELECT image_id,hex(payload) FROM tiff_views ORDER BY image_id");
    if(source->reader_id()=="tiff") {const auto encoded=tiff_view_encode(*source->tiff_view());check(views==Rows{{std::to_string(id),hex(std::vector<std::uint8_t>(encoded.begin(),encoded.end()))}},"Legacy TIFF view payload changed");}
    const auto reopened=Project::open(first.u8string());check(reopened->document(id)->server()->reader_source()->scope=="unopened","Saved project ceased to open lazily");
    reopened->save_new(second.u8string());
    check(rows(second,"SELECT id,name,reader,source,embedded FROM images ORDER BY id")==descriptor && rows(second,"SELECT image_id,hex(payload) FROM tiff_views ORDER BY image_id")==views && rows(second,"SELECT hex(payload) FROM source_verifications")==Rows{{expected}},"Reopen/resave changed exact saved reader/view/verification bytes");
    check(reopened->document(id)->server()->read_region({0,0,0,2,2}).bytes()==source->read_region({0,0,0,2,2}).bytes(),"Saved source projection changed");
}
void offline_reader_validation(const fs::path& root,const fs::path& ppm) {
    if(!has_project())return;
    Project baseline;const auto id=baseline.add_image(ppm.u8string(),"ppm");const auto original=root/"validation.qpx";baseline.save_new(original.u8string());
    for(const auto* reader:{"ppm","openslide","tiff","ome-zarr"}) {
        const auto path=root/(std::string("known-")+reader+".qpx");fs::copy_file(original,path);
        // A known raw reader still needs its format-specific saved link. Author
        // valid bounded payloads without asking an unavailable decoder to open.
        if(std::string(reader)=="tiff" || std::string(reader)=="ome-zarr") {
            TiffView view;view.channels={0,1,2};view.lower={0,0,0};view.upper={255,255,255};
            std::string payload;
            if(std::string(reader)=="tiff")payload=tiff_view_encode(view);
            else {rd::ZarrLink link;link.options.view=view;link.expected=std::string(64,'0');payload=rd::encode_zarr_link(link);}
            sql(path,"INSERT INTO tiff_views(image_id,payload) VALUES('"+std::to_string(id)+"',X'"+hex(std::vector<std::uint8_t>(payload.begin(),payload.end()))+"')");
        }
        sql(path,"UPDATE images SET reader='"+std::string(reader)+"',source='definitely-missing-source'");
        const auto project=Project::open(path.u8string());const auto server=project->document(id)->server();
        check(server->reader_id()==reader && server->reader_source()->scope=="unopened","Known disabled/missing reader failed saved-row validation");
        failure([&]{server->read_region({0,0,0,1,1});});
    }
    for(const auto* reader:{"tiff","ome-zarr"}) {
        const auto malformed=root/(std::string("missing-view-")+reader+".qpx");fs::copy_file(original,malformed);
        sql(malformed,"UPDATE images SET reader='"+std::string(reader)+"',source='definitely-missing-source'");
        const auto rejected=failure([&]{Project::open(malformed.u8string());});
        check(rejected.type=="invalid_argument" && rejected.message==(std::string(reader)=="tiff"?"TIFF project link requires its captured view":"Zarr link requires saved view/directory identity"),"Missing source allowed a malformed raw-reader link");
    }
    for(const auto* reader:{"auto","embedded","external","qpx.dense-rgb/1","unknown"}) {
        const auto invalid=root/"invalid.qpx";if(fs::exists(invalid))fs::remove(invalid);fs::copy_file(original,invalid);
        sql(invalid,"UPDATE images SET reader='"+std::string(reader)+"'");
        rejects_invalid([&]{Project::open(invalid.u8string());},"Unknown/reserved saved provider passed validation");
    }
}
#else
void persistence_roundtrip(const fs::path&,const std::string&,const std::shared_ptr<ImageServer>&,const fs::path&) {}
void offline_reader_validation(const fs::path&,const fs::path&) {}
#endif
void tiff_reads(const fs::path& root) {
    if(!has_tiff())return;
    const auto path=root/"source.ome.tif";qpx_test::raw_pixel_tiff_fixture(path,2,2);
    rd::ReaderOpenOptions options;options.tiff.view.channels={1,0,-1};options.tiff.view.lower={100,0,0};options.tiff.view.upper={103,3,1};options.tiff.decode_bytes=65536;options.tiff.cache_bytes=1024;
    const auto direct=open_tiff(path.u8string(),options.tiff),selected=registry_open(path,"auto",rd::ReaderPolicy::Project,options);
    same_metadata(direct->metadata(),selected->metadata());same_source(*direct,*selected);
    RawRegionRequest request;request.region={0,0,0,2,2};request.channels=2;same_raw(direct,selected,request);
    check(std::get<std::vector<std::uint16_t>>(selected->read_raw(request).storage())==std::vector<std::uint16_t>{0,100,1,101,2,102,3,103},"Independent TIFF raw sample oracle changed");
    request.region={-1,-1,0,4,4};same_raw(direct,selected,request);
    const std::vector<std::uint8_t> expected={0,0,0,255,85,85,0,255,170,170,0,255,255,255,0,255};
    check(direct->read_region({0,0,0,2,2}).bytes()==expected && selected->read_region({0,0,0,2,2}).bytes()==expected,"TIFF channel projection changed");
    check(tiff_view_encode(*direct->tiff_view())==tiff_view_encode(*selected->tiff_view()) && selected->tiff_options()->decode_bytes==65536 && selected->tiff_options()->cache_bytes==1024,"Registry lost TIFF view/budgets");
    persistence_roundtrip(root,"tiff",selected,path);
}
void zarr_reads(const fs::path& root) {
#ifdef QPX_ZARR_FIXTURES
    if(!has_ome_zarr())return;
    const auto path=root/"source.zarr";fs::copy(fs::path(QPX_ZARR_FIXTURES)/"plain",path,fs::copy_options::recursive);
    rd::ReaderOpenOptions options;options.zarr.view.z=1;options.zarr.view.t=1;options.zarr.view.channels={0,1,2};options.zarr.view.lower={10100,11100,12100};options.zarr.view.upper={10146,11146,12146};options.zarr.source.chunk_bytes=1024*1024;options.zarr.source.encoded_bytes=2u*1024*1024;options.zarr.work_bytes=128u*1024*1024;
    const auto direct=open_ome_zarr(path.u8string(),options.zarr),selected=registry_open(path,"auto",rd::ReaderPolicy::Project,options);
    same_metadata(direct->metadata(),selected->metadata());same_source(*direct,*selected);
    RawRegionRequest request;request.region={3,2,0,1,1};request.channels=3;request.z=1;request.t=1;same_raw(direct,selected,request);
    check(std::get<std::vector<std::uint16_t>>(selected->read_raw(request).storage())==std::vector<std::uint16_t>{10123,11123,12123},"Independent Zarr sample/plane oracle changed");
    const auto metadata=selected->raw_metadata();check(metadata->width==7 && metadata->height==5 && metadata->channels==3 && metadata->z_planes==2 && metadata->timepoints==2 && metadata->pixel_width_um==1.5 && metadata->pixel_height_um==1 && metadata->z_spacing_um==4 && metadata->time_interval_seconds==.5,"Zarr axes/calibration changed");
    check(direct->read_region({-1,-1,0,9,7}).bytes()==selected->read_region({-1,-1,0,9,7}).bytes(),"Zarr projection/transparent padding changed");
    const auto actual=selected->ome_zarr_options();check(actual && actual->source.chunk_bytes==options.zarr.source.chunk_bytes && actual->source.encoded_bytes==options.zarr.source.encoded_bytes && actual->work_bytes==options.zarr.work_bytes && ome_zarr_view_encode(actual->view)==ome_zarr_view_encode(direct->ome_zarr_options()->view),"Registry lost Zarr view/budgets");
    persistence_roundtrip(root,"zarr",selected,path/".zattrs");
#else
    (void)root;
    check(!has_ome_zarr(),"Enabled Zarr registry test requires the admitted fixture path");
#endif
}
void original_factory_errors(const fs::path& root) {
    const auto missing=(root/"missing.ome.tif").u8string();
    for(const auto* id:{"ppm","openslide","tiff","ome-zarr"}) {
        const auto direct=failure([&]{if(std::string(id)=="ppm")ppm_image_server(missing);else if(std::string(id)=="openslide")open_slide(missing);else if(std::string(id)=="tiff")open_tiff(missing);else open_ome_zarr(missing);});
        const auto selected=failure([&]{registry_open(fs::u8path(missing),id);});
        check(direct.type==selected.type && direct.message==selected.message,"Registry replaced a factory error or fell back after failure");
    }
    std::atomic<bool> cancelled{true};
    for(const auto* id:{"tiff","ome-zarr"}) {
        const auto direct=failure([&]{if(std::string(id)=="tiff")open_tiff(missing,{},&cancelled);else open_ome_zarr(missing,{},&cancelled);});
        const auto selected=failure([&]{rd::open_reader(missing,rd::select_reader(missing,id),{},&cancelled);});
        check(direct.type==selected.type && direct.message==selected.message,"Registry lost native open cancellation");
    }
    if(const auto fixture=std::getenv("QUPATHXX_TEST_SLIDE");fixture && has_openslide()) {
        const auto direct=open_slide(fixture),selected=registry_open(fs::u8path(fixture),"openslide");same_metadata(direct->metadata(),selected->metadata());same_source(*direct,*selected);
        check(direct->read_region({0,0,0,8,8}).bytes()==selected->read_region({0,0,0,8,8}).bytes(),"OpenSlide display bytes changed");
    }
}
}
int main() {try {
    selection_baseline();desktop_route_baseline();
    const auto root=fs::current_path()/("reader-registry-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));check(fs::create_directory(root),"Create unique registry fixture directory");
    const auto ppm=ppm_fixture(root);ppm_reads(ppm);persistence_roundtrip(root,"ppm",registry_open(ppm,"ppm"),ppm);offline_reader_validation(root,ppm);tiff_reads(root);zarr_reads(root);original_factory_errors(root);
    // Keep failed fixture directories for diagnosis. Only our verified unique
    // child of the current test directory is retired after all owned readers die.
    check(root.parent_path()==fs::current_path() && root.filename().u8string().find("reader-registry-")==0,"Unsafe fixture cleanup target");fs::remove_all(root);
    std::cout<<"Native reader catalog, frozen path/attachment policies, direct factory bytes/identity and exact saved payload checks passed\n";
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
