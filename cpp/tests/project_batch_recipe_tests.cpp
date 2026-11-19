// SPDX-License-Identifier: AGPL-3.0-only
#include "qupathxx/file_fingerprint.hpp"
#include "qupathxx/pixel_stream_file.hpp"
#include "qupathxx/project_batch.hpp"
#include "object_forest_fixture.hpp"
#include "quantitative_tiff_fixture.hpp"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <nlohmann/json.hpp>
#include <random>

namespace {
using namespace qpx;
using Json=nlohmann::json;
namespace fs=std::filesystem;

void check(bool ok,const char* reason) {
    if(!ok)throw std::runtime_error(reason);
}

std::string sha(const fs::path& path) {
    return fingerprint_file(path.u8string())->sha256;
}

void write(const fs::path& path,const Json& value) {
    std::ofstream out(path,std::ios::binary);
    out<<value.dump(2);
    check(bool(out),"Cannot write recipe fixture");
}
void bytes(const fs::path& path,const std::string& value) {
    std::ofstream out(path,std::ios::binary);
    out<<value;
    check(bool(out),"Cannot write scratch fixture");
}

Json fixture(const fs::path& root) {
    check(fs::create_directory(root),"Recipe fixture must use a fresh directory");
    StainTransform transform({{1,0,0},{0,1,0}});
    PixelFeatureOptions features;
    features.sigmas={0,1};
    std::vector<std::pair<std::string,std::string>> schema;
    for(const auto& feature:pixel_feature_schema(features,transform))
        schema.emplace_back(feature.name,feature.unit);
    const PixelClassifier model(ForestModel::from_bytes(qpx_test::object_forest_fixture(schema)),
        features,transform,PixelResolution{});
    const auto model_file=root/"model.qpc", source_file=root/"input.qpxproj";
    save_pixel_classifier(model,model_file.u8string());
    Project project;
    Json selection=Json::array();
    for(const auto label:{"first","second"}) {
        auto doc=std::make_shared<Document>(Image(32,24,std::vector<std::uint8_t>(32*24*3,16)));
        doc->add_rectangle({0,0,6,6});
        const auto chosen=doc->add_rectangle({1,2,20,16});
        const auto image=project.add_document(doc,label);
        selection.push_back({{"image",image},{"objects",{chosen}}});
    }
    project.save_new(source_file.u8string());
    Json recipe={{"schema","qpx.project-pixel-batch-recipe/1"},
        {"project",{{"path","input.qpxproj"},{"sha256",sha(source_file)}}},
        {"model",{{"path","model.qpc"},{"sha256",sha(model_file)}}},
        {"selection",selection},{"failure","stop"},{"resume",true},
        {"pixels",{{"block_size",16},{"tile_size",16},{"work_bytes",8*1024*1024},{"min_vote_fraction",0}}}};
    write(root/"recipe.json",recipe);
    return recipe;
}

void workflow(const fs::path& root) {
    const auto recipe=fixture(root);
    const auto output=root/"output";
    const auto path=root/"recipe.json";
    std::atomic<bool> cancel{false};
    const auto interrupted=run_project_pixel_batch_recipe(path.u8string(),output.u8string(),&cancel,
        [&](const auto& image){if(image.state=="saved")cancel.store(true);});
    check(interrupted.state=="cancelled" && interrupted.saved==1,"Completed image lost on interruption");
    const auto folder=fs::u8path(interrupted.images.front().path).parent_path().parent_path();
    const std::string work_token(32,'a'),stream_token(32,'b'),mixed_token(32,'c');
    const auto work=output/(".qupathxx-batch-work-"+work_token);
    const auto stream=folder/("pixels-"+stream_token);
    const auto mixed=folder/("pixels-"+mixed_token);
    check(fs::create_directory(work) && fs::create_directory(stream) && fs::create_directory(mixed),
        "Cannot create abandoned scratch fixture");
    bytes(work/"owner","qupathxx batch work 1\n"+work_token);
    bytes(work/"input.qpxproj-wal","orphan");
    bytes(stream/"owner","qupathxx pixel batch 1\n"+stream_token);
    bytes(stream/("object-2.qws.qpx-tmp-"+std::string(24,'d')),"orphan");
    bytes(mixed/"owner","qupathxx pixel batch 1\n"+mixed_token);
    bytes(mixed/"object-2.qps","stream");
    bytes(mixed/"notes.txt","keep");
    cancel.store(false);
    const auto resumed=run_project_pixel_batch_recipe(path.u8string(),output.u8string(),&cancel);
    check(resumed.state=="succeeded" && resumed.saved==1 && resumed.reused==1,
        "Recipe did not reuse the verified first image and finish the second");
    check(!fs::exists(work) && !fs::exists(stream) && fs::exists(mixed/"owner") &&
          fs::exists(mixed/"object-2.qps") && fs::exists(mixed/"notes.txt"),
          "Restart failed to distinguish owned scratch and mixed contents");
    for(std::size_t index=0;index<resumed.images.size();++index) {
        const auto& image=resumed.images[index];
        const auto expected=recipe.at("selection").at(index).at("objects").at(0).get<std::uint64_t>();
        const auto saved=Project::open_without_journal_recovery(image.path);
        const auto records=saved->document(image.image_id)->pixel_stream_records();
        check(records.size()==1 && records.front()->link->object_id()==expected,"Stored project selected the wrong ROI");
        const PixelStreamFile artifact(records.front()->link->path());
        artifact.verify();
        check(artifact.link()->same_content(*records.front()->link) &&
              batch_checkpoint_csv(image.path).find("class_pixels")!=std::string::npos,
              "Saved project does not reference the exact exported result");
        if(image.state=="reused")check(image.inferred_blocks==0,"Reused image ran inference");
    }
    check(sha(root/"input.qpxproj")==recipe.at("project").at("sha256").get<std::string>(),
        "Recipe changed the source project");

    const auto rejected=[&](const char* name,Json modified,const char* expected) {
        auto bad=root/(std::string(name)+".json");
        auto target=root/name;
        write(bad,modified);
        try { (void)run_project_pixel_batch_recipe(bad.u8string(),target.u8string()); }
        catch(const std::exception& error) {
            check(std::string(error.what()).find(expected)!=std::string::npos && !fs::exists(target),
                "Recipe failed for an unexpected reason or published output");
            return;
        }
        throw std::runtime_error("Invalid recipe was accepted");
    };
    auto modified=recipe;
    modified["schema"]="qpx.project-pixel-batch-recipe/2";
    rejected("bad-version",modified,"version");
    modified=recipe;
    modified["model"]["sha256"]=std::string(64,'0');
    rejected("wrong-model",modified,"Pixel model file SHA-256");
    modified=recipe;
    modified["project"]["sha256"]=std::string(64,'0');
    rejected("wrong-project",modified,"Project file SHA-256");
    modified=recipe;
    modified["selection"][0]["objects"].push_back(modified["selection"][0]["objects"][0]);
    rejected("duplicate-roi",modified,"Duplicate pixel batch ROI");
}

void raw_workflow(const fs::path& root) {
    if(!has_tiff()) return;
    check(fs::create_directory(root),"Raw recipe fixture must use a fresh directory");
    const auto source=root/"source.tif";
    qpx_test::raw_pixel_tiff_fixture(source);
    const auto server=open_tiff(source.u8string());
    ScientificRawOptions channels;
    channels.channels=2;
    RawPixelFeatureOptions features;
    features.offsets={0,0};
    features.scales={1,1};
    const RawPixelRecipe sampling(*server->raw_metadata(),
        ScientificRawLattice(8,6,{0,8,8},{0,6,6}),channels,features);
    std::vector<std::pair<std::string,std::string>> schema;
    for(const auto& feature:sampling.feature_schema())
        schema.emplace_back(feature.name,feature.unit);
    const RawPixelClassifier model(ForestModel::from_bytes(qpx_test::object_forest_fixture(schema)),sampling);
    const auto model_file=root/"model.qrm", project_path=root/"input.qpxproj";
    save_raw_pixel_classifier(model,model_file.u8string());
    Project project;
    Json selection=Json::array();
    for(std::size_t i=0;i<2;++i) {
        auto doc=std::make_shared<Document>(open_tiff(source.u8string()));
        if(i) doc->add_rectangle({1,1,2,2});
        const auto chosen=doc->add_rectangle({0,0,8,6});
        doc->add_rectangle({2,2,2,2});
        selection.push_back({{"image",project.add_document(doc)},{"objects",{chosen}}});
    }
    project.save_new(project_path.u8string());
    const auto recipe_path=root/"raw.json", output=root/"output";
    Json raw={{"schema","qpx.project-raw-pixel-batch-recipe/1"},
        {"project",{{"path","input.qpxproj"},{"sha256",sha(project_path)}}},
        {"model",{{"path","model.qrm"},{"sha256",sha(model_file)}}},
        {"selection",selection},{"failure","stop"},{"resume",true},
        {"raw",{{"kind","raw-pixels"},{"grid",{{"mode","model"}}},
         {"stream",{{"tile_width",2},{"tile_height",2},{"work_bytes",16*1024*1024},
                    {"min_vote_fraction",0},{"max_samples",1000},{"max_windows",1000}}}}}};
    write(recipe_path,raw);
    std::atomic<bool> cancel{false};
    const auto stopped=run_project_pixel_batch_recipe(recipe_path.u8string(),output.u8string(),&cancel,
        [&](const auto& image){if(image.state=="saved")cancel.store(true);});
    check(stopped.state=="cancelled" && stopped.saved==1,"Raw recipe did not preserve the first result on cancellation");
    cancel.store(false);
    const auto resumed=run_project_pixel_batch_recipe(recipe_path.u8string(),output.u8string(),&cancel);
    check(resumed.state=="succeeded" && resumed.reused==1 && resumed.saved==1,
          "Raw recipe did not reuse the verified image and finish the remaining image");
    for(std::size_t i=0;i<resumed.images.size();++i) {
        const auto& result=resumed.images[i];
        const auto saved=Project::open_without_journal_recovery(result.path);
        const auto records=saved->document(result.image_id)->raw_pixel_stream_records();
        check(records.size()==1 && records.front()->link->object_id()==selection[i]["objects"][0].get<std::uint64_t>(),
              "Raw recipe selected a different ROI");
        RawPixelStreamFile stream(records.front()->link->path());
        stream.verify();
        check(stream.link()->same_content(*records.front()->link) && stream.declared_summary().valid_samples==48,
              "Raw recipe saved a different stream");
        if(result.state=="reused") check(result.inferred_blocks==0,"Resumed raw recipe repeated inference");
    }
    check(sha(project_path)==raw["project"]["sha256"] && sha(model_file)==raw["model"]["sha256"],
          "Raw recipe changed its source project or model");
    std::cout<<"Pinned raw recipe selected distinct ROIs, verified stored streams and resumed after cancellation\n";
}
}

int main(int argc,char** argv) {
    try {
        if(argc==3 && std::string(argv[1])=="--fixture") {
            (void)fixture(fs::absolute(fs::u8path(argv[2])));
            return 0;
        }
        if(argc==4 && std::string(argv[1])=="--hold-after-image") {
            const auto report=run_project_pixel_batch_recipe(argv[2],argv[3],nullptr,[](const auto& image){
                if(image.state=="saved") {
                    std::cout<<image.path<<std::endl;
                    (void)std::cin.get();
                }
            });
            return report.state=="succeeded"?0:1;
        }
        const auto root=fs::current_path()/("pixel-recipe-"+std::to_string(std::random_device{}()));
        workflow(root);
        raw_workflow(root/"raw");
        std::cout<<"Versioned pixel recipe, selection, resume, export, reopen and pinning passed\n";
        return 0;
    } catch(const std::exception& error) {
        std::cerr<<error.what()<<'\n';
        return 1;
    }
}
