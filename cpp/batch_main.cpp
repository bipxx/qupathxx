#include "qupathxx/project_batch.hpp"
#include "batch_checkpoint.hpp"
#include "pixel_stream_cli.hpp"
#include "raw_pixel_cli.hpp"
#include "cell_table_cli.hpp"
#include "tma_case_cli.hpp"
#include "qptma_project_cli.hpp"
#include "tma_dearray_cli.hpp"
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
            "       qupathxx_batch --export-cell-table PROJECT OUTPUT.csv [--capture-calibration] [--published] [--require-current]\n"
            "       qupathxx_batch --export-tma-cases PROJECT OUTPUT.csv [--image ID]\n"
            "       qupathxx_batch --export-tma-cohort PROJECT OUTPUT.csv (--image ID ... | --all-tma-images)\n"
            "       qupathxx_batch --export-pixels PROJECT IMAGE_ID OBJECT_ID MODEL OUTPUT.qps [options]\n"
            "       qupathxx_batch --verify-pixels FILE.qps [--file-bytes N]\n"
            "Pixel export: --block-size N --work-bytes N --tile-size N --file-bytes N\n"
            "              --max-pixels N --max-blocks N --min-vote NUMBER\n"
            "              --window X,Y,WIDTH,HEIGHT --replace-existing\n"
            "TMA case export: --measurements INPUT.csv --value-column NAME\n"
            "                 --weight-column NAME --allow-unversioned --csv-bytes N\n"
            "Raw options: --raw-level N --raw-channel N --raw-channels N (zero-based axes)\n"
            "             --raw-z N --raw-time N --raw-explicit-axes --raw-max-pixels N\n"
            "Raw grids: --raw-grid-samples NX,NY [--raw-grid-context CX,CY --raw-grid-work-bytes N] (no level/tile flags)\n"
            "--kind od|stains|tissue|cells|pixels|raw|raw-pixels  --image ID (repeatable)\n"
            "Cells: --cell-algorithm distance|log|log-bodies|log-sampled --cell-log-prominence N --cell-context-padding N\n"
            "Grid cells: --cell-algorithm grid --cell-grid-samples NX,NY --cell-grid-context CX,CY\n"
            "            --grid-units px|um --grid-feature od|stain1|stain2|stain3\n"
            "            --grid-sigma N --grid-prominence N --grid-min-radius N --grid-seed-spacing N\n"
            "            --grid-expansion N --grid-min-area N --grid-max-area N\n"
            "            --grid-work-bytes N --grid-max-cells N --grid-contour-bytes N\n"
            "            --grid-sampling coverage|winding (distances/areas use grid units)\n"
            "--object ID (repeatable, requires one --image)\n"
            "Pixel batches: --pixel-model MODEL --block-size N --pixel-work-bytes N\n"
            "               --pixel-file-bytes N --pixel-total-bytes N --min-vote NUMBER\n"
            "               --max-pixels N --max-blocks N (per target)\n"
            "--white R,G,B  --stains R,G,B,R,G,B[,R,G,B]  --normalized\n"
            "--threshold NUMBER  --level INTEGER  --tile-size INTEGER\n"
            "Cell area: --cell-min-area-um2 NUMBER --cell-max-area-um2 NUMBER (inclusive)\n"
            "--roi-sampling coverage|winding  --tissue-sampling coverage|winding\n"
            "--continue-on-error  --no-resume  --publish-cells  --nucleus\n"
            "--tma-only  --include-missing-tma (TMA scope only)\n"
            "--snapshot-bytes N  --materialize-bytes N  --result-bytes N\n"
            "--csv-bytes N  --checkpoint-bytes N\n"
            "Default model: H&E basis (0.65,0.70,0.29), (0.07,0.99,0.11).\n"
            "--import-qptma PROJECT GRID.qptma OUTPUT.qpxproj --image ID [--qptma-bytes N --snapshot-bytes N --project-bytes N]\n"
            "--export-qptma PROJECT OUTPUT.qptma --image ID [--qptma-bytes N]\n"
            "--detect-tma PROJECT OUTPUT.qptma --image ID --rows N --columns N --core-width SIZE --core-height SIZE\n"
            "TMA preview: --core-units px|um (default px) --polarity dark|light --missing-density N --bounds-scale N --min-valid-fraction N\n"
            "             --preview-width N --preview-height N --source-pixels N --output-pixels N --work-bytes N\n"
            "             --min-component-area N --max-component-area N --min-component-extent N\n"
            "             --max-fit-error N --max-rotation N --max-candidates N --pair-checks N --qptma-bytes N --name TEXT\n"
            "       qupathxx_batch --train-raw-pixels PROJECT IMAGE_ID MODEL --checkpoint FILE --archive FILE [raw options]\n"
            "       qupathxx_batch --export-raw-pixels PROJECT IMAGE_ID MODEL OUTPUT [raw options]\n"
            "       qupathxx_batch --verify-raw-pixels FILE [--file-bytes N --file-work-bytes N]\n"
            "Raw classifier batches: --kind raw-pixels --raw-pixel-model MODEL [--raw-pixel-x-axis O,E,N,C --raw-pixel-y-axis O,E,N,C]\n"
            "             --raw-pixel-tile-width N --raw-pixel-tile-height N --raw-pixel-work-bytes N\n"
            "             --raw-pixel-file-work-bytes N --raw-pixel-max-samples N --raw-pixel-max-windows N --raw-pixel-min-vote N\n"
            "             --pixel-file-bytes N --pixel-total-bytes N (shared RGB/raw output quotas)\n"
            "Raw options: --x-axis O,E,N,C --y-axis O,E,N,C (default unit level-zero lattice)\n"
            "             --tile-width N --tile-height N --work-bytes N --max-windows N --object ID\n"
            "             --resume FILE --max-new-windows N --replace-existing --output-project NEW_FILE\n"
            "Raw training: --channels N --first-channel N --z N --time N --offsets LIST --scales LIST\n"
            "              --sigmas X,Y,... --gradient --laplacian --physical-sigmas --max-samples N\n"
            "              --samples-per-class N --max-scanned-samples N --unbalanced --seed N\n"
            "              --trees N --max-depth N --min-samples N --active-features N --forest-work-bytes N --checkpoint-bytes N --checkpoint-work-bytes N\n"
            "Raw prediction: --region X,Y,W,H (sample indices) --min-vote N --max-samples N --file-bytes N --file-work-bytes N\n"
            "JSONL stdout; exit 0 success, 1 failure/partial, 2 arguments, 130 cancelled.\n";
        return args.size()==1?2:0;
    }
    try {
        if(const auto result=detail::raw_pixel_cli(args,cancelled)) return *result;
        if(const auto result=detail::pixel_stream_cli(args,cancelled)) return *result;
        if(const auto result=detail::cell_table_cli(args,cancelled)) return *result;
        if(const auto result=detail::tma_case_cli(args,cancelled)) return *result;
        if(const auto result=detail::qptma_project_cli(args,cancelled)) return *result;
        if(const auto result=detail::tma_dearray_cli(args,cancelled)) return *result;
        if(args.size()==4 && args[1]=="--export-csv") {save_batch_checkpoint_csv(args[2],args[3]);return 0;}
        if(args.size()<3 || args[1].rfind("--",0)==0) throw std::invalid_argument("expected PROJECT and OUTPUT_DIRECTORY; see --help");
        ProjectBatchOptions options;std::vector<ProjectBatchSelection> selection;std::vector<std::uint64_t> objects;
        bool legacy_cell_flags=false,legacy_algorithm=false,requested_raw_level=false,requested_raw_grid=false,raw_grid_flags=false,raw_pixel_flags=false;
        bool requested_level=false;std::optional<std::size_t> requested_tile_size;std::string pixel_model_path,raw_pixel_model_path;ColorVector white{255,255,255};std::vector<ColorVector> stains{{.65,.70,.29},{.07,.99,.11}};bool normalized=false;
        for(std::size_t i=3;i<args.size();++i) {
            const auto& flag=args[i];auto value=[&]()->const std::string& {if(++i==args.size()) throw std::invalid_argument("missing value for "+flag);return args[i];};
            if(flag.rfind("--raw-pixel-",0)==0) raw_pixel_flags=true;
            if(flag=="--kind") {const auto& v=value();if(v=="od" || v=="stains") {options.kind=ProjectBatchKind::Roi;options.roi_kind=v=="od"?AnalysisKind::OpticalDensityMeans:AnalysisKind::StainMeans;}else if(v=="tissue") options.kind=ProjectBatchKind::Tissue;else if(v=="cells") options.kind=ProjectBatchKind::Cells;else if(v=="pixels") options.kind=ProjectBatchKind::Pixels;else if(v=="raw") options.kind=ProjectBatchKind::Raw;else if(v=="raw-pixels") options.kind=ProjectBatchKind::RawPixels;else throw std::invalid_argument("unknown batch kind");}
            else if(flag=="--roi-sampling" || flag=="--tissue-sampling") {
                const auto& v=value();
                if(v!="coverage" && v!="winding") throw std::invalid_argument("sampling must be coverage or winding");
                const auto policy=v=="coverage"?RoiSamplingPolicy::GeometryCoverage:RoiSamplingPolicy::PolygonWinding;
                if(flag=="--roi-sampling") options.roi_sampling_policy=policy;else options.tissue.sampling_policy=policy;
            }
            else if(flag=="--raw-level" || flag=="--raw-channel" || flag=="--raw-channels" || flag=="--raw-z" || flag=="--raw-time") {
                const auto n=integer(value());if(n>65535 && !(flag=="--raw-channels" && n==65536)) throw std::invalid_argument("raw axis exceeds limit");
                if(flag=="--raw-level") {requested_raw_level=true;options.raw.region.level=static_cast<std::int32_t>(n);}
                else if(flag=="--raw-channel") options.raw.first_channel=static_cast<std::uint32_t>(n);
                else if(flag=="--raw-channels") options.raw.channels=static_cast<std::uint32_t>(n);
                else {options.raw_follow_display=false;if(flag=="--raw-z") options.raw.z=static_cast<std::uint32_t>(n);else options.raw.t=static_cast<std::uint32_t>(n);}
            }
            else if(flag=="--raw-grid-samples" || flag=="--raw-grid-context" || flag=="--raw-grid-work-bytes") {
                raw_grid_flags=true;if(!options.raw_grid) options.raw_grid=RawGridSampling{};
                if(flag=="--raw-grid-work-bytes") options.raw_grid_work_bytes=std::size_t(integer(value()));
                else {
                    const auto v=value();const auto comma=v.find(',');
                    if(comma==std::string::npos || v.find(',',comma+1)!=std::string::npos) throw std::invalid_argument("raw grid axes require two comma-separated integers");
                    const auto x=integer(v.substr(0,comma)),y=integer(v.substr(comma+1));const auto limit=flag=="--raw-grid-samples"?1000000u:4096u;
                    if(x>limit || y>limit) throw std::invalid_argument("raw grid axes exceed limits");
                    if(flag=="--raw-grid-samples") {requested_raw_grid=true;options.raw_grid->samples_x=std::uint32_t(x);options.raw_grid->samples_y=std::uint32_t(y);}
                    else {options.raw_grid->context_x=std::uint32_t(x);options.raw_grid->context_y=std::uint32_t(y);}
                }
            }
            else if(flag=="--raw-max-pixels") options.raw_max_pixels=integer(value());
            else if(flag=="--raw-explicit-axes") options.raw_follow_display=false;
            else if(flag=="--raw-pixel-model") raw_pixel_model_path=value();
            else if(flag=="--raw-pixel-x-axis") options.raw_pixel_x=detail::raw_pixel_cli_axis(value());
            else if(flag=="--raw-pixel-y-axis") options.raw_pixel_y=detail::raw_pixel_cli_axis(value());
            else if(flag=="--raw-pixel-tile-width") options.raw_pixels.tile_width=static_cast<std::int64_t>(integer(value()));
            else if(flag=="--raw-pixel-tile-height") options.raw_pixels.tile_height=static_cast<std::int64_t>(integer(value()));
            else if(flag=="--raw-pixel-work-bytes") options.raw_pixels.work_bytes=static_cast<std::size_t>(integer(value()));
            else if(flag=="--raw-pixel-file-work-bytes") options.raw_pixel_file_work_bytes=static_cast<std::size_t>(integer(value()));
            else if(flag=="--raw-pixel-max-samples") options.raw_pixels.max_samples=integer(value());
            else if(flag=="--raw-pixel-max-windows") options.raw_pixels.max_windows=integer(value());
            else if(flag=="--raw-pixel-min-vote") options.raw_pixels.min_vote_fraction=real(value());
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
            else if(flag=="--threshold") options.grid_cells.threshold=options.cells.threshold=options.tissue.threshold=real(value());
            else if(flag=="--cell-algorithm") {const auto v=value();if(v=="grid") {if(!options.cell_grid) options.cell_grid=GridCellSampling{};}else {legacy_algorithm=true;if(v=="distance") options.cells.algorithm=CellAlgorithm::DistanceWatershed;else if(v=="log") options.cells.algorithm=CellAlgorithm::LogWatershed;else if(v=="log-bodies") options.cells.algorithm=CellAlgorithm::LogWatershedBodies;else if(v=="log-sampled") options.cells.algorithm=CellAlgorithm::SampledLogWatershedBodies;else throw std::invalid_argument("unknown cell algorithm");}}
            else if(flag=="--cell-grid-samples" || flag=="--cell-grid-context" || flag.rfind("--grid-",0)==0) {
                if(!options.cell_grid) options.cell_grid=GridCellSampling{};auto& g=*options.cell_grid;auto& p=options.grid_cells;
                if(flag=="--cell-grid-samples" || flag=="--cell-grid-context") {
                    const auto v=value();const auto comma=v.find(',');if(comma==std::string::npos || v.find(',',comma+1)!=std::string::npos) throw std::invalid_argument("grid axes require two comma-separated integers");
                    const auto x=integer(v.substr(0,comma)),y=integer(v.substr(comma+1));const auto limit=flag=="--cell-grid-samples"?1000000u:4096u;
                    if(x>limit || y>limit) throw std::invalid_argument("grid axes exceed their count/context limits");
                    if(flag=="--cell-grid-samples") {g.samples_x=std::uint32_t(x);g.samples_y=std::uint32_t(y);}else {g.context_x=std::uint32_t(x);g.context_y=std::uint32_t(y);}
                } else if(flag=="--grid-units") {const auto v=value();if(v=="px") p.units=GridCellUnits::LevelZeroPixels;else if(v=="um") p.units=GridCellUnits::Micrometres;else throw std::invalid_argument("grid units must be px or um");}
                else if(flag=="--grid-feature") {const auto v=value();if(v=="od") p.feature=TissueFeature::OpticalDensitySum;else if(v=="stain1") p.feature=TissueFeature::Stain1;else if(v=="stain2") p.feature=TissueFeature::Stain2;else if(v=="stain3") p.feature=TissueFeature::Stain3;else throw std::invalid_argument("unknown grid cell feature");}
                else if(flag=="--grid-sampling") {const auto v=value();if(v=="coverage") p.sampling_policy=RoiSamplingPolicy::GeometryCoverage;else if(v=="winding") p.sampling_policy=RoiSamplingPolicy::PolygonWinding;else throw std::invalid_argument("grid sampling must be coverage or winding");}
                else if(flag=="--grid-sigma") p.sigma=real(value());else if(flag=="--grid-prominence") p.log_prominence=real(value());
                else if(flag=="--grid-min-radius") p.min_peak_radius=real(value());else if(flag=="--grid-seed-spacing") p.min_seed_distance=real(value());
                else if(flag=="--grid-expansion") p.cell_expansion=real(value());else if(flag=="--grid-min-area") p.min_nucleus_area=real(value());else if(flag=="--grid-max-area") p.max_nucleus_area=real(value());
                else if(flag=="--grid-work-bytes") p.work_bytes=std::size_t(integer(value()));else if(flag=="--grid-max-cells") p.max_cells=std::size_t(integer(value()));
                else if(flag=="--grid-contour-bytes") p.contours.work_bytes=std::size_t(integer(value()));else throw std::invalid_argument("unknown grid option: "+flag);
            }
            else if(flag=="--cell-log-prominence") {legacy_cell_flags=true;options.cells.log_prominence=real(value());}
            else if(flag=="--cell-context-padding") {legacy_cell_flags=true;const auto n=integer(value());if(n>4096) throw std::invalid_argument("cell context padding exceeds 4096");options.cells.context_padding=static_cast<std::uint32_t>(n);}
            else if(flag=="--cell-min-area-um2") options.cell_min_nucleus_area_um2=real(value());
            else if(flag=="--cell-max-area-um2") options.cell_max_nucleus_area_um2=real(value());
            else if(flag=="--level") {requested_level=true;const auto& v=value();if(v=="-1") options.cells.level=options.tissue.level=-1;else {auto n=integer(v);if(n>INT32_MAX) throw std::invalid_argument("level exceeds int32");options.cells.level=options.tissue.level=static_cast<std::int32_t>(n);}}
            else if(flag=="--tile-size") requested_tile_size=static_cast<std::size_t>(integer(value()));
            else if(flag=="--snapshot-bytes") options.snapshot_budget=static_cast<std::size_t>(integer(value()));
            else if(flag=="--materialize-bytes") options.materialize_budget=static_cast<std::size_t>(integer(value()));
            else if(flag=="--result-bytes") options.result_budget=static_cast<std::size_t>(integer(value()));
            else if(flag=="--csv-bytes") options.csv_limit=static_cast<std::size_t>(integer(value()));
            else if(flag=="--checkpoint-bytes") options.checkpoint_limit=integer(value());
            else if(flag=="--tma-only") options.tma_only=true;
            else if(flag=="--include-missing-tma") options.include_missing_tma=true;
            else if(flag=="--continue-on-error") options.failure=ProjectBatchFailure::Continue;
            else if(flag=="--no-resume") options.resume=false;
            else if(flag=="--publish-cells") options.publish_cells=true;
            else if(flag=="--nucleus") options.nucleus=true;
            else throw std::invalid_argument("unknown option: "+flag);
        }
        if(raw_pixel_flags && options.kind!=ProjectBatchKind::RawPixels) throw std::invalid_argument("raw classifier flags require --kind raw-pixels");
        if(!raw_pixel_model_path.empty()) options.raw_pixel_model=load_raw_pixel_classifier(raw_pixel_model_path,&cancelled);
        if(raw_grid_flags && !requested_raw_grid) throw std::invalid_argument("raw grid options require --raw-grid-samples NX,NY");
        if(options.raw_grid && (requested_raw_level || requested_level || requested_tile_size)) throw std::invalid_argument("raw grids do not accept natural level or tile flags");
        if(options.cells.algorithm==CellAlgorithm::SampledLogWatershedBodies) {options.cells.min_peak_radius=0;options.cells.min_seed_distance=0;}
        if(options.cell_grid && (legacy_algorithm || legacy_cell_flags || requested_level || requested_tile_size || options.cell_min_nucleus_area_um2 || options.cell_max_nucleus_area_um2))
            throw std::invalid_argument("grid cells require grid-unit options without legacy algorithm, level, tile or physical-area flags");
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
