#pragma once
#include "raw_pixel_workflow_checks.hpp"
#include "raw_pixel_archive_checks.hpp"
#include "raw_window_checks.hpp"
#include "qupathxx/raw_pixel_tiled_training.hpp"
#include "qupathxx/raw_pixel_window_workflows.hpp"
#include <type_traits>
namespace raw_tiled_training_checks {
inline void require(bool value,const char* message) {if(!value) throw std::runtime_error(std::string("Raw tiled training: ")+message);}
template<class A,class B> void same(const A& a,const B& b) {
    require(a.values()==b.values(),"feature rows differ across partitions");require(a.labels()==b.labels(),"class labels differ");require(a.coordinates()==b.coordinates(),"global coordinates differ");require(a.annotation_ids()==b.annotation_ids(),"minimum-ID owners differ");require(a.classes()==b.classes(),"class order differs");require(a.eligible_class_samples()==b.eligible_class_samples(),"eligible counts differ");require(a.sampled_class_counts()==b.sampled_class_counts(),"retained counts differ");require(a.ambiguous_samples()==b.ambiguous_samples(),"ambiguous counts differ");
}
inline void run() {
    using namespace qpx;using raw_feature_checks::rejects;
    static_assert(!std::is_convertible_v<const RawPixelTiledTrainingTable&,const RawPixelTrainingTable&>);
    auto source=std::make_shared<raw_workflow_checks::Reader>();Document doc(source);
    const auto a=doc.add_detection(Roi::rectangle({0,0,3,5}),"A",0,"A"),b=doc.add_detection(Roi::rectangle({4,0,3,5}),"B",0,"B"),duplicate=doc.add_detection(Roi::rectangle({0,0,2,5}),"duplicate",0,"A");
    std::vector<std::uint64_t> ids{b,duplicate,a};for(unsigned i=0;i<12;++i) ids.push_back(doc.add_detection(Roi::rectangle({0,0,3,5}),"A overlap",0,"A"));
    const auto conflict=doc.add_detection(Roi::rectangle({2,0,3,5}),"conflict",0,"B");ids.push_back(conflict);
    ScientificRawOptions selection;selection.channels=2;RawPixelFeatureOptions features;features.offsets={0,0};features.scales={1,1};RawPixelTrainingOptions options;options.max_samples=12;options.max_samples_per_class=6;options.work_bytes=8*1024*1024;
    const ScientificGrid grid(7,5,{0,6,7,4},{0,5,6,4});const ScientificRawLattice lattice(grid);
    std::shared_ptr<const RawPixelTiledTrainingTable> last;
    for(bool filtered:{false,true}) {
        auto f=features;if(filtered) {f.sigmas={{0,0},{.5,1}};f.gradient_magnitude=true;f.laplacian=true;}
        const RawPixelTrainingRequest full(doc,ids,grid,selection,f,options);const auto baseline=sample_raw_pixel_training(full);
        for(const RawPixelTileOptions tiles:std::vector<RawPixelTileOptions>{{2,2,100},{3,1,100},{64,64,100}}) {
            const RawPixelTiledTrainingRequest request(doc,ids,lattice,selection,f,options,tiles);
            require(request.matches(doc) && request.scanned_samples()==42 && request.window_count()<=100,"unique union coverage");require(request.recipe().bytes()==full.recipe().bytes(),"lattice recipe changed portable identity");
            const auto table=sample_raw_pixel_tiled_training(request);same(*baseline,*table);require(table->retained_bytes()<options.work_bytes && table->estimated_work_bytes()<=options.work_bytes,"bounded training memory");
            require(sample_raw_pixel_tiled_training(request)->sha256()==table->sha256(),"repeat identity differs");if(last && !filtered) require(last->input_sha256()!=table->input_sha256(),"tile policy absent from input commitment");last=table;
        }
    }
    // Positive backend comparison uses the same effective budget, rather than
    // assuming fresh table admission always chooses identical model parameters.
    const RawPixelTrainingRequest full(doc,ids,grid,selection,features,options);const auto baseline=sample_raw_pixel_training(full);
    const RawPixelTiledTrainingRequest request(doc,ids,lattice,selection,features,options,{2,2,100});const auto table=sample_raw_pixel_tiled_training(request);
    RawPixelTiledTrainingJob job;const auto revision=doc.revision();require(job.sample(request),"sample dispatch");job.await_completion();require(!job.table() && !job.busy(),"await published result");require(job.poll(doc).state=="ready" && job.table()->sha256()==table->sha256() && doc.revision()==revision,"owner sample delivery");
    if(has_classifier_training()) {
        ForestParameters p;p.trees=7;p.max_depth=3;p.work_bytes=2*1024*1024;
        const auto expected=train_raw_pixel_classifier(*baseline,p),actual=train_raw_pixel_tiled_classifier(*table,p);require(expected.bytes()==actual.bytes(),"actual backend model differs");
        require(job.train(request,p),"backend job dispatch");const auto stats=job.wait(doc);if(stats.state!="ready") throw std::runtime_error("Raw tiled training backend: "+stats.error);require(job.model() && job.model()->bytes()==actual.bytes(),"job model differs");
        const auto window=lattice.window(1,1,3,3);const auto input=read_scientific_raw_window(*source,window,selection);const auto left=classify_raw_pixel_window(input,expected),right=classify_raw_pixel_window(input,actual);require(left->labels()==right->labels() && std::memcmp(left->vote_fractions().data(),right->vote_fractions().data(),left->vote_fractions().size()*sizeof(float))==0,"trained prediction votes differ");
        std::cout<<"Raw tiled training: actual backend full/tiled matrix, portable model bytes and prediction votes passed\n";
    } else {const auto before=source->reads.load();rejects<std::runtime_error>([&]{job.train(request);});require(source->reads.load()==before,"backend OFF performed IO");}
    auto bad=options;bad.max_scanned_samples=1;const auto before=source->reads.load();rejects<std::length_error>([&]{RawPixelTiledTrainingRequest invalid(doc,ids,lattice,selection,features,bad);});
    rejects<std::length_error>([&]{RawPixelTiledTrainingRequest invalid(doc,ids,lattice,selection,features,options,{1,1,1});});bad=options;bad.work_bytes=1024*1024;rejects<std::length_error>([&]{RawPixelTiledTrainingRequest invalid(doc,ids,lattice,selection,features,bad);});rejects<std::invalid_argument>([&]{RawPixelTiledTrainingRequest invalid(doc,{a,a,b},lattice,selection,features,options);});
    std::atomic<bool> cancelled{true};rejects<std::runtime_error>([&]{sample_raw_pixel_tiled_training(request,&cancelled);});require(source->reads.load()==before,"preflight refusal performed IO");
    Document copied=doc;require(!request.matches(copied),"copied document accepted");
    source->arm();require(job.sample(request),"blocked dispatch");source->wait_entered();require(job.busy() && !job.sample(request),"duplicate dispatch");job.cancel();source->release();require(job.wait(doc).state=="cancelled" && !job.table(),"cancel during read published");
    require(job.sample(request),"completed cancel dispatch");job.await_completion();job.cancel();require(job.poll(doc).state=="cancelled" && !job.table(),"completed cancellation published");
    source->failing.store(true);require(job.sample(request),"failure dispatch");require(job.wait(doc).state=="failed" && !job.table(),"decoder failure published");source->failing.store(false);require(job.sample(request) && job.wait(doc).state=="ready","failure retry");
    require(std::async(std::launch::async,[&] {try {job.table();return false;}catch(const std::runtime_error&) {return true;}}).get(),"cross-thread delivery accepted");
    require(job.sample(request),"stale dispatch");job.await_completion();doc.classify_annotation(a,"changed");require(job.poll(doc).state=="stale" && !job.table(),"edited label delivered");doc.classify_annotation(a,"A");
    const RawPixelTiledTrainingRequest fresh(doc,ids,lattice,selection,features,options);
    const auto count=source->reads.load();source->raw->series=1;rejects<std::invalid_argument>([&]{sample_raw_pixel_tiled_training(fresh);});source->raw->series=0;require(source->reads.load()==count,"changed series reached decoder");
    const auto name=source->raw->channel_names[0];source->raw->channel_names[0]=std::string(4*options.work_bytes,'x');rejects<std::length_error>([&]{sample_raw_pixel_tiled_training(fresh);});source->raw->channel_names[0]=name;require(source->reads.load()==count,"metadata quota reached decoder");
    require(job.sample(fresh),"late metadata dispatch");job.await_completion();const auto original_order=source->raw->dimension_order;source->raw->dimension_order+="changed";require(job.poll(doc).state=="stale" && !job.table(),"late metadata delivered");source->raw->dimension_order=original_order;
    doc.set_calibration(.5,.25);auto physical=features;physical.sigmas={{.5,.5}};const ScientificRawLattice physical_lattice(7,5,{0,7,7,6},{0,5,5,6});const RawPixelTiledTrainingRequest physical_request(doc,ids,physical_lattice,selection,physical,options,{2,2,100},true);
    require(physical_request.recipe().features().sigmas[0].x==1 && physical_request.recipe().features().sigmas[0].y==2,"document physical calibration ignored");require(job.sample(physical_request) && job.wait(doc).state=="ready","physical training");require(job.sample(physical_request),"physical stale dispatch");job.await_completion();doc.set_calibration(.25,.5);require(job.poll(doc).state=="stale" && !job.table(),"calibration change delivered");
    // Only one 64x64 candidate tile is needed on this trillion-cell lattice.
    auto large=std::make_shared<raw_window_checks::LargeReader>();Document huge(large);const auto ha=huge.add_detection(Roi::rectangle({543201.2,123455.8,.2,1.3}),"A",0,"A"),hb=huge.add_detection(Roi::rectangle({543202.2,123455.8,.2,1.3}),"B",0,"B");
    const ScientificRawLattice huge_lattice(1000000,1000000,{-1,999983,1000000,1},{2,999979,1000000,1});RawPixelFeatureOptions hf;hf.offsets={static_cast<double>(raw_window_checks::LargeReader::value(543200,123455))};hf.scales={1};
    RawPixelTrainingOptions ho;ho.max_scanned_samples=4096;const RawPixelTiledTrainingRequest sparse(huge,{ha,hb},huge_lattice,{},hf,ho);
    require(large->reads==0 && sparse.window_count()==1 && sparse.scanned_samples()==4096,"sparse planning scanned full lattice");const auto selected=sample_raw_pixel_tiled_training(sparse);require(selected->eligible_class_samples()==std::vector<std::uint64_t>({2,2}) && selected->row_count()==4,"sparse global ROI membership");
    for(std::size_t row=0;row<4;++row) {require(selected->coordinates()[2*row]==543211+selected->labels()[row],"sparse coordinate was local");require(selected->coordinates()[2*row+1]==123456 || selected->coordinates()[2*row+1]==123457,"sparse global Y");}
    // Fresh actual compound-file guards apply at completed owner delivery.
    const auto directory=std::filesystem::temp_directory_path()/("qpx-raw-tiled-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));std::filesystem::create_directory(directory);
    const auto primary=directory/"primary.bin",companion=directory/"companion.bin";{std::ofstream x(primary);x<<"primary";std::ofstream y(companion);y<<"companion";}
    auto files=std::make_shared<raw_workflow_checks::Reader>();files->files(primary.u8string(),companion.u8string());Document compound(files);const auto ca=compound.add_detection(Roi::rectangle({0,0,3,5}),"A",0,"A"),cb=compound.add_detection(Roi::rectangle({4,0,3,5}),"B",0,"B");
    RawPixelTiledTrainingJob filejob;require(filejob.sample(RawPixelTiledTrainingRequest(compound,{ca,cb},lattice,selection,features,options)),"compound dispatch");filejob.await_completion();{std::ofstream changed(companion,std::ios::app);changed<<"changed";}require(filejob.poll(compound).state=="stale" && !filejob.table(),"changed companion delivered");files->close();std::filesystem::remove(primary);std::filesystem::remove(companion);
    if(has_tiff() && has_project()) {
        const auto image=directory/"source.tif";raw_archive_checks::simple_tiff(image);auto project=std::make_shared<Project>();const auto image_id=project->add_tiff_image(image.u8string(),{});const auto live=project->document(image_id);const auto ta=live->add_detection(Roi::rectangle({0,0,3,6}),"A",0,"A"),tb=live->add_detection(Roi::rectangle({4,0,4,6}),"B",0,"B");
        const ScientificRawLattice real(8,6,{0,8,7,2},{0,6,5,2});const RawPixelTiledTrainingRequest real_request(*live,{ta,tb},real,selection,features,options,{2,2,100});RawPixelTiledTrainingJob actual;require(actual.sample(real_request) && actual.wait(*live).state=="ready","real TIFF sampling");const auto retained=actual.table();
        if(has_classifier_training()) {ForestParameters p;p.trees=3;p.work_bytes=2*1024*1024;require(actual.train(real_request,p) && actual.wait(*live).state=="ready" && actual.model(),"real TIFF actual training");const auto predicted=predict_raw_pixel_window(RawPixelWindowPredictionRequest(*live,*actual.model(),real.window(1,1,3,3)));require(predicted->labels().size()==9,"trained real TIFF prediction");}
        require(actual.sample(real_request),"TIFF stale dispatch");actual.await_completion();{std::ofstream changed(image,std::ios::binary|std::ios::app);changed<<"changed";}require(actual.poll(*live).state=="stale" && !actual.table() && retained->row_count()>0,"late TIFF mutation delivered");live->server()->close();project.reset();std::filesystem::remove(image);
        std::cout<<"Raw tiled training: real TIFF fractional sampling, optional backend prediction and late-file refusal passed\n";
    }
    std::filesystem::remove(directory);
    std::cout<<"Raw tiled training: full/tiled global reservoir rows, filtered seams, sparse trillion-cell lattice, quotas, owner lifecycle, calibration and compound-source controls passed\n";
}
}
