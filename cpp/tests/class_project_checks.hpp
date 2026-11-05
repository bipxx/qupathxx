#pragma once
#include "qupathxx/raw_image.hpp"
#include "../sha256.hpp"

// Expectations below are from the retained, separately read-back QuPath0.7.0
// corpus. Native and assisted imports use the same pixels/builder/UUIDs.
inline void class_project_checks(const std::filesystem::path& scratch) {
    namespace fs=std::filesystem;
    const auto fixtures=fs::path(QPX_PROJECT_SOURCE_ROOT)/"tests/fixtures";
    const auto captured=fixtures/"qupath-class-hierarchy-0.7.0";
    const auto read=[](const fs::path& path) {std::ifstream f(path,std::ios::binary);check(bool(f));return std::string(std::istreambuf_iterator<char>(f),{});};
    const auto write=[](const fs::path& path,const std::string& bytes) {std::ofstream f(path,std::ios::binary);f<<bytes;check(bool(f));};
    const auto digest=[](const std::string& bytes) {qpx::Sha256 sha;sha.update(bytes.data(),bytes.size());return sha.finish();};
    const std::string tumor=u8"Tumor \u03b1",positive=tumor+": Positive",deep=positive+": High";
    const std::vector<std::string> palette{deep,tumor,tumor+": Negative",positive,u8"Cell \u00b5 \U0001f680","Marker / label #1",deep};
    const auto graph=[&](const Project& project,const std::string& name) {
        if(name=="rgb" || name=="empty") {
            check(project.class_defaults()==palette && project.classes().size()==6);
            check(project.classes().at(deep).parent==positive && project.classes().at(positive).parent==tumor);
            check(project.classes().at(deep).color_argb()==0x778899 && project.classes().at(tumor).color_argb()==0x113355);
        } else if(name=="rgba") {
            check(project.class_defaults()==std::vector<std::string>({tumor,tumor+": Alpha"}));
            check(project.classes().at(tumor+": Alpha").color_argb()==0x80123456);
        } else if(name=="null-color") {
            check(project.class_defaults()==std::vector<std::string>({"Cleared color"}));
            check(!project.classes().at("Cleared color").color_argb() && project.classes().at("Cleared color").color==0xc81767);
        } else check(project.classes().at("Conflict class").color_argb()==0xa0b0c0);
    };
    const auto exchange=[&](const fs::path& dir,const std::string& geometry) {
        const auto out=dir/"qupathxx-exchange";fs::create_directory(out);write(out/"objects.geojson",geometry);
        const auto classes=read(dir/"classifiers/classes.json"),metadata=read(dir/"data/metadata.json");
        write(out/"index.json",std::string(R"({"schema":"qpx.qupath-project-exchange/1","qupath_version":"0.7.0","project_sha256":")")+digest(read(dir/"project.qpproj"))+R"(","files":{"classifiers/classes.json":")"+digest(classes)+R"(","data/metadata.json":")"+digest(metadata)+R"("},"images":[{"entry_id":"1","qpdata_sha256":")"+digest(read(dir/"data/1/data.qpdata"))+R"(","file":"objects.geojson","sha256":")"+digest(geometry)+R"(","pixel_width_um":null,"pixel_height_um":null,"issues":[]}]})");
    };
    for(const std::string name:{"rgb","rgba","null-color","color-conflict"}) {
        const auto decoded=detail::decode_qupath_saved_data(read(captured/name/"data/1/data.qpdata"));
        for(const auto& issue:decoded.issues)check(issue.code!="classification_hierarchy_flattened");
        Document native(Image(64,48,std::vector<std::uint8_t>(64*48*3)));
        const auto imported=import_geojson(native,decoded.geojson,true,true);
        check(imported.features==(name=="rgb"?6:1));
        for(const auto& [id,object]:native.objects())check(object.geometry.area()==120);
        if(name=="rgb") {
            check(native.class_path(deep)==std::vector<std::string>({tumor,"Positive","High"}));
            check(native.object_class(deep).color_argb()==0x778899 && imported.source_ids.at(3)=="08031a24-2a03-4523-9f33-0bdff167ed28");
        } else if(name=="rgba")check(native.object_class(tumor+": Alpha").color_argb()==0x80123456);
        else if(name=="null-color")check(!native.object_class("Cleared color").color_argb() && native.object_class("Cleared color").color==0xc81767);
        else check(native.object_class("Conflict class").color_argb()==0x101112);
        Document assisted(Image(64,48,std::vector<std::uint8_t>(64*48*3)));
        const auto geo=import_geojson(assisted,read(captured/name/"objects.geojson"),true,true);
        check(geo.source_ids==imported.source_ids && assisted.object_count()==native.object_count());
        if(name=="rgba")check(assisted.object_class(tumor+": Alpha").color_argb()==0x80123456);
        if(name=="color-conflict")check(assisted.object_class("Conflict class").color_argb()==0xff101112);
    }
    // Empty projects need no reader; all other cases use one retained open TIFF.
    if(!has_tiff())std::cout<<"Class project image cases skipped: TIFF capability disabled\n";
    for(const std::string name:{"rgb","empty","rgba","null-color","color-conflict"}) {
        if(name!="empty" && !has_tiff())continue;
        const auto dir=scratch/("class-"+name);fs::copy(captured/name,dir,fs::copy_options::recursive);
        const std::map<std::string,std::string> overrides=name=="empty"?std::map<std::string,std::string>{}:std::map<std::string,std::string>{{"1",(fixtures/"qupath-root-facts-0.7.0/source.ome.tif").u8string()}};
        for(int route=0;route<(name=="empty"?1:2);++route) {
            if(route)exchange(dir,read(captured/name/"objects.geojson"));
            const auto preview=preview_qupath_project((dir/"project.qpproj").u8string(),overrides);
            for(const auto& issue:preview->issues())check(issue.code!="classification_hierarchy_flattened" && issue.code!="project_classes_dropped");
            if(name!="empty")check(preview->images().size()==1 && preview->images()[0].available);
            const auto path=scratch/(name+"-"+std::to_string(route)+".qpxproj");
            const auto project=preview->import_new(path.u8string(),true);graph(*project,name);
            const auto reopened=Project::open(path.u8string());graph(*reopened,name);
            if(name!="empty") {
                const auto doc=reopened->document(preview->images()[0].native_id);
                check(doc->object_count()==(name=="rgb"?6:1));
                for(const auto& [id,object]:doc->objects())check(object.geometry.area()==120);
                for(const auto& [key,c]:reopened->classes())check(doc->object_class(key)==c);
                if(name=="rgb")check(doc->class_path(deep)==std::vector<std::string>({tumor,"Positive","High"}));
                if(name=="color-conflict")check(doc->object_class("Conflict class").color_argb()==0xa0b0c0);
            } else check(reopened->images().empty());
        }
        if(name=="rgb") {
            Document flat(Image(64,48,std::vector<std::uint8_t>(64*48*3)));flat.set_class(positive,0xaa2211);
            flat.add_detection(Roi::rectangle({1,2,3,4}),"Flat collision",0,positive);
            const auto exchange_dir=dir/"qupathxx-exchange";
            write(exchange_dir/"objects.geojson",export_geojson(flat).text);
            // Recreate the exchange hash after replacing geometry.
            fs::remove(exchange_dir/"index.json");exchange(dir,export_geojson(flat).text);
            rejects([&]{preview_qupath_project((dir/"project.qpproj").u8string(),overrides);});
        }
    }
    const auto invalid=scratch/"class-invalid";fs::create_directories(invalid/"classifiers");
    write(invalid/"project.qpproj",R"({"version":"0.7.0","images":[]})");
    for(const auto& definitions:{
        R"({"pathClasses":[{"name":"A","color":1},{"name":"A","color":2}]})",
        R"({"pathClasses":[{"name":"A","color":4294967296}]})",
        R"({"pathClasses":[{"name":"A: : B","color":1}]})"}) {
        write(invalid/"classifiers/classes.json",definitions);rejects([&]{preview_qupath_project((invalid/"project.qpproj").u8string());});
    }
    write(invalid/"classifiers/classes.json",R"({"pathClasses":[{"name":"\u2003Parent\u2003: Child\u3000","color":-2146290602},{"name":"Parent","color":12},{"name":"Parent: Child","color":-2146290602}]})");
    const auto normalized=preview_qupath_project((invalid/"project.qpproj").u8string())->import_new((scratch/"class-unicode.qpxproj").u8string());
    check(normalized->class_defaults()==std::vector<std::string>({"Parent: Child","Parent","Parent: Child"}) && normalized->classes().size()==2);
    check(normalized->classes().at("Parent: Child").color_argb()==0x80123456);
    std::cout<<"Class binary/project corpus: exact ancestry/raw colors, UUID/geometry, native/assisted palette precedence, repeated defaults, empty projects, reopen and refusal passed\n";
}
