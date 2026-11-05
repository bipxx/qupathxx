#include "qupathxx/analysis.hpp"
#include "qupathxx/cell_statistics.hpp"
#include "qupathxx/measurement_tables.hpp"
#include "qupathxx/project.hpp"
#include <iostream>
#include <stdexcept>
using namespace qpx;
int main(int argc,char** argv) {
    try {
        if(argc!=2) throw std::invalid_argument("region statistics witness requires the pre-statistics project path");
        auto project=Project::open(argv[1]);const auto doc=project->document(1);
        if(doc->analysis_record_count()!=5) throw std::runtime_error("legacy region witness must have five records");
        std::cout<<"{\"observer_revision\":\""<<build_environment()->git_revision<<"\",\"analysis_result_bytes\":"<<sizeof(AnalysisResult)
            <<",\"analysis_request_bytes\":"<<sizeof(AnalysisRequest)<<",\"analysis_record_bytes\":"<<sizeof(AnalysisRecord)
            <<",\"compartment_statistics_bytes\":"<<sizeof(CompartmentIntensityStatistics)
            <<",\"channel_statistics_bytes\":"<<sizeof(ChannelIntensityStatistics)<<",\"records\":[";
        bool first=true;
        for(const AnalysisRecord& record:doc->analysis_records()) {
            if(!first) std::cout<<',';first=false;
            const auto captured=MeasurementTableRequest(*doc,MeasurementTableKind::Roi,{record.id});
            std::cout<<"{\"id\":"<<record.id<<",\"samples\":"<<record.result.samples<<",\"payload_bytes\":"<<record.payload_bytes()
                <<",\"table_bytes\":"<<captured.snapshot_bytes()<<'}';
        }
        std::cout<<"]}\n";return 0;
    } catch(const std::exception& e) {std::cerr<<e.what()<<'\n';return 1;}
}
