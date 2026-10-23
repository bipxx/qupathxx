#include "qupathxx/project_batch.hpp"
#include "batch_checkpoint.hpp"
#include "pixel_stream_cli.hpp"
#include <charconv>
#include <cmath>
#include <csignal>
#include <filesystem>
#include <iostream>
#ifdef _WIN32
#define NOMINMAX
#include <windows.h>
#endif
namespace {
std::atomic<bool> cancelled{false};
void interrupt(int) {cancelled.store(true);}
#ifdef _WIN32
BOOL WINAPI console_interrupt(DWORD event) {if(event==CTRL_C_EVENT || event==CTRL_BREAK_EVENT) {cancelled.store(true);return TRUE;}return FALSE;}
#endif
std::uint64_t integer(const std::string& s) {
    std::uint64_t n=0;const auto r=std::from_chars(s.data(),s.data()+s.size(),n);
    if(s.empty() || r.ec!=std::errc{} || r.ptr!=s.data()+s.size()) throw std::invalid_argument("expected unsigned integer: "+s);return n;
}
double real(const std::string& s) {std::size_t end=0;const auto n=std::stod(s,&end);if(end!=s.size() || !std::isfinite(n)) throw std::invalid_argument("expected finite number: "+s);return n;}
std::vector<double> numbers(const std::string& s) {
    std::vector<double> out;std::size_t at=0;do {const auto end=s.find(',',at);out.push_back(real(s.substr(at,end==std::string::npos?end:end-at)));if(out.size()>9) throw std::invalid_argument("too many vector components");if(end==std::string::npos) break;at=end+1;}while(true);return out;
}
int execute(const std::vector<std::string>& args) {
    using namespace qpx;
    if(args.size()==1 || (args.size()==2 && args[1]=="--help")) {
        std::cout<<"Usage: qupathxx_batch PROJECT OUTPUT_DIRECTORY [options]\n"
            "       qupathxx_batch --export-csv CHECKPOINT OUTPUT.csv\n"
            "       qupathxx_batch --export-pixels PROJECT IMAGE_ID OBJECT_ID MODEL OUTPUT.qps [options]\n"
            "       qupathxx_batch --verify-pixels FILE.qps [--file-bytes N]\n"
            "Pixel export: --block-size N --work-bytes N --tile-size N --file-bytes N\n"
            "              --max-pixels N --max-blocks N --min-vote NUMBER\n"
            "              --window X,Y,WIDTH,HEIGHT --replace-existing\n"
            "--kind od|stains|tissue|cells|pixels  --image ID (repeatable)\n"
            "--object ID (repeatable, requires one --image)\n"
            "Pixel batches: --pixel-model MODEL --block-size N --pixel-work-bytes N\n"
            "               --pixel-file-bytes N --pixel-total-bytes N --min-vote NUMBER\n"
            "               --max-pixels N --max-blocks N (per target)\n"
            "--white R,G,B  --stains R,G,B,R,G,B[,R,G,B]  --normalized\n"
            "--threshold NUMBER  --level INTEGER  --tile-size INTEGER\n"
            "--continue-on-error  --no-resume  --publish-cells  --nucleus\n"
            "--snapshot-bytes N  --materialize-bytes N  --result-bytes N\n"
            "--csv-bytes N  --checkpoint-bytes N\n"
            "Default model: H&E basis (0.65,0.70,0.29), (0.07,0.99,0.11).\n"
            "JSONL stdout; exit 0 success, 1 failure/partial, 2 arguments, 130 cancelled.\n";
        return args.size()==1?2:0;
    }
    try {
        if(const auto result=detail::pixel_stream_cli(args,cancelled)) return *result;
        if(args.size()==4 && args[1]=="--export-csv") {save_batch_checkpoint_csv(args[2],args[3]);return 0;}
        if(args.size()<3 || args[1].rfind("--",0)==0) throw std::invalid_argument("expected PROJECT and OUTPUT_DIRECTORY; see --help");
        ProjectBatchOptions options;std::vector<ProjectBatchSelection> selection;std::vector<std::uint64_t> objects;
        std::optional<std::size_t> requested_tile_size;std::string pixel_model_path;ColorVector white{255,255,255};std::vector<ColorVector> stains{{.65,.70,.29},{.07,.99,.11}};bool normalized=false;
        for(std::size_t i=3;i<args.size();++i) {
            const auto& flag=args[i];auto value=[&]()->const std::string& {if(++i==args.size()) throw std::invalid_argument("missing value for "+flag);return args[i];};
            if(flag=="--kind") {const auto& v=value();if(v=="od" || v=="stains") {options.kind=ProjectBatchKind::Roi;options.roi_kind=v=="od"?AnalysisKind::OpticalDensityMeans:AnalysisKind::StainMeans;}else if(v=="tissue") options.kind=ProjectBatchKind::Tissue;else if(v=="cells") options.kind=ProjectBatchKind::Cells;else if(v=="pixels") options.kind=ProjectBatchKind::Pixels;else throw std::invalid_argument("unknown batch kind");}
            else if(flag=="--pixel-model") pixel_model_path=value();
            else if(flag=="--block-size") options.pixels.block_size=static_cast<std::size_t>(integer(value()));
            else if(flag=="--pixel-work-bytes") options.pixels.prediction.work_bytes=static_cast<std::size_t>(integer(value()));
            else if(flag=="--pixel-file-bytes") options.pixel_file_limit=integer(value());
            else if(flag=="--pixel-total-bytes") options.pixel_total_limit=integer(value());
            else if(flag=="--max-pixels") options.pixels.max_pixels=integer(value());
            else if(flag=="--max-blocks") options.pixels.max_blocks=integer(value());
            else if(flag=="--min-vote") options.pixels.prediction.min_vote_fraction=real(value());
            else if(flag=="--image") selection.push_back({integer(value()),{}});
            else if(flag=="--object") objects.push_back(integer(value()));
            else if(flag=="--white") {const auto v=numbers(value());if(v.size()!=3) throw std::invalid_argument("white requires three components");std::copy(v.begin(),v.end(),white.begin());}
            else if(flag=="--stains") {const auto v=numbers(value());if(v.size()!=6 && v.size()!=9) throw std::invalid_argument("stains require six or nine components");stains.clear();for(std::size_t n=0;n<v.size();n+=3) stains.push_back({v[n],v[n+1],v[n+2]});}
            else if(flag=="--normalized") normalized=true;
            else if(flag=="--threshold") options.cells.threshold=options.tissue.threshold=real(value());
            else if(flag=="--level") {const auto& v=value();if(v=="-1") options.cells.level=options.tissue.level=-1;else {auto n=integer(v);if(n>INT32_MAX) throw std::invalid_argument("level exceeds int32");options.cells.level=options.tissue.level=static_cast<std::int32_t>(n);}}
            else if(flag=="--tile-size") requested_tile_size=static_cast<std::size_t>(integer(value()));
            else if(flag=="--snapshot-bytes") options.snapshot_budget=static_cast<std::size_t>(integer(value()));
            else if(flag=="--materialize-bytes") options.materialize_budget=static_cast<std::size_t>(integer(value()));
            else if(flag=="--result-bytes") options.result_budget=static_cast<std::size_t>(integer(value()));
            else if(flag=="--csv-bytes") options.csv_limit=static_cast<std::size_t>(integer(value()));
            else if(flag=="--checkpoint-bytes") options.checkpoint_limit=integer(value());
            else if(flag=="--continue-on-error") options.failure=ProjectBatchFailure::Continue;
            else if(flag=="--no-resume") options.resume=false;
            else if(flag=="--publish-cells") options.publish_cells=true;
            else if(flag=="--nucleus") options.nucleus=true;
            else throw std::invalid_argument("unknown option: "+flag);
        }
        if(!objects.empty()) {if(selection.size()!=1) throw std::invalid_argument("--object requires exactly one --image");selection[0].object_ids=objects;}
        if(normalized && stains.size()!=3) throw std::invalid_argument("--normalized requires a complete nine-component basis");
        if(requested_tile_size) {if(options.kind==ProjectBatchKind::Pixels) options.pixels.prediction.tile_size=*requested_tile_size;else options.roi_tile_size=options.cells.tile_size=options.tissue.tile_size=*requested_tile_size;}
        if(!pixel_model_path.empty()) {if(options.kind!=ProjectBatchKind::Pixels) throw std::invalid_argument("--pixel-model requires --kind pixels");options.pixel_model=load_pixel_classifier(pixel_model_path,&cancelled);}
        const auto transform=normalized?StainTransform::from_normalized({stains[0],stains[1],stains[2]},white):StainTransform(stains,white);options.validate();
        auto project=Project::open(args[1]);ProjectBatchRequest request(*project,transform,selection,options);project.reset();
        const auto report=run_project_batch(request,args[2],&cancelled,[](const auto& image){std::cout<<project_batch_image_json(image)<<'\n'<<std::flush;});
        std::cout<<project_batch_report_json(report)<<std::flush;return report.state=="succeeded"?0:report.state=="cancelled"?130:1;
    } catch(const std::exception& e) {
        std::cerr<<"{\"schema\":\"qpx.batch-error/1\",\"error\":"<<qpx::detail::batch_json_quote(e.what())<<"}\n";return cancelled.load()?130:2;
    }
}
}
#ifdef _WIN32
int wmain(int argc,wchar_t** argv) {
    SetConsoleCtrlHandler(console_interrupt,TRUE);std::vector<std::string> args;for(int i=0;i<argc;++i) args.push_back(std::filesystem::path(argv[i]).u8string());return execute(args);
}
#else
int main(int argc,char** argv) {std::signal(SIGINT,interrupt);std::signal(SIGTERM,interrupt);return execute({argv,argv+argc});}
#endif
