#include "qptma_project_cli.hpp"
#include "qupathxx/project.hpp"
#include "qupathxx/qptma.hpp"
#include "atomic_export.hpp"
#include "reader_file_guard.hpp"
#include "batch_checkpoint.hpp"
#include <charconv>
#include <filesystem>
#include <iostream>
#include <limits>
#include <set>
namespace qpx::detail {
std::optional<int> qptma_project_cli(const std::vector<std::string>& args,const std::atomic<bool>& cancelled) {
    if(args.size()<2 || (args[1]!="--import-qptma" && args[1]!="--export-qptma"))return {};
    const bool importing=args[1]=="--import-qptma";const std::size_t positional=importing?5:4;
    const char* usage=importing?"usage: --import-qptma PROJECT GRID.qptma OUTPUT.qpxproj --image ID [--qptma-bytes N --snapshot-bytes N --project-bytes N]":
        "usage: --export-qptma PROJECT OUTPUT.qptma --image ID [--qptma-bytes N]";
    if(args.size()<positional)throw std::invalid_argument(usage);
    std::uint64_t image_id=0,file_bytes=1024ull*1024*1024;std::size_t text_bytes=8*1024*1024,snapshot_bytes=512ull*1024*1024;std::set<std::string> seen;
    for(std::size_t i=positional;i<args.size();++i) {
        const auto option=args[i];
        if(option!="--image" && option!="--qptma-bytes" && !(importing && (option=="--snapshot-bytes" || option=="--project-bytes")))throw std::invalid_argument(usage);
        if(!seen.insert(option).second || ++i==args.size())throw std::invalid_argument(usage);
        std::uint64_t number=0;const auto& value=args[i];const auto result=std::from_chars(value.data(),value.data()+value.size(),number);
        if(value.empty() || !number || result.ec!=std::errc{} || result.ptr!=value.data()+value.size())throw std::invalid_argument("QPTMA options require positive unsigned integers");
        if(option=="--image")image_id=number;
        else if(option=="--qptma-bytes") {if(number>64*1024*1024)throw std::invalid_argument("QPTMA text budget exceeds 64 MiB");text_bytes=static_cast<std::size_t>(number);}
        else {if(number>4ull*1024*1024*1024 || number>std::numeric_limits<std::size_t>::max())throw std::invalid_argument("QPTMA project budget exceeds 4 GiB");if(option=="--snapshot-bytes")snapshot_bytes=static_cast<std::size_t>(number);else file_bytes=number;}
    }
    if(!image_id)throw std::invalid_argument("QPTMA project commands require an explicit --image ID");
    try {
        export_checkpoint(&cancelled);
        const auto source=export_path(args[2]),output=export_path(args[importing?4:3]);
        // Project::open can repair a hot rollback journal. This read-only CLI
        // rejects journal-bearing inputs before opening rather than repairing.
        auto journal=std::filesystem::u8path(source);journal+="-journal";
        if(std::filesystem::exists(journal))throw std::invalid_argument("project has a rollback journal; reopen/recover it before QPTMA interchange");
        ReaderFileGuard observed(source);guard_file_output(source,observed.version()->identity,output);
        const auto project=Project::open(source);const auto images=project->images();
        const ProjectImage* selected=nullptr;for(const auto& image:images)if(image.id==image_id)selected=&image;
        if(!selected)throw std::invalid_argument("selected QPTMA image does not exist");
        auto protect=[&] {guard_file_output(source,observed.version()->identity,output);
            for(const auto& image:images)if(!image.embedded && !image.source_path.empty())guard_file_output(export_path(image.source_path),"",output);};
        protect();const auto document=project->document(image_id);
        if(importing) {
            const auto input=export_path(args[3]);ReaderFileGuard wire(input);guard_file_output(input,wire.version()->identity,output);
            auto data=load_qptma(input,text_bytes,&cancelled);export_checkpoint(&cancelled);import_qptma(*document,data);
            auto validate=[&]{observed.verify();wire.verify();protect();guard_file_output(input,wire.version()->identity,output);};
            validate();project->save_new(output,snapshot_bytes,file_bytes,&cancelled,validate);
            std::cout<<"{\"schema\":\"qpx.qptma-project-import/1\",\"state\":\"saved\",\"image_id\":"<<image_id<<",\"cores\":"<<data.cores.size()<<",\"rows\":"<<data.rows<<",\"columns\":"<<data.columns<<",\"output\":"<<batch_json_quote(output)<<"}\n";
        } else {
            const auto snapshot=document->tma_grid();if(!snapshot)throw std::invalid_argument("selected image has no TMA grid");
            const auto report=qptma_from_grid(*snapshot,selected->source_path,selected->name);const auto text=qptma_text(report.data,text_bytes,&cancelled);
            AtomicExportFile file(output,text_bytes,&cancelled);file.write(text.data(),text.size());observed.verify();protect();file.commit(false);
            std::cout<<"{\"schema\":\"qpx.qptma-project-export/1\",\"state\":\"saved\",\"image_id\":"<<image_id<<",\"cores\":"<<report.data.cores.size()<<",\"bytes\":"<<text.size()<<",\"omitted_metadata\":[";
            for(std::size_t i=0;i<report.omitted_metadata.size();++i){if(i)std::cout<<',';std::cout<<batch_json_quote(report.omitted_metadata[i]);}
            std::cout<<"],\"output\":"<<batch_json_quote(output)<<"}\n";
        }
        return 0;
    }catch(const std::exception& error) {
        std::cerr<<"{\"schema\":\"qpx.batch-error/1\",\"error\":"<<batch_json_quote(error.what())<<"}\n";return cancelled.load()?130:1;
    }
}
}
