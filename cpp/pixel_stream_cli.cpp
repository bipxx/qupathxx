#include "pixel_stream_cli.hpp"
#include "qupathxx/pixel_stream_file.hpp"
#include "qupathxx/project.hpp"
#include "batch_checkpoint.hpp"
#include <charconv>
#include <chrono>
#include <cmath>
#include <iostream>
#include <iomanip>
#include <locale>
#include <sstream>
#include <thread>
namespace qpx::detail {
namespace {
std::uint64_t integer(const std::string& text) {
    std::uint64_t n=0;const auto parsed=std::from_chars(text.data(),text.data()+text.size(),n);
    if(text.empty() || parsed.ec!=std::errc{} || parsed.ptr!=text.data()+text.size()) throw std::invalid_argument("expected unsigned integer: "+text);return n;
}
double number(const std::string& text) {
    std::istringstream input(text);input.imbue(std::locale::classic());double n;
    if(!(input>>n) || !std::isfinite(n)) throw std::invalid_argument("expected finite number: "+text);input>>std::ws;if(!input.eof()) throw std::invalid_argument("expected finite number: "+text);return n;
}
Rect window(const std::string& text) {
    std::vector<double> values;std::size_t at=0;
    do {const auto end=text.find(',',at);values.push_back(number(text.substr(at,end==std::string::npos?end:end-at)));if(values.size()>4) throw std::invalid_argument("window requires x,y,width,height");if(end==std::string::npos) break;at=end+1;}while(true);
    if(values.size()!=4) throw std::invalid_argument("window requires x,y,width,height");return {values[0],values[1],values[2],values[3]};
}
std::string summary_json(const PixelStreamFileSummary& s) {
    std::string out="{\"bytes\":"+std::to_string(s.bytes)+",\"blocks\":"+std::to_string(s.blocks)+",\"valid_pixels\":"+std::to_string(s.valid_pixels)+",\"classified_pixels\":"+std::to_string(s.classified_pixels)+",\"content_sha256\":"+batch_json_quote(s.sha256)+",\"class_counts\":[";
    bool first=true;for(auto count:s.class_counts) {if(!first) out+=',';first=false;out+=std::to_string(count);}return out+"]}";
}
void export_line(const PixelStreamExportStats& s,const std::string& path,std::uint64_t image,std::uint64_t object,const std::string& model) {
    std::ostringstream out;out.imbue(std::locale::classic());out<<std::setprecision(17);
    out<<"{\"schema\":\"qpx.pixel-export/1\",\"state\":"<<batch_json_quote(s.state)<<",\"error\":"<<batch_json_quote(s.error)<<",\"path\":"<<batch_json_quote(path)<<",\"image_id\":"<<image<<",\"object_id\":"<<object<<",\"model_sha256\":"<<batch_json_quote(model)<<",\"source_tree_sha256\":"<<batch_json_quote(build_environment()->source_tree_sha256)<<",\"completed_blocks\":"<<s.completed_blocks<<",\"total_blocks\":"<<s.total_blocks<<",\"estimated_work_bytes\":"<<s.estimated_work_bytes<<",\"elapsed_ms\":"<<s.elapsed_ms<<",\"summary\":"<<(s.summary?summary_json(*s.summary):"null")<<"}\n";
    std::cout<<out.str()<<std::flush;
}
int verify_file(const std::vector<std::string>& args,const std::atomic<bool>& cancelled) {
    if(args.size()!=3 && args.size()!=5) throw std::invalid_argument("usage: --verify-pixels FILE [--file-bytes N]");
    std::uint64_t limit=1ULL<<40;if(args.size()==5) {if(args[3]!="--file-bytes") throw std::invalid_argument("unknown verification option: "+args[3]);limit=integer(args[4]);}
    PixelStreamReadJob job;job.open(args[2],0,limit);
    auto collect=[&] {while(job.busy()) {if(cancelled.load()) job.cancel();job.poll();if(job.busy()) std::this_thread::sleep_for(std::chrono::milliseconds(5));}return job.poll();};
    auto stats=collect();if(stats.state=="ready" && !cancelled.load()) {job.verify();stats=collect();}
    else if(stats.state=="ready") {job.cancel();stats=job.poll();}
    const auto state=stats.state=="ready" && stats.verified?"verified":stats.state;
    std::cout<<"{\"schema\":\"qpx.pixel-verify/1\",\"state\":"<<batch_json_quote(state)<<",\"error\":"<<batch_json_quote(stats.error)<<",\"path\":"<<batch_json_quote(args[2])<<",\"source_tree_sha256\":"<<batch_json_quote(build_environment()->source_tree_sha256)<<",\"summary\":"<<(stats.verified && stats.declared_summary?summary_json(*stats.declared_summary):"null")<<"}\n"<<std::flush;
    return state=="verified"?0:state=="cancelled"?130:1;
}
}
std::optional<int> pixel_stream_cli(const std::vector<std::string>& args,const std::atomic<bool>& cancelled) {
    if(args.size()<2) return {};
    if(args[1]=="--verify-pixels") return verify_file(args,cancelled);
    if(args[1]!="--export-pixels") return {};
    if(args.size()<7) throw std::invalid_argument("usage: --export-pixels PROJECT IMAGE_ID OBJECT_ID MODEL OUTPUT [options]");
    const auto image=integer(args[3]),object=integer(args[4]);if(!image || !object) throw std::invalid_argument("image and object IDs must be positive");
    PixelStreamOptions options;std::uint64_t limit=1ULL<<40;bool replace=false;std::optional<Rect> crop;
    for(std::size_t i=7;i<args.size();++i) {
        const auto& flag=args[i];auto value=[&]()->const std::string& {if(++i==args.size()) throw std::invalid_argument("missing value for "+flag);return args[i];};
        if(flag=="--block-size") options.block_size=static_cast<std::size_t>(integer(value()));
        else if(flag=="--work-bytes") options.prediction.work_bytes=static_cast<std::size_t>(integer(value()));
        else if(flag=="--tile-size") options.prediction.tile_size=static_cast<std::size_t>(integer(value()));
        else if(flag=="--file-bytes") limit=integer(value());
        else if(flag=="--max-pixels") options.max_pixels=integer(value());
        else if(flag=="--max-blocks") options.max_blocks=integer(value());
        else if(flag=="--min-vote") options.prediction.min_vote_fraction=number(value());
        else if(flag=="--window") crop=window(value());
        else if(flag=="--replace-existing") {if(replace) throw std::invalid_argument("duplicate replacement flag");replace=true;}
        else throw std::invalid_argument("unknown pixel export option: "+flag);
    }
    options.validate();if(limit<128 || limit>(1ULL<<46)) throw std::invalid_argument("invalid pixel stream file byte limit");
    if(cancelled.load()) return 130;
    const auto model=load_pixel_classifier(args[5],&cancelled);auto project=Project::open(args[2]);const auto doc=project->document(image);
    PixelStreamExportJob job;job.request(*doc,object,model,args[6],options,crop,replace,limit);
    auto stats=job.poll(*doc);if(job.busy()) export_line(stats,args[6],image,object,model.sha256());auto emitted=std::chrono::steady_clock::now();auto blocks=stats.completed_blocks;
    while(job.busy()) {
        if(cancelled.load()) job.cancel();stats=job.poll(*doc);const auto now=std::chrono::steady_clock::now();
        if(job.busy() && stats.completed_blocks!=blocks && now-emitted>=std::chrono::milliseconds(100)) {export_line(stats,args[6],image,object,model.sha256());emitted=now;blocks=stats.completed_blocks;}
        if(job.busy()) std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
    stats=job.poll(*doc);export_line(stats,args[6],image,object,model.sha256());return stats.state=="saved"?0:stats.state=="cancelled"?130:1;
}
}
