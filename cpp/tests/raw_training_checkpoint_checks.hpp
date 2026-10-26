#pragma once
#include "raw_tiled_archive_checks.hpp"
#include "qupathxx/raw_pixel_training_checkpoint.hpp"
namespace raw_training_checkpoint_checks {
using Bytes=std::vector<std::uint8_t>;
inline void require(bool v,const char* m) {if(!v) throw std::runtime_error(std::string("Raw training checkpoint: ")+m);}
inline void put(const std::filesystem::path& p,const Bytes& b) {std::ofstream f(p,std::ios::binary|std::ios::trunc);f.write(reinterpret_cast<const char*>(b.data()),std::streamsize(b.size()));}
inline void run() {
    using namespace qpx;using raw_feature_checks::rejects;using raw_tiled_archive_checks::same;
    const auto directory=std::filesystem::temp_directory_path()/("qpx-training-checkpoint-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));std::filesystem::create_directory(directory);
    const auto path=directory/"checkpoint.qtc",direct=directory/"direct.qtc",other=directory/"other.qtc";
    auto source=std::make_shared<raw_workflow_checks::Reader>();Document doc(source);
    const auto a=doc.add_detection(Roi::rectangle({0,0,3,5}),"A",0,"A"),b=doc.add_detection(Roi::rectangle({4,0,3,5}),"B",0,"B");
    const ScientificRawLattice lattice(7,5,{0,6,7,4},{0,5,6,4});ScientificRawOptions selection;selection.channels=2;
    RawPixelFeatureOptions features;features.offsets={0,0};features.scales={1,1};features.sigmas={{0,0},{.5,1}};features.gradient_magnitude=features.laplacian=true;
    RawPixelTrainingOptions options;options.max_samples=12;options.max_samples_per_class=6;options.work_bytes=8*1024*1024;
    for(bool balanced:{false,true}) {
        options.balanced=balanced;const RawPixelTiledTrainingRequest request(doc,{a,b},lattice,selection,features,options,{2,2,100});const auto baseline=sample_raw_pixel_tiled_training(request);
        RawPixelTrainingCheckpointOptions save;save.max_new_windows=0;const auto reads=source->reads.load();const auto empty=save_raw_pixel_training_checkpoint(request,path.u8string(),save);
        require(!empty.table && empty.checkpoint->completed_windows()==0 && !empty.checkpoint->complete() && source->reads.load()==reads,"empty checkpoint performed pixel IO or published table");
        require(empty.checkpoint->bytes()[0]=='Q' && empty.checkpoint->bytes()[1]=='T' && empty.checkpoint->bytes()[2]=='C' && empty.checkpoint->bytes()[3]==1,"QTC1 tag");
        // Independent wire cursor checks count/hash positions without native decode.
        raw_tiled_archive_checks::Cursor cursor{empty.checkpoint->bytes()};cursor.p+=80;cursor.source();const auto total_at=cursor.p;
        require(cursor.integer()==request.window_count() && cursor.integer()==0,"independent prefix dimensions");
        save.resume_path=path.u8string();save.replace_existing=true;save.max_new_windows=1;auto partial=save_raw_pixel_training_checkpoint(request,path.u8string(),save);
        require(partial.replayed_windows==0 && !partial.table && partial.checkpoint->completed_windows()==1,"first prefix publication");
        const auto prefix_bytes=partial.checkpoint->bytes();require(load_raw_pixel_training_checkpoint(path.u8string())->bytes()==prefix_bytes,"file readback");
        save.max_new_windows=2;partial=save_raw_pixel_training_checkpoint(request,path.u8string(),save);require(partial.replayed_windows==1 && partial.checkpoint->completed_windows()==3 && !partial.table,"multistage progress");
        save.max_new_windows=UINT64_MAX;const auto resumed=save_raw_pixel_training_checkpoint(request,path.u8string(),save);require(resumed.replayed_windows==3 && resumed.checkpoint->complete() && resumed.table,"completed prefix missing table");same(*baseline,*resumed.table);
        const auto uninterrupted=save_raw_pixel_training_checkpoint(request,direct.u8string());same(*baseline,*uninterrupted.table);require(uninterrupted.checkpoint->bytes()==resumed.checkpoint->bytes(),"resumed/uninterrupted checkpoint bytes differ");
        save.max_new_windows=0;const auto complete_again=save_raw_pixel_training_checkpoint(request,path.u8string(),save);same(*baseline,*complete_again.table);require(complete_again.replayed_windows==request.window_count(),"complete checkpoint skipped validation replay");
        if(has_classifier_training()) {ForestParameters p;p.trees=5;p.max_depth=3;p.work_bytes=2*1024*1024;require(train_raw_pixel_tiled_classifier(*baseline,p).bytes()==train_raw_pixel_tiled_classifier(*resumed.table,p).bytes(),"resumed backend model changed");std::cout<<"Raw training checkpoint: actual backend uninterrupted/resumed table and model bytes passed\n";}
        // A historical completed checkpoint is metadata, not a retained table.
        require(RawPixelTrainingCheckpoint::from_bytes(resumed.checkpoint->bytes())->complete(),"historical completion");
        auto malformed=prefix_bytes;raw_stream_file_checks::put64(malformed,total_at+8,request.window_count()+1);malformed=raw_tiled_archive_checks::rehash(std::move(malformed));rejects<std::invalid_argument>([&]{RawPixelTrainingCheckpoint::from_bytes(malformed);});
        malformed=prefix_bytes;malformed.resize(malformed.size()-1);rejects<std::invalid_argument>([&]{RawPixelTrainingCheckpoint::from_bytes(malformed);});
        // Rehashing does not make changed decoded input a valid live prefix.
        put(path,prefix_bytes);save.max_new_windows=0;source->altered.store(true);rejects<std::invalid_argument>([&]{save_raw_pixel_training_checkpoint(request,path.u8string(),save);});source->altered.store(false);require(load_raw_pixel_training_checkpoint(path.u8string())->bytes()==prefix_bytes,"bad replay replaced checkpoint");
        auto changed=options;changed.seed++;const RawPixelTiledTrainingRequest wrong(doc,{a,b},lattice,selection,features,changed,{2,2,100});const auto before=source->reads.load();rejects<std::invalid_argument>([&]{save_raw_pixel_training_checkpoint(wrong,path.u8string(),save);});require(source->reads.load()==before,"request mismatch reached pixel IO");
        malformed=prefix_bytes;const auto prefix_at=malformed.size()-64-64;malformed[prefix_at]=malformed[prefix_at]=='0'?'1':'0';put(other,raw_tiled_archive_checks::rehash(std::move(malformed)));auto forged=save;forged.resume_path=other.u8string();rejects<std::invalid_argument>([&]{save_raw_pixel_training_checkpoint(request,path.u8string(),forged);});require(load_raw_pixel_training_checkpoint(path.u8string())->bytes()==prefix_bytes,"forged prefix replaced checkpoint");
        std::atomic<bool> cancelled{true};rejects<std::runtime_error>([&]{save_raw_pixel_training_checkpoint(request,path.u8string(),save,&cancelled);});require(load_raw_pixel_training_checkpoint(path.u8string())->bytes()==prefix_bytes,"cancel replaced checkpoint");
        RawPixelTrainingCheckpointLimits tiny;tiny.max_bytes=256;rejects<std::length_error>([&]{load_raw_pixel_training_checkpoint(path.u8string(),tiny);});auto tight=save;tight.limits=tiny;rejects<std::length_error>([&]{save_raw_pixel_training_checkpoint(request,directory.u8string()+"/too-small",tight);});
        require(!std::filesystem::exists(directory/"too-small"),"byte quota left output");
        std::filesystem::remove(path);std::filesystem::remove(direct);std::filesystem::remove(other);
    }
    const RawPixelTiledTrainingRequest request(doc,{a,b},lattice,selection,features,options,{2,2,100});
    // A competing writer creates a destination while the sampler is blocked.
    source->arm();RawPixelTrainingCheckpointOptions race;race.replace_existing=true;auto future=std::async(std::launch::async,[&] {try {save_raw_pixel_training_checkpoint(request,path.u8string(),race);return false;}catch(const std::runtime_error&) {return true;}});source->wait_entered();put(path,Bytes{'k','e','e','p'});source->release();require(future.get(),"concurrent new destination overwritten");{std::ifstream f(path,std::ios::binary);require(std::string(std::istreambuf_iterator<char>(f),{})=="keep","competitor bytes replaced");}std::filesystem::remove(path);
    // Cancel during a window: the previous successful prefix remains intact.
    RawPixelTrainingCheckpointOptions save;save.max_new_windows=1;const auto original=save_raw_pixel_training_checkpoint(request,path.u8string(),save);save.resume_path=path.u8string();save.replace_existing=true;source->arm();std::atomic<bool> cancel{false};auto blocked=std::async(std::launch::async,[&] {try {save_raw_pixel_training_checkpoint(request,path.u8string(),save,&cancel);return false;}catch(const std::runtime_error&) {return true;}});source->wait_entered();cancel.store(true);source->release();require(blocked.get() && load_raw_pixel_training_checkpoint(path.u8string())->bytes()==original.checkpoint->bytes(),"cancel during replay changed checkpoint");std::filesystem::remove(path);
    if(has_tiff() && has_project()) {
        const auto image=directory/"source.tif",project_path=directory/"project.qpx";raw_archive_checks::simple_tiff(image);auto project=std::make_shared<Project>();const auto image_id=project->add_tiff_image(image.u8string(),{});auto live=project->document(image_id);const auto ta=live->add_detection(Roi::rectangle({0,0,3,6}),"A",0,"A"),tb=live->add_detection(Roi::rectangle({4,0,4,6}),"B",0,"B");live->set_calibration(.5,.25);
        const ScientificRawLattice real(8,6,{0,8,7,4},{0,6,5,4});RawPixelTrainingCheckpointOptions prefix;prefix.max_new_windows=2;
        auto first=save_raw_pixel_training_checkpoint(RawPixelTiledTrainingRequest(*live,{ta,tb},real,selection,features,options,{2,2,100}),path.u8string(),prefix);const auto first_bytes=first.checkpoint->bytes();project->save(project_path.u8string());live->server()->close();live.reset();project.reset();
        project=Project::open(project_path.u8string());live=project->document(image_id);const RawPixelTiledTrainingRequest reopened(*live,{ta,tb},real,selection,features,options,{2,2,100});prefix.resume_path=path.u8string();prefix.replace_existing=true;prefix.max_new_windows=UINT64_MAX;
        const auto final=save_raw_pixel_training_checkpoint(reopened,path.u8string(),prefix);require(final.replayed_windows==2 && final.table,"TIFF fresh-reader resume");same(*sample_raw_pixel_tiled_training(reopened),*final.table);
        if(has_classifier_training()) {ForestParameters p;p.trees=3;p.work_bytes=2*1024*1024;require(train_raw_pixel_tiled_classifier(*final.table,p).bytes()==train_raw_pixel_tiled_classifier(*sample_raw_pixel_tiled_training(reopened),p).bytes(),"TIFF resumed actual model");}
        auto alias=directory/"alias.tif";std::filesystem::create_hard_link(image,alias);rejects<std::invalid_argument>([&]{save_raw_pixel_training_checkpoint(reopened,alias.u8string(),prefix);});std::filesystem::remove(alias);
        live->set_calibration(.25,.5);const RawPixelTiledTrainingRequest changed(*live,{ta,tb},real,selection,features,options,{2,2,100});rejects<std::invalid_argument>([&]{save_raw_pixel_training_checkpoint(changed,path.u8string(),prefix);});
        {std::ofstream changed_file(image,std::ios::binary|std::ios::app);changed_file<<"changed";}rejects<std::runtime_error>([&]{save_raw_pixel_training_checkpoint(reopened,path.u8string(),prefix);});
        live->server()->close();live.reset();project.reset();std::filesystem::remove(path);std::filesystem::remove(image);std::filesystem::remove(project_path);
        // Project save's advisory lock is owned by the now-closed fixture only.
        const auto lock=std::filesystem::u8path(project_path.u8string()+".lock");if(std::filesystem::exists(lock)) std::filesystem::remove(lock);
        std::cout<<"Raw training checkpoint: real TIFF project close/reopen, fresh replay, optional trained model, calibration/file and source-alias refusal passed\n";
    }
    require(std::filesystem::is_empty(directory),"fixture leaked checkpoint temporaries or project sidecars");std::filesystem::remove(directory);
    std::cout<<"Raw training checkpoint: independent QTC1 bounds, balanced/unbalanced multistage replay, exact rows/hashes/bytes, input tamper, limits, cancellation and concurrent-writer refusal passed\n";
}
}
