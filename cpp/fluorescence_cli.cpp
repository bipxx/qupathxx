#include "fluorescence_cli.hpp"
#include "qupathxx/fluorescence_cell_objects.hpp"
#include "qupathxx/core.hpp"
#include "qupathxx/project.hpp"
#include "qupathxx/provenance.hpp"
#include "spatial_output_guard.hpp"
#include "json_string.hpp"
#include <charconv>
#include <cmath>
#include <iostream>
#include <set>
namespace qpx::detail {
namespace {
std::uint64_t unsigned_value(const std::string& s,std::uint64_t max) {
    std::uint64_t n=0;const auto p=std::from_chars(s.data(),s.data()+s.size(),n);
    if(s.empty() || p.ec!=std::errc{} || p.ptr!=s.data()+s.size() || n>max)throw std::invalid_argument("Invalid fluorescence unsigned integer: "+s);
    return n;
}
double real_value(const std::string& s) {
    double n=0;const auto p=std::from_chars(s.data(),s.data()+s.size(),n);
    if(s.empty() || p.ec!=std::errc{} || p.ptr!=s.data()+s.size() || !std::isfinite(n))throw std::invalid_argument("Invalid fluorescence finite number: "+s);
    return n;
}
}
std::optional<int> fluorescence_cli(const std::vector<std::string>& args,const std::atomic<bool>& cancelled) {
    if(args.size()<2 || (args[1]!="--detect-fluorescence" && args[1]!="--verify-fluorescence" && args[1]!="--list-fluorescence"))return {};
    const bool detect=args[1]=="--detect-fluorescence",verify=args[1]=="--verify-fluorescence";
    const std::size_t positional=detect?4:3;
    if(args.size()<positional)throw std::invalid_argument("Fluorescence commands need PROJECT, and detection needs a new OUTPUT project; see --help");
    std::uint64_t image=0,roi=0;RawRegionRequest region;FluorescenceCellOptions options;
    std::array<std::uint32_t,2> context{32,32};std::set<std::string> seen;std::string record;
    bool record_only=false;std::size_t save_bytes=512ull*1024*1024;std::uint64_t project_bytes=1024ull*1024*1024;
    for(std::size_t i=positional;i<args.size();++i) {
        const auto flag=args[i];if(!seen.insert(flag).second)throw std::invalid_argument("Duplicate fluorescence option: "+flag);
        if(detect && flag=="--record-only"){record_only=true;continue;}
        const bool common=flag=="--image" || (verify && flag=="--record");
        const bool numeric=flag=="--threshold" || flag=="--sigma" || flag=="--prominence" || flag=="--min-area" || flag=="--max-area" || flag=="--expansion";
        const bool index=flag=="--roi" || flag=="--first-channel" || flag=="--channels" || flag=="--segmentation-channel" || flag=="--z" || flag=="--time" || flag=="--x" || flag=="--y" || flag=="--width" || flag=="--height" || flag=="--context" || flag=="--max-cells" || flag=="--work-bytes" || flag=="--save-snapshot-bytes" || flag=="--project-bytes";
        if(!common && !(detect && (numeric || index || flag=="--method" || flag=="--units")))throw std::invalid_argument("Unknown fluorescence option: "+flag);
        if(++i==args.size())throw std::invalid_argument("Missing fluorescence option value: "+flag);const auto& v=args[i];
        if(flag=="--record"){record=v;continue;}
        if(flag=="--image"){image=unsigned_value(v,UINT64_MAX);continue;}
        if(flag=="--method"){if(v=="otsu")options.threshold_method=FluorescenceThreshold::Otsu256;else if(v=="explicit")options.threshold_method=FluorescenceThreshold::Explicit;else throw std::invalid_argument("Fluorescence method must be explicit or otsu");continue;}
        if(flag=="--units"){if(v=="px")options.units=FluorescenceCellUnits::Pixels;else if(v=="um")options.units=FluorescenceCellUnits::Micrometres;else throw std::invalid_argument("Fluorescence units must be px or um");continue;}
        if(numeric){const auto n=real_value(v);if(flag=="--threshold")options.threshold=n;else if(flag=="--sigma")options.sigma=n;else if(flag=="--prominence")options.peak_prominence=n;else if(flag=="--min-area")options.min_nucleus_area=n;else if(flag=="--max-area")options.max_nucleus_area=n;else options.cell_expansion=n;continue;}
        const auto n=unsigned_value(v,flag=="--roi"?UINT64_MAX:4ull*1024*1024*1024);
        if(flag=="--roi")roi=n;
        else if(flag=="--x")region.region.x=std::int64_t(n);else if(flag=="--y")region.region.y=std::int64_t(n);
        else if(flag=="--width")region.region.width=std::int64_t(n);else if(flag=="--height")region.region.height=std::int64_t(n);
        else if(flag=="--max-cells")options.max_cells=std::size_t(n);else if(flag=="--work-bytes")options.work_bytes=std::size_t(n);
        else if(flag=="--save-snapshot-bytes")save_bytes=std::size_t(n);else if(flag=="--project-bytes")project_bytes=n;
        else {if(n>65535)throw std::invalid_argument("Fluorescence axis/context option exceeds65535");
            if(flag=="--first-channel")region.first_channel=std::uint32_t(n);else if(flag=="--channels")region.channels=std::uint32_t(n);
            else if(flag=="--segmentation-channel")options.segmentation_channel=std::uint32_t(n);else if(flag=="--z")region.z=std::uint32_t(n);else if(flag=="--time")region.t=std::uint32_t(n);else context={std::uint32_t(n),std::uint32_t(n)};}
    }
    if(!image || (verify && record.empty()))throw std::invalid_argument("Fluorescence commands require positive --image; verification also requires --record");
    if(detect){options.validate();if(!save_bytes || !project_bytes)throw std::invalid_argument("Fluorescence save budgets must be positive");
        if((seen.count("--width") && !region.region.width) || (seen.count("--height") && !region.region.height) || !region.channels)throw std::invalid_argument("Fluorescence crop dimensions and channel count must be positive");
        if(options.threshold_method==FluorescenceThreshold::Otsu256 && seen.count("--threshold"))throw std::invalid_argument("Explicit --threshold cannot be combined with otsu");
        if(roi && (seen.count("--x") || seen.count("--y") || seen.count("--width") || seen.count("--height")))throw std::invalid_argument("Choose --roi or explicit crop, not both");}
    try {
        export_checkpoint(&cancelled);const auto source=export_path(args[2]);const auto output=detect?export_path(args[3]):std::string{};ReaderFileGuard guard(source);
        if(detect)protect_spatial_path(source,guard.version()->identity,output);
        const auto project=Project::open_without_journal_recovery(source);const auto doc=project->document(image);const auto images=project->images();
        std::shared_ptr<const FluorescenceCellRun> run;
        std::unique_ptr<FluorescenceCellRequest> captured;
        auto protect=[&]{guard.verify();export_checkpoint(&cancelled);if(!detect)return;
            if(run && captured)require_current_fluorescence_cells(*doc,*captured,*run,&cancelled);
            protect_spatial_path(source,guard.version()->identity,output);
            for(const auto* suffix:{"-journal","-wal","-shm"})protect_spatial_path(source+suffix,"",output);
            for(const auto& entry:images)if(!entry.embedded && !entry.source_path.empty())protect_spatial_path(export_path(entry.source_path),"",output);
            if(run && run->provenance()->reader_source){const auto& files=*run->provenance()->reader_source;
                if(files.primary)protect_spatial_path(files.primary->path,files.primary->identity,output);
                for(const auto& file:files.companions)protect_spatial_path(file.path,file.identity,output);}
        };
        protect();
        if(!detect && !verify){const auto ids=fluorescence_cell_record_ids(*doc);protect();
            std::cout<<"{\"schema\":\"qpx.fluorescence-cli/1\",\"state\":\"records\",\"records\":[";
            for(std::size_t i=0;i<ids.size();++i){if(i)std::cout<<',';std::cout<<json_string(ids[i]);}std::cout<<"]}\n";return 0;}
        std::vector<std::uint64_t> cells;
        if(verify){const auto request=replay_fluorescence_cell_record(*doc,record);run=detect_fluorescence_cells(request,&cancelled);
            if(!verify_fluorescence_cell_record(*doc,record,request,*run,&cancelled))throw std::runtime_error("Fluorescence replay differs from retained result");protect();}
        else {
            if(!roi){const auto m=doc->server()->raw_metadata(&cancelled);if(!m)throw std::runtime_error("Fluorescence source has no raw capability");
                if(!seen.count("--width"))region.region.width=m->width-region.region.x;if(!seen.count("--height"))region.region.height=m->height-region.region.y;}
            captured=std::make_unique<FluorescenceCellRequest>(*doc,region,roi,options,context);const auto& request=*captured;run=detect_fluorescence_cells(request,&cancelled);protect();
            if(!record_only && !run->raster().measurements.empty()){TissueContourOptions contours;contours.work_bytes=options.work_bytes;
                const auto traced=trace_fluorescence_cells(run,contours,&cancelled);cells=publish_fluorescence_cells(*doc,request,*traced,"Fluorescence cell",options.work_bytes,&cancelled);}
            record=keep_fluorescence_cell_run(*doc,request,*run,&cancelled);protect();project->save_new(output,save_bytes,project_bytes,&cancelled,protect);
        }
        std::size_t boundary=0;for(const auto& b:run->raster().boundaries)if(b.nucleus || b.cell)++boundary;
        std::cout<<"{\"schema\":\"qpx.fluorescence-cli/1\",\"state\":"<<json_string(verify?"verified":"saved")<<",\"record\":"<<json_string(record)<<",\"cells\":"<<run->raster().measurements.size()<<",\"published_cells\":"<<cells.size()<<",\"boundary_cells\":"<<boundary<<",\"run_sha256\":"<<json_string(run->sha256())<<"}\n";return 0;
    }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return cancelled.load()?130:1;}
}
}
