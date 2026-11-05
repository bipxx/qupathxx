#include "../cell_intensity_statistics.hpp"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>

namespace {
using namespace qpx::cell_detail;
void check(bool value) {if(!value) throw std::runtime_error("cell intensity statistics regression failed");}
void close(double a,double b) {check(std::abs(a-b)<=1e-12*std::max(1.,std::abs(b)));}
template<class E,class F> void refuses(F action) {
    try {action();} catch(const E&) {return;}
    throw std::runtime_error("cell intensity statistics refusal missing");
}
}
void check_cell_intensity_statistics() {
    std::ifstream reference(std::filesystem::path(QPX_CELL_STATISTICS_FIXTURE)/"reference.tsv");
    check(bool(reference));std::string line;std::getline(reference,line);check(line=="qpx.cell-intensity-reference/1");
    std::getline(reference,line);std::size_t cases=0;
    while(std::getline(reference,line)) {
        std::istringstream row(line);std::string name,sum,mean,minimum,maximum,sd;std::size_t count;
        check(bool(row>>name>>count>>sum>>mean>>minimum>>maximum>>sd));
        std::vector<double> rgb,channels;std::vector<std::uint32_t> labels(count,1);
        for(std::size_t i=0;i<count;++i) {double v;check(bool(row>>v));rgb.insert(rgb.end(),{12,34,56});channels.insert(channels.end(),{v,2*v,-v});}
        std::string extra;check(!(row>>extra));
        const auto cells=count?1:0;const auto bytes=cell_intensity_statistics_work_bytes(cells);
        const auto measured=measure_cell_intensity_statistics(rgb,channels,labels,labels,cells,bytes);
        ++cases;if(!count) {check(measured.empty());continue;}
        const auto& result=measured.at(0);check(result.nucleus.samples==count && result.cell.samples==count && !result.cytoplasm);
        close(result.nucleus.channels.sum[0],std::stod(sum));close(result.nucleus.channels.sum[1],2*std::stod(sum));
        close(result.nucleus.channels.sum[2],-std::stod(sum));
        close(result.nucleus.channels.minimum[0],std::stod(minimum));close(result.nucleus.channels.maximum[0],std::stod(maximum));
        close(result.nucleus.channels.minimum[2],-std::stod(maximum));close(result.nucleus.channels.maximum[2],-std::stod(minimum));
        close(result.nucleus.channels.sum[0]/double(count),std::stod(mean));
        close(result.nucleus.rgb.sum[0],12*double(count));check(result.nucleus.rgb.minimum[1]==34 && result.nucleus.rgb.maximum[2]==56);
        check(bool(result.nucleus.channels.sample_standard_deviation)==(sd!="none"));
        if(sd!="none") {
            close((*result.nucleus.channels.sample_standard_deviation)[0],std::stod(sd));
            close((*result.nucleus.channels.sample_standard_deviation)[1],2*std::stod(sd));
            close((*result.nucleus.channels.sample_standard_deviation)[2],std::stod(sd));
            check((*result.nucleus.rgb.sample_standard_deviation)[0]==0);
        }
        refuses<std::length_error>([&]{measure_cell_intensity_statistics(rgb,channels,labels,labels,cells,bytes-1);});
    }
    check(cases==6);
    std::vector<double> rgb{1,10,100,3,30,200,9,90,255},channels{2,20,-1,4,40,1,8,80,3};
    std::vector<std::uint32_t> nuclei{1,0,0},bodies{1,1,1};
    const auto budget=cell_intensity_statistics_work_bytes(1);
    const auto result=measure_cell_intensity_statistics(rgb,channels,nuclei,bodies,1,budget).at(0);
    check(result.nucleus.samples==1 && result.cell.samples==3 && result.cytoplasm->samples==2);
    check(!result.nucleus.rgb.sample_standard_deviation && !result.nucleus.channels.sample_standard_deviation);
    check(result.cell.rgb.sum[0]==13 && result.cell.rgb.sum[1]==130 && result.cell.rgb.sum[2]==555);
    check(result.cytoplasm->rgb.sum[0]==12 && result.cytoplasm->rgb.minimum[2]==200 && result.cytoplasm->rgb.maximum[2]==255);
    close((*result.cytoplasm->rgb.sample_standard_deviation)[0],std::sqrt(18.));
    close((*result.cytoplasm->channels.sample_standard_deviation)[0],std::sqrt(8.));
    close((*result.cell.channels.sample_standard_deviation)[0],std::sqrt(28./3.));
    const std::vector<float> float_channels(channels.begin(),channels.end());
    const std::vector<std::uint8_t> byte_rgb(rgb.begin(),rgb.end());
    const auto legacy=measure_cell_intensity_statistics(byte_rgb,float_channels,nuclei,bodies,1,budget).at(0);
    check(legacy.cytoplasm->channels.sum==result.cytoplasm->channels.sum);
    // Background is not part of any compartment. Do not reject unsupported
    // samples or make zero-valued foreground indistinguishable from background.
    auto background_rgb=rgb,background_channels=channels;auto foreground=nuclei;
    background_rgb[3]=std::numeric_limits<double>::quiet_NaN();background_channels[3]=std::numeric_limits<double>::infinity();
    const auto background=measure_cell_intensity_statistics(background_rgb,background_channels,foreground,foreground,1,budget).at(0);
    check(background.cell.samples==1 && !background.cytoplasm);
    refuses<std::invalid_argument>([&]{measure_cell_intensity_statistics(background_rgb,channels,nuclei,bodies,1,budget);});
    refuses<std::invalid_argument>([&]{measure_cell_intensity_statistics(rgb,background_channels,nuclei,bodies,1,budget);});
    refuses<std::invalid_argument>([&]{measure_cell_intensity_statistics(rgb,channels,std::vector<std::uint32_t>{2,0,0},bodies,2,cell_intensity_statistics_work_bytes(2));});
    refuses<std::invalid_argument>([&]{measure_cell_intensity_statistics(rgb,channels,nuclei,bodies,2,cell_intensity_statistics_work_bytes(2));});
    refuses<std::invalid_argument>([&]{measure_cell_intensity_statistics(std::vector<double>{0},channels,nuclei,bodies,1,budget);});
    refuses<std::length_error>([&]{cell_intensity_statistics_work_bytes(std::numeric_limits<std::size_t>::max());});
    std::atomic<bool> cancelled{true};
    refuses<std::runtime_error>([&]{measure_cell_intensity_statistics(rgb,channels,nuclei,bodies,1,budget,&cancelled);});
    auto extreme=channels;extreme[0]=std::numeric_limits<double>::max();extreme[3]=-std::numeric_limits<double>::max();
    refuses<std::overflow_error>([&]{measure_cell_intensity_statistics(rgb,extreme,nuclei,bodies,1,budget);});
    std::cout<<"Cell intensity reducer: six QuPath0.7 numeric references, sample SD n-1/absent singleton, exact compartments, float/byte and double input, finite/background/budget/cancellation checks passed; retained API integration remains open\n";
}
