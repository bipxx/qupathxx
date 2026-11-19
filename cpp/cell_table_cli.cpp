#include "cell_table_cli.hpp"
#include "project_export_guard.hpp"
#include "qupathxx/cell_tables.hpp"
#include "qupathxx/project.hpp"
#include "project_database_output.hpp"
#include "atomic_export.hpp"
namespace qpx::detail {
std::optional<int> cell_table_cli(const std::vector<std::string>& args,const std::atomic<bool>& cancelled) {
    if(args.size()<2 || args[1]!="--export-cell-table") return {};
    if(args.size()<4) throw std::invalid_argument("usage: --export-cell-table PROJECT OUTPUT.csv [--capture-calibration] [--intensity-statistics] [--published] [--require-current]");
    CellTableOptions options;
    for(std::size_t i=4;i<args.size();++i) {
        const auto& flag=args[i];
        if(flag=="--capture-calibration") {if(options.include_capture_calibration) throw std::invalid_argument("duplicate capture-calibration option");options.include_capture_calibration=true;}
        else if(flag=="--intensity-statistics") {if(options.include_intensity_statistics) throw std::invalid_argument("duplicate intensity-statistics option");options.include_intensity_statistics=true;}
        else if(flag=="--published") {if(options.published_objects) throw std::invalid_argument("duplicate published option");options.published_objects=true;}
        else if(flag=="--require-current") {if(options.require_current) throw std::invalid_argument("duplicate require-current option");options.require_current=true;}
        else throw std::invalid_argument("unknown cell table export option: "+flag);
    }
    export_checkpoint(&cancelled);
    const auto source=export_path(args[2]),destination=export_path(args[3]);
    ReaderFileGuard observed(source);
    guard_project_database_output(observed,destination);
    const auto project=Project::open_without_journal_recovery(source);const auto images=project->images();
    auto protect=[&] {
        guard_project_database_output(observed,destination);
        guard_project_export_output(*project,images,destination,&cancelled);
    };
    protect();
    const auto csv=cell_measurements_csv(CellTableRequest(*project,{},options),&cancelled);
    AtomicExportFile output(destination,options.byte_limit,&cancelled);output.write(csv.data(),csv.size());
    observed.verify();protect();output.commit();return 0;
}
}
