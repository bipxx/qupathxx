#include "qupathxx/training_cohort.hpp"
#include "raw_pixel_workflow_checks.hpp"
#include <iostream>
using namespace qpx;
void check_at(bool value,int line){if(!value)throw std::runtime_error("Training cohort failed at line "+std::to_string(line));}
#define check(...) check_at((__VA_ARGS__),__LINE__)
int main(){try{
    auto source=std::make_shared<raw_workflow_checks::Reader>();Document doc(source);
    const auto a=doc.add_detection(Roi::rectangle({0,0,3,5}),"A",0,"A");
    const auto unlabelled=doc.add_rectangle({0,0,3,5});
    const auto line=doc.add_roi(Roi::polyline({{0,0},{5,4}}));doc.classify_annotation(line,"A");
    const auto b=doc.add_detection(Roi::rectangle({4,0,3,5}),"B",0,"B");
    const auto cell=doc.add_cell(Roi::rectangle({1,1,1,1}),{},"Cell",0,"A");
    ScientificRawOptions selection;selection.channels=2;RawPixelFeatureOptions features;features.offsets={0,0};features.scales={1,1};
    RawPixelTrainingOptions options;options.max_samples=12;options.max_samples_per_class=6;options.work_bytes=8*1024*1024;
    const ScientificRawLattice lattice(ScientificGrid(7,5,{0,6,7,4},{0,5,6,4}));const RawPixelTileOptions tiles{2,2,100};
    const auto revision=doc.revision();const auto captured=capture_labelled_raw_pixel_training(doc,lattice,selection,features,options,tiles);
    const RawPixelTiledTrainingRequest explicit_ids(doc,{cell,b,a},lattice,selection,features,options,tiles);
    check(source->reads.load()==0 && captured.matches(doc) && doc.revision()==revision);
    const auto native=sample_raw_pixel_tiled_training(captured),reference=sample_raw_pixel_tiled_training(explicit_ids);
    check(native->sha256()==reference->sha256() && native->input_sha256()==reference->input_sha256());
    check(native->values()==reference->values() && native->labels()==reference->labels() && native->coordinates()==reference->coordinates());
    check(native->annotation_ids()==reference->annotation_ids() && native->classes()==reference->classes());
    check(native->eligible_class_samples()==reference->eligible_class_samples() && native->sampled_class_counts()==reference->sampled_class_counts());
    check(std::find(native->annotation_ids().begin(),native->annotation_ids().end(),unlabelled)==native->annotation_ids().end());
    check(std::find(native->annotation_ids().begin(),native->annotation_ids().end(),line)==native->annotation_ids().end());
    doc.classify_annotation(cell,"");check(!captured.matches(doc));
    const auto changed=capture_labelled_raw_pixel_training(doc,lattice,selection,features,options,tiles);
    const RawPixelTiledTrainingRequest changed_explicit(doc,{a,b},lattice,selection,features,options,tiles);
    check(sample_raw_pixel_tiled_training(changed)->sha256()==sample_raw_pixel_tiled_training(changed_explicit)->sha256());
    auto invalid=options;invalid.max_samples=1;bool refused=false;
    try{capture_labelled_raw_pixel_training(doc,lattice,selection,features,invalid,tiles);}catch(const std::invalid_argument&){refused=true;}check(refused);
    Document empty(source);refused=false;try{capture_labelled_raw_pixel_training(empty,lattice,selection,features,options,tiles);}catch(const std::invalid_argument&){refused=true;}check(refused);
    Document too_many(source);too_many.set_class("A",0);std::vector<ObjectEdit> edits;
    for(int i=0;i<4097;++i)edits.push_back(ObjectEdit::create_geometry(Roi::rectangle({0,0,1,1}),"Area",0,"A"));
    too_many.execute(edits);const auto reads=source->reads.load();refused=false;
    try{capture_labelled_raw_pixel_training(too_many,lattice,selection,features,options,tiles);}catch(const std::invalid_argument& e){refused=std::string(e.what()).find("4096")!=std::string::npos;}
    check(refused && source->reads.load()==reads);
    std::cout<<"Native training cohort: labelled area eligibility/order, all object kinds, no capture reads, exact table/model inputs, stale capture and native limits passed\n";
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
