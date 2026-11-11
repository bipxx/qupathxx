#include "../reader_file_guard.hpp"
#include "../reader_plugin_link.hpp"
#include "qupathxx/analysis.hpp"
#include "qupathxx/project.hpp"
#include "qupathxx/reader_plugin.h"
#include "qupathxx/reader_plugins.hpp"
#include <sqlite3.h>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <stdexcept>

namespace fs=std::filesystem;
namespace rd=qpx::detail;
using namespace qpx;
namespace {
void check(bool value,const char* message) {if(!value)throw std::runtime_error(message);}
std::string bytes(const fs::path& path) {
    std::ifstream in(path,std::ios::binary);check(bool(in),"Open owned offline fixture");
    return {std::istreambuf_iterator<char>(in),std::istreambuf_iterator<char>()};
}
struct Database {
    sqlite3* value=nullptr;
    explicit Database(const fs::path& path,int flags) {
        const auto code=sqlite3_open_v2(path.u8string().c_str(),&value,flags,nullptr);
        if(code!=SQLITE_OK) {if(value)sqlite3_close(value);value=nullptr;throw std::runtime_error("Open owned offline project database");}
    }
    ~Database() {if(value)sqlite3_close(value);}
    void exec(const char* sql) {check(sqlite3_exec(value,sql,nullptr,nullptr,nullptr)==SQLITE_OK,"Execute owned offline fixture SQL");}
};
struct Statement {
    sqlite3_stmt* value=nullptr;
    Statement(Database& db,const char* sql) {check(sqlite3_prepare_v2(db.value,sql,-1,&value,nullptr)==SQLITE_OK,"Prepare owned offline fixture SQL");}
    ~Statement() {if(value)sqlite3_finalize(value);}
};
std::string query(const fs::path& path,const char* sql) {
    Database db(path,SQLITE_OPEN_READONLY);Statement row(db,sql);std::string result;int status;
    while((status=sqlite3_step(row.value))==SQLITE_ROW) {
        for(int c=0;c<sqlite3_column_count(row.value);++c) {
            const auto type=sqlite3_column_type(row.value,c);
            const auto* data=sqlite3_column_blob(row.value,c);const auto count=sqlite3_column_bytes(row.value,c);
            result+=std::to_string(type)+":"+std::to_string(count)+":";
            if(count) {check(data!=nullptr,"Read owned offline fixture value");result.append(static_cast<const char*>(data),std::size_t(count));}
            result+='\0';
        }
        result+='\n';
    }
    check(status==SQLITE_DONE,"Read owned offline fixture rows");return result;
}
std::string history(const fs::path& path) {
    return query(path,"SELECT * FROM analysis_records ORDER BY image_id,id")+
        query(path,"SELECT image_id,record_id,payload FROM analysis_provenance ORDER BY image_id,record_id");
}
void attach_saved_link(const fs::path& path,const std::string& encoded) {
    Database db(path,SQLITE_OPEN_READWRITE);db.exec("BEGIN IMMEDIATE");
    db.exec("UPDATE images SET embedded=0,rgb=NULL,reader='org.qupathxx.offline',source='offline-input.control' WHERE id='1'");
    check(sqlite3_changes(db.value)==1,"Mutate exactly one owned embedded fixture row");
    Statement row(db,"INSERT INTO plugin_links(image_id,payload) VALUES('1',?)");
    check(sqlite3_bind_blob64(row.value,1,encoded.data(),encoded.size(),SQLITE_STATIC)==SQLITE_OK &&
        sqlite3_step(row.value)==SQLITE_DONE,"Attach well-formed offline plugin link");
    db.exec("COMMIT");
}
template<class Function>void provider_refuses(Function&& function) {
    try {function();}catch(const std::runtime_error& error) {
        const std::string message=error.what();
        check(message.find("Reader plugin provider unavailable: org.qupathxx.offline")!=std::string::npos,
            "Offline pixel read must refuse the provider without decoder fallback");return;
    }
    throw std::runtime_error("Offline plugin unexpectedly supplied source pixels");
}
}
int main() {
    try {
        Project seed;const auto root=fs::temp_directory_path()/("qpx-reader-offline-"+seed.identity());
        check(fs::create_directory(root),"Create unique owned offline fixture directory");
        const auto primary=root/"offline-input.control",source=root/"source.qpx",resaved=root/"resaved.qpx";
        {std::ofstream out(primary,std::ios::binary);out<<"owned ASCII input; no image decoder or module\n";out.close();check(bool(out),"Write owned offline primary input");}
        const auto primary_before=bytes(primary);
        auto document=std::make_shared<Document>(Image(2,2,std::vector<std::uint8_t>{40,80,160,40,80,160,40,80,160,40,80,160}));
        const auto image=seed.add_document(document);check(image==1,"Fixture image identity");
        const auto object=document->add_rectangle({0,0,2,2});
        AnalysisRequest request(*document,object,StainTransform({{1,0,0},{0,1,0}}),AnalysisKind::OpticalDensityMeans);
        const auto result=analyze(request);check(result.samples==4 && result.input_sha256.size()==64,"Native retained region fixture");
        const auto record=document->keep_analysis(request,result);seed.save_new(source.u8string());
        const auto retained_before=history(source);
        rd::ReaderPluginLink link;auto& implementation=link.implementation;
        implementation.provider_id="org.qupathxx.offline";implementation.version="1.0.0";
        implementation.contract="offline-raw/1";implementation.profile="offline-native-test/1";
        implementation.module_sha256=std::string(64,'a');implementation.capabilities=QPX_READER_CAP_RGBA|QPX_READER_CAP_RAW;
        link.metadata=document->metadata();auto raw=std::make_shared<RawImageMetadata>();
        raw->width=2;raw->height=2;raw->levels=link.metadata.levels;raw->channels=2;raw->z_planes=2;raw->timepoints=1;
        raw->sample_type=SampleType::UInt16;raw->channel_names={"first","second"};raw->dimension_order="XYZCT";link.raw=raw;
        {rd::ReaderFileGuard guard(primary.u8string());link.observed=*make_reader_source(rd::reader_plugin_binding(implementation),primary.u8string(),"single_file",guard.version());guard.verify();}
        const auto encoded=rd::encode_reader_plugin_link(link);
        check(encoded.size()<=rd::reader_plugin_link_limit && rd::encode_reader_plugin_link(*rd::decode_reader_plugin_link(encoded))==encoded,
            "Offline fixture uses bounded canonical native link codec");
        attach_saved_link(source,encoded);const auto source_before=bytes(source);
        const auto project=Project::open(source.u8string());const auto opened=project->document(image);
        const auto images=project->images();
        check(images.size()==1 && !images.front().embedded && !images.front().available && images.front().reader==implementation.provider_id,
            "Offline project lists the unavailable saved provider");
        const auto metadata=opened->server()->raw_metadata();
        check(metadata && metadata->channels==2 && metadata->z_planes==2 && metadata->timepoints==1 &&
            metadata->sample_type==SampleType::UInt16 && metadata->channel_names==raw->channel_names && !opened->server()->raw_view(),
            "Offline project exposes cached raw facts without a TIFF view");
        const auto& retained=opened->analysis_record(record);
        check(opened->analysis_record_count()==1 && retained.result.samples==4 && retained.result.input_sha256==result.input_sha256 &&
            retained.result.means==result.means,"Offline project retains readable historical region results");
        project->save_new(resaved.u8string());
        check(query(source,"SELECT image_id,payload FROM plugin_links ORDER BY image_id")==
            query(resaved,"SELECT image_id,payload FROM plugin_links ORDER BY image_id") &&
            history(source)==retained_before && history(resaved)==retained_before,"Offline resave preserves exact link and historical payloads");
        // This public bootstrap succeeds only if every preceding operation left
        // the catalog unfrozen. The manifest deliberately does not exist.
        const auto manifest=root/"not-present.json";check(!fs::exists(manifest),"Offline test has no module manifest");
        qpx::configure_reader_plugins(manifest.u8string());
        provider_refuses([&]{opened->server()->read_region({0,0,0,1,1});});
        provider_refuses([&]{RawRegionRequest region;opened->server()->read_raw(region);});
        check(bytes(primary)==primary_before && bytes(source)==source_before,"Offline operations preserve primary and project input bytes");
        std::cout<<"Offline plugin project: loader-free parse/cache/resave, exact reports and unavailable-provider refusal passed\n";
    }catch(const std::exception& error) {std::cerr<<error.what()<<'\n';return 1;}
}
