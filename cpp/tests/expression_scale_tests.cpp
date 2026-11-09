#include "qupathxx/gene_view.hpp"
#include "qupathxx/workflow.hpp"
#include <nlohmann/json.hpp>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <future>
#include <iostream>
#include <thread>

namespace fs=std::filesystem;
using namespace qpx;
using Clock=std::chrono::steady_clock;
constexpr std::size_t work=256u*1024*1024;
constexpr std::uint64_t observations=100000,features=30000,entries=5000000;
void require(bool yes,const char* message){if(!yes)throw std::runtime_error(message);}
double milliseconds(Clock::time_point start){return std::chrono::duration<double,std::milli>(Clock::now()-start).count();}
std::uint64_t expected(std::uint64_t observation,std::uint64_t feature) {
    // Independent row oracle: inspect all50 feature positions for this observation.
    for(std::uint64_t k=0;k<50;++k)
        if((observation%features+k*601)%features==feature)return (feature+observation)%97+1;
    return 0;
}
void generate(const fs::path& root) {
    for(const auto* name:{"matrix.mtx","features.tsv","observations.tsv","counts.qmx","scale.json"})
        require(!fs::exists(root/name),"Scale generation requires new outputs; use --reopen to reuse a qualified store");
    std::ofstream matrix(root/"matrix.mtx",std::ios::binary),genes(root/"features.tsv",std::ios::binary),spots(root/"observations.tsv",std::ios::binary);
    require(bool(matrix)&&bool(genes)&&bool(spots),"Create scale fixture");
    matrix<<"%%MatrixMarket matrix coordinate integer general\n"<<features<<' '<<observations<<' '<<entries<<'\n';
    genes<<"feature_id\n";spots<<"observation_id\n";
    for(std::uint64_t f=0;f<features;++f)genes<<'G'<<f<<'\n';
    for(std::uint64_t o=0;o<observations;++o)spots<<'O'<<o<<'\n';
    for(std::uint64_t k=0;k<50;++k)for(std::uint64_t o=0;o<observations;++o) {
        const auto f=(o%features+k*601)%features;
        matrix<<f+1<<' '<<o+1<<' '<<(f+o)%97+1<<'\n';
    }
    matrix.close();genes.close();spots.close();require(bool(matrix)&&bool(genes)&&bool(spots),"Complete scale fixture streams");
}
int main(int argc,char** argv) {
    try {
        require(argc==3 && (std::string(argv[1])=="--generate" || std::string(argv[1])=="--reopen"),"Usage: expression_scale_tests --generate|--reopen DIRECTORY");
        require(has_expression_matrix_storage(),"Scale qualification requires native indexed storage");
        const auto root=fs::absolute(fs::u8path(argv[2]));fs::create_directories(root);
        nlohmann::json report={{"work_budget_bytes",work},{"features",features},{"observations",observations},{"entries",entries}};
        std::string digest;
        if(std::string(argv[1])=="--generate") {
            generate(root);ExpressionMatrixImportOptions options;options.work_bytes=work;
            const auto imported=import_expression_matrix((root/"matrix.mtx").u8string(),(root/"features.tsv").u8string(),(root/"observations.tsv").u8string(),(root/"counts.qmx").u8string(),options);
            require(imported.peak_work_bytes<=work,"Import exceeds declared accounted work");
            digest=imported.file_sha256;
            report["import"]={{"elapsed_ms",imported.elapsed_ms},{"peak_accounted_work_bytes",imported.peak_work_bytes},{"matrix_bytes",imported.matrix_bytes},{"store_bytes",imported.file_bytes},{"input_sha256",imported.metadata.input_sha256}};
        } else {
            std::ifstream previous(root/"scale.json");nlohmann::json prior;previous>>prior;
            digest=prior.at("file_sha256").get<std::string>();report["reused_import"]=true;
        }
        auto begin=Clock::now();const auto source=open_expression_matrix((root/"counts.qmx").u8string(),digest,work);
        report["open_ms"]=milliseconds(begin);report["file_sha256"]=source->file_sha256();report["source_sha256"]=source->metadata().source_sha256;
        require(source->metadata().observations==observations && source->metadata().features==features && source->metadata().entries==entries,"Qualified scale dimensions");
        begin=Clock::now();const auto page=source->search_features("",features-16,16,work);
        require(page.matches==features && page.features.size()==16 && page.features.back().id=="G29999","High-axis metadata page");
        report["catalogue_ms"]=milliseconds(begin);
        ExpressionMatrixSelection selection;selection.features={0,14999,29999};begin=Clock::now();
        {
            const auto block=source->read(selection,work);std::size_t visited=0;
            for(std::size_t f=0;f<selection.features.size();++f) {
                auto at=block.offsets[f];
                for(std::uint64_t o=0;o<observations;++o) {
                    const auto wanted=expected(o,selection.features[f]);std::uint64_t actual=0;
                    if(at<block.offsets[f+1] && block.observation_indices[at]==o){actual=block.counts[at++];++visited;}
                    require(actual==wanted,"Selected sparse count differs from independent row oracle");
                }
                require(at==block.offsets[f+1],"Every selected sparse entry accounted");
            }
            report["selected_entries_checked"]=visited;report["selected_block_sha256"]=block.sha256;
        }
        report["read_and_oracle_ms"]=milliseconds(begin);
        std::vector<ExpressionObservation> rows;rows.reserve(observations);
        for(std::uint64_t o=0;o<observations;++o)rows.push_back({o+1,"O"+std::to_string(o),{double(o%1000),double(o/1000)}});
        CoordinateFrame frame;frame.id="scale-level-zero-px";frame.source_id="generated-open-scale-fixture";
        begin=Clock::now();const auto spatial=ExpressionMatrixSpatialSnapshot::create(source,std::move(rows),frame,work);
        report["spatial_capture_ms"]=milliseconds(begin);
        GeneViewSession session(work);session.bind(spatial);GeneViewOptions options;options.enabled=true;options.compare=true;options.genes={"G0","G29999"};
        begin=Clock::now();session.configure(options);session.wait();require(bool(session.result()),session.error().c_str());
        report["comparison_ms"]=milliseconds(begin);
        auto before=session.result();const auto* buffer=before->fields[0]->dataset.counts().data();
        require(before->fields[0]->dataset.observations().size()==observations,"All scale observations are displayed");
        for(std::uint64_t o=0;o<observations;++o)for(unsigned f=0;f<2;++f)
            require(before->fields[0]->dataset.counts()[2*o+f]==expected(o,f?29999:0),"Full spatial projection agrees with independent row oracle");
        const auto retained=spatial.payload_bytes()+before->fields[0]->dataset.payload_bytes()+before->fields[0]->payload_bytes+before->fields[1]->payload_bytes;
        require(retained<=work,"Retained spatial/value/color buffers exceed declared bound");report["retained_accounted_bytes"]=retained;
        before.reset();begin=Clock::now();options.display.opacity=.375;options.display.palette=GeneDisplayPalette::Gray;
        session.configure(options);session.wait();require(bool(session.result()),session.error().c_str());
        require(session.result()->fields[0]->dataset.counts().data()==buffer,"Scale recolor must reuse exact selected values");report["recolor_ms"]=milliseconds(begin);
        // Exercise obsolete-generation cancellation with active native work, then
        // require the newest gene request rather than merely an empty result.
        options.genes={"G14999","G1"};session.configure(options);session.poll();session.cancel();
        options.genes={"G29999","G0"};session.configure(options);begin=Clock::now();session.wait();
        require(session.result() && session.result()->fields[0]->feature=="G29999" && session.error().empty(),"Latest scale generation publishes after cancellation");
        report["cancel_and_latest_ms"]=milliseconds(begin);
        std::atomic<bool> cancelled{false};selection.features.clear();for(std::uint64_t f=0;f<4096;++f)selection.features.push_back(f);
        std::promise<void> started;auto ready=started.get_future();
        auto pending=std::async(std::launch::async,[&]{started.set_value();try{source->read(selection,work,&cancelled);return false;}catch(const WorkflowCancelled&){return true;}});
        ready.wait();std::this_thread::sleep_for(std::chrono::milliseconds(5));begin=Clock::now();cancelled.store(true);
        require(pending.get(),"Native indexed read must observe cancellation");report["read_cancel_ms"]=milliseconds(begin);
        report["all_checks_passed"]=true;
        const auto name=std::string(argv[1])=="--generate"?"scale.json":"reopen.json";
        require(!fs::exists(root/name),"Do not overwrite a prior scale receipt");
        std::ofstream output(root/name);output<<report.dump(2)<<'\n';output.close();require(bool(output),"Write scale evidence");
        std::cout<<report.dump(2)<<'\n';
    }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
