#include "raw_pixel_cli.hpp"
#include "qupathxx/raw_pixel_file_jobs.hpp"
#include "qupathxx/raw_pixel_archive.hpp"
#include "qupathxx/project.hpp"
#include "batch_checkpoint.hpp"
#include "atomic_export.hpp"
#include "reader_file_guard.hpp"
#include <charconv>
#include <chrono>
#include <cmath>
#include <iostream>
#include <limits>
#include <locale>
#include <set>
#include <sstream>
#include <thread>

namespace qpx::detail {
namespace {
template<class T> T integer(const std::string& text) {
    T n{};const auto r=std::from_chars(text.data(),text.data()+text.size(),n);
    if(text.empty() || r.ec!=std::errc{} || r.ptr!=text.data()+text.size()) throw std::invalid_argument("invalid integer: "+text);return n;
}
std::vector<std::string> parts(const std::string& text) {
    std::vector<std::string> out;std::size_t at=0;
    do {const auto end=text.find(',',at);out.push_back(text.substr(at,end==std::string::npos?end:end-at));if(out.back().empty() || out.size()>256) throw std::invalid_argument("invalid bounded comma-separated list");if(end==std::string::npos) return out;at=end+1;} while(true);
}
double number(const std::string& text) {
    std::istringstream in(text);in.imbue(std::locale::classic());double n;
    if(!(in>>n) || !std::isfinite(n)) throw std::invalid_argument("expected finite number: "+text);in>>std::ws;if(!in.eof()) throw std::invalid_argument("expected finite number: "+text);return n;
}
std::vector<double> numbers(const std::string& text) {std::vector<double> out;for(const auto& p:parts(text)) out.push_back(number(p));return out;}
ScientificGridAxis axis(const std::string& text) {
    const auto p=parts(text);if(p.size()!=4) throw std::invalid_argument("axis requires origin,extent,samples,context");
    return {integer<std::int64_t>(p[0]),integer<std::int64_t>(p[1]),integer<std::int64_t>(p[2]),integer<std::int64_t>(p[3])};
}
struct Options {
    std::optional<ScientificGridAxis> x,y;
    ScientificRawOptions selection;RawPixelFeatureOptions features;RawPixelTrainingOptions sampling;
    RawPixelTileOptions tiles;ForestParameters forest;RawPixelStreamOptions stream;
    RawPixelStreamSaveOptions save;RawPixelTrainingCheckpointOptions checkpoint;
    std::vector<std::uint64_t> ids;std::uint64_t object=0;
    std::optional<RawPixelStreamRegion> region;
    std::string checkpoint_path,archive_path,output_project;
    bool physical=false;
};
Options parse(const std::vector<std::string>& args,std::size_t first,bool train,bool verify) {
    Options o;std::set<std::string> seen;
    for(std::size_t i=first;i<args.size();++i) {
        const auto& f=args[i];if(f!="--object" && !seen.insert(f).second) throw std::invalid_argument("duplicate option: "+f);
        auto value=[&]()->const std::string& {if(++i==args.size()) throw std::invalid_argument("missing value for "+f);return args[i];};
        if(f=="--work-bytes") {auto n=integer<std::size_t>(value());o.selection.work_bytes=o.features.work_bytes=o.sampling.work_bytes=o.stream.work_bytes=n;}
        else if(f=="--file-work-bytes" && !train) o.save.limits.work_bytes=integer<std::size_t>(value());
        else if(f=="--file-bytes") {if(train) throw std::invalid_argument("--file-bytes applies to prediction files");o.save.limits.max_bytes=integer<std::uint64_t>(value());}
        else if(verify) throw std::invalid_argument("unknown raw verification option: "+f);
        else if(f=="--x-axis") o.x=axis(value());
        else if(f=="--y-axis") o.y=axis(value());
        else if(f=="--tile-width") o.tiles.width=o.stream.tile_width=integer<std::int64_t>(value());
        else if(f=="--tile-height") o.tiles.height=o.stream.tile_height=integer<std::int64_t>(value());
        else if(f=="--max-windows") {auto n=integer<std::size_t>(value());o.tiles.max_windows=n;o.stream.max_windows=n;}
        else if(f=="--max-new-windows") o.save.max_new_windows=o.checkpoint.max_new_windows=integer<std::uint64_t>(value());
        else if(f=="--resume") o.save.resume_path=o.checkpoint.resume_path=value();
        else if(f=="--replace-existing") o.save.replace_existing=o.checkpoint.replace_existing=true;
        else if(f=="--output-project") o.output_project=value();
        else if(f=="--object") {auto n=integer<std::uint64_t>(value());if(!n) throw std::invalid_argument("object ID must be positive");if(train) o.ids.push_back(n);else {if(o.object) throw std::invalid_argument("prediction accepts one --object");o.object=n;}}
        else if(!train && f=="--min-vote") o.stream.min_vote_fraction=number(value());
        else if(!train && f=="--max-samples") o.stream.max_samples=integer<std::uint64_t>(value());
        else if(!train && f=="--region") {auto p=parts(value());if(p.size()!=4) throw std::invalid_argument("region requires first_x,first_y,width,height in sample indices");o.region=RawPixelStreamRegion{integer<std::int64_t>(p[0]),integer<std::int64_t>(p[1]),integer<std::int64_t>(p[2]),integer<std::int64_t>(p[3])};}
        else if(!train) throw std::invalid_argument("unknown raw prediction option: "+f);
        else if(f=="--checkpoint") o.checkpoint_path=value();
        else if(f=="--archive") o.archive_path=value();
        else if(f=="--first-channel") o.selection.first_channel=integer<std::uint32_t>(value());
        else if(f=="--channels") o.selection.channels=integer<std::uint32_t>(value());
        else if(f=="--z") o.selection.z=integer<std::uint32_t>(value());
        else if(f=="--time") o.selection.t=integer<std::uint32_t>(value());
        else if(f=="--offsets") o.features.offsets=numbers(value());
        else if(f=="--scales") o.features.scales=numbers(value());
        else if(f=="--sigmas") {auto n=numbers(value());if(n.size()%2) throw std::invalid_argument("sigmas require x,y pairs");o.features.sigmas.clear();for(std::size_t j=0;j<n.size();j+=2) o.features.sigmas.push_back({n[j],n[j+1]});}
        else if(f=="--gradient") o.features.gradient_magnitude=true;
        else if(f=="--laplacian") o.features.laplacian=true;
        else if(f=="--physical-sigmas") o.physical=true;
        else if(f=="--max-samples") o.sampling.max_samples=integer<std::size_t>(value());
        else if(f=="--samples-per-class") o.sampling.max_samples_per_class=integer<std::size_t>(value());
        else if(f=="--max-scanned-samples") o.sampling.max_scanned_samples=integer<std::uint64_t>(value());
        else if(f=="--unbalanced") o.sampling.balanced=false;
        else if(f=="--seed") o.sampling.seed=o.forest.seed=integer<std::uint64_t>(value());
        else if(f=="--trees") o.forest.trees=integer<std::uint32_t>(value());
        else if(f=="--max-depth") o.forest.max_depth=integer<std::uint32_t>(value());
        else if(f=="--min-samples") o.forest.min_samples=integer<std::uint32_t>(value());
        else if(f=="--active-features") o.forest.active_features=integer<std::uint32_t>(value());
        else if(f=="--forest-work-bytes") o.forest.work_bytes=integer<std::uint64_t>(value());
        else if(f=="--checkpoint-work-bytes") o.checkpoint.limits.work_bytes=integer<std::size_t>(value());
        else if(f=="--checkpoint-bytes") o.checkpoint.limits.max_bytes=integer<std::size_t>(value());
        else throw std::invalid_argument("unknown raw training option: "+f);
    }
    if(!o.output_project.empty()) {
        std::error_code error;const auto exists=std::filesystem::exists(std::filesystem::u8path(export_path(o.output_project)),error);
        if(error || exists) throw std::invalid_argument("output project must be a new file");
    }
    if(bool(o.x)!=bool(o.y)) throw std::invalid_argument("provide both --x-axis and --y-axis");
    if(train) {o.selection.validate();if(o.features.offsets.empty()) o.features.offsets.assign(o.selection.channels,0);if(o.features.scales.empty()) o.features.scales.assign(o.selection.channels,1);o.features.validate(o.selection.channels);o.sampling.validate();o.tiles.validate();o.forest.validate();o.checkpoint.limits.validate();}
    else {o.save.limits.validate();if(!verify) o.stream.validate();}
    return o;
}
ScientificRawLattice lattice(const Document& doc,const Options& o) {
    const auto& m=doc.metadata();return {m.width,m.height,o.x?*o.x:ScientificGridAxis(0,m.width,m.width),o.y?*o.y:ScientificGridAxis(0,m.height,m.height)};
}
// Inputs and outputs are different trust boundaries from the worker's raw-source
// guard: protect the project, its sidecars, every reader file and other products.
void guard_outputs(const Project& project,const std::vector<std::string>& outputs,const std::vector<std::string>& inputs) {
    for(std::size_t i=0;i<outputs.size();++i) {
        const auto out=export_path(outputs[i]);
        for(const auto& in:inputs) guard_file_output(export_path(in),"",out);
        for(const auto& suffix:{"","-journal","-wal","-shm",".qpx-lock"}) guard_file_output(export_path(project.path()+suffix),"",out);
        for(std::size_t j=0;j<i;++j) guard_file_output(export_path(outputs[j]),"",out);
        for(const auto& image:project.images()) {
            if(!image.source_path.empty()) guard_file_output(export_path(image.source_path),"",out);
            const auto source=project.document(image.id)->server()->reader_source();if(!source) continue;
            if(source->primary) guard_file_output(source->primary->path,source->primary->identity,out);
            for(const auto& file:source->companions) guard_file_output(file.path,file.identity,out);
        }
    }
}
std::string summary_json(const RawPixelStreamFileSummary& s) {
    std::string out="{\"bytes\":"+std::to_string(s.bytes)+",\"total_windows\":"+std::to_string(s.total_windows)+",\"completed_windows\":"+std::to_string(s.completed_windows)+",\"reused_windows\":"+std::to_string(s.reused_windows)+",\"valid_samples\":"+std::to_string(s.valid_samples)+",\"classified_samples\":"+std::to_string(s.classified_samples)+",\"complete\":"+(s.complete?"true":"false")+",\"input_sha256\":"+batch_json_quote(s.input_sha256)+",\"sha256\":"+batch_json_quote(s.sha256)+",\"content_sha256\":"+batch_json_quote(s.content_sha256)+",\"class_counts\":[";
    for(std::size_t i=0;i<s.class_counts.size();++i) {if(i) out+=',';out+=std::to_string(s.class_counts[i]);}return out+"]}";
}
void line(const std::string& verb,const std::string& state,const std::string& path,const RawPixelFileJobStats& stats,const std::string& fields="") {
    std::cout<<"{\"schema\":\"qpx.raw-pixel-"<<verb<<"/1\",\"state\":"<<batch_json_quote(state)<<",\"path\":"<<batch_json_quote(path)<<",\"error\":"<<batch_json_quote(stats.error)<<",\"source_tree_sha256\":"<<batch_json_quote(build_environment()->source_tree_sha256)<<",\"total_windows\":"<<stats.total_windows<<",\"completed_windows\":"<<stats.completed_windows<<fields<<"}\n"<<std::flush;
}
template<class Job,class Poll> RawPixelFileJobStats collect(Job& job,Poll poll,const std::atomic<bool>& cancelled) {
    while(job.busy()) {if(cancelled.load()) job.cancel();poll();if(job.busy()) std::this_thread::sleep_for(std::chrono::milliseconds(5));}
    if(cancelled.load()) job.cancel();return poll();
}
int verify(const std::vector<std::string>& args,const std::atomic<bool>& cancelled) {
    if(args.size()<3) throw std::invalid_argument("usage: --verify-raw-pixels FILE [--file-bytes N --work-bytes N]");
    const auto o=parse(args,3,false,true);RawPixelStreamFileReadJob job;job.open(args[2],o.save.limits);
    auto stats=collect(job,[&] {return job.poll();},cancelled);const auto file=job.take_file();
    line("verify",file?"verified":stats.state,args[2],stats,file?",\"summary\":"+summary_json(file->declared_summary()):"");
    return file?0:stats.state=="cancelled"?130:1;
}
int predict(const std::vector<std::string>& args,const std::atomic<bool>& cancelled) {
    if(args.size()<6) throw std::invalid_argument("usage: --export-raw-pixels PROJECT IMAGE_ID MODEL OUTPUT [options]");
    const auto id=integer<std::uint64_t>(args[3]);if(!id) throw std::invalid_argument("image ID must be positive");const auto o=parse(args,6,false,false);
    export_checkpoint(&cancelled);auto model=load_raw_pixel_classifier(args[4],&cancelled);auto project=Project::open(args[2]);auto doc=project->document(id);
    std::vector<std::string> outputs{args[5]};if(!o.output_project.empty()) outputs.push_back(o.output_project);guard_outputs(*project,outputs,{args[4]});
    if(!o.save.resume_path.empty()) for(std::size_t i=1;i<outputs.size();++i) guard_file_output(export_path(o.save.resume_path),"",export_path(outputs[i]));
    RawPixelTiledPredictionRequest request(*doc,model,lattice(*doc,o),o.stream,o.region,o.object);
    RawPixelStreamSaveJob job;job.request(request,args[5],o.save);line("export","running",args[5],job.poll(*doc),",\"model_sha256\":"+batch_json_quote(model.sha256()));
    auto stats=collect(job,[&] {return job.poll(*doc);},cancelled);auto summary=job.take(*doc);
    if(summary && !o.output_project.empty()) {
        RawPixelStreamFile file(args[5],o.save.limits);file.verify(&cancelled);auto link=file.link();doc->keep_raw_pixel_stream(link);
        guard_outputs(*project,outputs,{args[4]});project->save_new(o.output_project,512ull*1024*1024,1ull<<30,&cancelled);
    }
    const auto state=summary?(summary->complete?"saved":"partial"):stats.state;
    line("export",state,args[5],stats,",\"model_sha256\":"+batch_json_quote(model.sha256())+(summary?",\"summary\":"+summary_json(*summary):""));
    return summary?(summary->complete?0:1):stats.state=="cancelled"?130:1;
}
int train(const std::vector<std::string>& args,const std::atomic<bool>& cancelled) {
    if(args.size()<5) throw std::invalid_argument("usage: --train-raw-pixels PROJECT IMAGE_ID MODEL --checkpoint FILE --archive FILE [options]");
    const auto id=integer<std::uint64_t>(args[3]);if(!id) throw std::invalid_argument("image ID must be positive");auto o=parse(args,5,true,false);
    if(o.checkpoint_path.empty() || o.archive_path.empty()) throw std::invalid_argument("training requires --checkpoint and --archive");
    if(!has_classifier_training()) throw std::runtime_error("raw training requires the optional OpenCV backend");export_checkpoint(&cancelled);
    auto project=Project::open(args[2]);auto doc=project->document(id);
    std::vector<std::string> outputs{args[4],o.checkpoint_path,o.archive_path};if(!o.output_project.empty()) outputs.push_back(o.output_project);guard_outputs(*project,outputs,{});
    if(!o.checkpoint.resume_path.empty()) for(std::size_t i=0;i<outputs.size();++i) if(i!=1) guard_file_output(export_path(o.checkpoint.resume_path),"",export_path(outputs[i]));
    if(o.ids.empty()) for(const auto& a:doc->annotations()) if(!a.class_name.empty() && a.geometry.is_area()) o.ids.push_back(a.id);
    RawPixelTiledTrainingRequest request(*doc,o.ids,lattice(*doc,o),o.selection,o.features,o.sampling,o.tiles,o.physical);
    RawPixelTrainingCheckpointJob job;job.request(request,o.checkpoint_path,o.checkpoint);line("train","sampling",o.checkpoint_path,job.poll(*doc));
    auto stats=collect(job,[&] {return job.poll(*doc);},cancelled);auto result=job.take(*doc);
    if(!result) {line("train",stats.state,o.checkpoint_path,stats);return stats.state=="cancelled"?130:1;}
    const auto fields=",\"request_sha256\":"+batch_json_quote(result->checkpoint->request_sha256())+",\"prefix_sha256\":"+batch_json_quote(result->checkpoint->prefix_sha256())+",\"replayed_windows\":"+std::to_string(result->replayed_windows);
    if(!result->table) {line("train","partial",o.checkpoint_path,stats,fields+",\"model_sha256\":null");return 1;}
    const auto model=train_raw_pixel_tiled_classifier(*result->table,o.forest,&cancelled);
    auto archive=archive_raw_pixel_tiled_training(result->table,std::make_shared<const RawPixelClassifier>(model),&cancelled);
    guard_outputs(*project,outputs,{});save_raw_pixel_classifier(model,args[4],o.checkpoint.replace_existing,&cancelled);save_raw_pixel_archive(*archive,o.archive_path,o.checkpoint.replace_existing,&cancelled);
    if(!o.output_project.empty()) {doc->keep_raw_pixel_tiled_training(request,archive);project->save_new(o.output_project,512ull*1024*1024,1ull<<30,&cancelled);}
    line("train","trained",args[4],stats,fields+",\"rows\":"+std::to_string(result->table->row_count())+",\"table_sha256\":"+batch_json_quote(result->table->sha256())+",\"model_sha256\":"+batch_json_quote(model.sha256())+",\"archive_path\":"+batch_json_quote(o.archive_path));return 0;
}
}
std::optional<int> raw_pixel_cli(const std::vector<std::string>& args,const std::atomic<bool>& cancelled) {
    if(args.size()<2) return {};
    const auto& mode=args[1];if(mode!="--train-raw-pixels" && mode!="--export-raw-pixels" && mode!="--verify-raw-pixels") return {};
    try {
        if(mode=="--train-raw-pixels") return train(args,cancelled);
        if(mode=="--export-raw-pixels") return predict(args,cancelled);
        return verify(args,cancelled);
    } catch(const std::invalid_argument&) {throw;}
    catch(const std::exception& e) {
        RawPixelFileJobStats stats;stats.error=e.what();const auto state=cancelled.load()?"cancelled":"failed";
        line(mode=="--train-raw-pixels"?"train":mode=="--export-raw-pixels"?"export":"verify",state,"",stats);return cancelled.load()?130:1;
    }
}
}
