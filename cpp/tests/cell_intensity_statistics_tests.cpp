#include "../cell_intensity_statistics.hpp"
#include "../cell_archive.hpp"
#include "../cell_statistics_codec.hpp"
#include "qupathxx/project.hpp"
#include "qupathxx/scientific_sampling.hpp"
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
    qpx::CellFeature feature;feature.label=1;feature.nucleus_pixels=1;feature.cell_pixels=3;
    feature.nucleus_rgb={1,10,100};feature.cell_rgb={13./3,130./3,185};
    feature.nucleus_channels={2,20,-1};feature.cell_channels={14./3,140./3,1};
    feature.cytoplasm_rgb=qpx::ColorVector{6,60,227.5};feature.cytoplasm_channels=qpx::ColorVector{6,60,2};
    validate_cell_statistics(result,feature);
    auto invalid=result;invalid.cell.rgb.minimum[0]=256;
    refuses<std::invalid_argument>([&]{validate_cell_statistics(invalid,feature);});
    invalid=result;invalid.cell.channels.sum[0]+=1;
    refuses<std::invalid_argument>([&]{validate_cell_statistics(invalid,feature);});
    invalid=result;(*invalid.cell.rgb.sample_standard_deviation)[0]=-1;
    refuses<std::invalid_argument>([&]{validate_cell_statistics(invalid,feature);});
    invalid=result;invalid.nucleus.rgb.sample_standard_deviation=qpx::ColorVector{};
    refuses<std::invalid_argument>([&]{validate_cell_statistics(invalid,feature);});
    invalid=result;invalid.cytoplasm.reset();
    refuses<std::invalid_argument>([&]{validate_cell_statistics(invalid,feature);});
    invalid=result;invalid.nucleus.samples=2;
    refuses<std::invalid_argument>([&]{validate_cell_statistics(invalid,feature);});
    invalid=result;invalid.cell.rgb.sum[0]=std::numeric_limits<double>::quiet_NaN();
    refuses<std::invalid_argument>([&]{validate_cell_statistics(invalid,feature);});
    std::cout<<"Cell intensity reducer: six QuPath0.7 numeric references, sample SD n-1/absent singleton, exact compartments, float/byte and double input, finite/background/budget/cancellation checks passed; retained API integration remains open\n";
}


void check_cell_intensity_results() {
    using namespace qpx;
    std::ifstream input(std::filesystem::path(QPX_NOMINAL_CELL_FIXTURE)/"source.rgb",std::ios::binary);
    std::vector<std::uint8_t> bytes{std::istreambuf_iterator<char>(input),std::istreambuf_iterator<char>()};check(bytes.size()==256*192*3);
    Document document(Image(256,192,bytes));document.set_calibration(.5,.5);
    const auto object=document.add_rectangle({0,0,256,192});const StainTransform transform({{1,0,0},{0,1,0}});
    auto verify=[&](const CellRequest& request,const std::vector<double>& rgb,bool float_channels) {
        const auto result=detect_cells(request);check(!result->features().empty());
        check(result->intensity_statistics_format()==std::optional<std::string>("qpx.cell-intensity-statistics/1"));
        const auto& nuclei=result->nuclei()->labels();const auto& bodies=result->cells()->labels();check(3*nuclei.size()==rgb.size());
        for(const auto& feature:result->features()) {
            const auto statistics=*result->intensity_statistics(feature.label);
            for(int compartment=0;compartment<3;++compartment) {
                std::vector<std::size_t> samples;
                for(std::size_t i=0;i<nuclei.size();++i) if(compartment==0?nuclei[i]==feature.label:compartment==1?bodies[i]==feature.label:bodies[i]==feature.label && !nuclei[i]) samples.push_back(i);
                const auto* actual=compartment==0?&statistics.nucleus:compartment==1?&statistics.cell:statistics.cytoplasm?&*statistics.cytoplasm:nullptr;
                if(samples.empty()) {check(!actual);continue;}check(actual && actual->samples==samples.size());
                for(bool transformed:{false,true}) for(std::size_t c=0;c<3;++c) {
                    auto value=[&](std::size_t i) {double v=rgb[3*i+c];if(transformed) {v=std::max(0.,std::log10(255./std::max(1.,v)));if(float_channels) v=double(float(v));}return v;};
                    long double sum=0;double lo=value(samples[0]),hi=lo;
                    for(auto i:samples) {const auto v=value(i);sum+=v;lo=std::min(lo,v);hi=std::max(hi,v);}
                    const auto& measured=transformed?actual->channels:actual->rgb;
                    close(measured.sum[c],double(sum));close(measured.minimum[c],lo);close(measured.maximum[c],hi);
                    if(samples.size()<2) check(!measured.sample_standard_deviation);
                    else {long double squared=0,mean=sum/samples.size();for(auto i:samples) {const auto delta=static_cast<long double>(value(i))-mean;squared+=delta*delta;}
                        close((*measured.sample_standard_deviation)[c],std::sqrt(double(squared/(samples.size()-1))));}
                }
            }
        }
        const auto encoded=CellArchiveCodec::encode(*result);check(encoded[3]==(result->grid_parameters()?11:10));
        const auto restored=CellArchiveCodec::decode(encoded.data(),encoded.size(),request.roi(),document.metadata());
        check(restored->intensity_statistics_format()==result->intensity_statistics_format());
        for(const auto& feature:result->features()) check(restored->intensity_statistics(feature.label)==result->intensity_statistics(feature.label));
        check(CellArchiveCodec::encode(*restored)==encoded);
        const auto record=document.keep_cells(request,result);const auto replay=CellRequest::replay(document,record);
        check(document.verify_cells(record,replay,*detect_cells(replay)));
    };
    const std::vector<double> original(bytes.begin(),bytes.end());
    for(auto algorithm:{CellAlgorithm::DistanceWatershed,CellAlgorithm::LogWatershed,CellAlgorithm::LogWatershedBodies,CellAlgorithm::SampledLogWatershedBodies}) {
        CellParameters p(algorithm);p.threshold=.4;p.sigma=1;p.min_nucleus_pixels=16;p.max_nucleus_pixels=512;p.contours.work_bytes=1024*1024;
        std::cout<<"Intensity integration natural "<<int(algorithm)<<std::endl;verify(CellRequest(document,object,transform,p),original,true);
    }
    GridCellParameters parameters;parameters.threshold=.4;parameters.sigma=1;parameters.min_nucleus_area=16;parameters.max_nucleus_area=512;parameters.contours.work_bytes=1024*1024;
    const ScientificGrid grid(256,192,{0,256,171,0},{0,192,128,0});
    std::cout<<"Intensity integration explicit grid"<<std::endl;verify(CellRequest(document,object,transform,grid,parameters),read_scientific_rgb(*document.server(),grid).values(),false);
    for(auto coordinates:{RgbNominalSampling::IntegerOrigin,RgbNominalSampling::AffineInverse}) {
        const RgbNominalRequest source(0,0,256,192,1.5,coordinates);const auto processed=read_rgb_nominal_region(*document.server(),source);
        std::vector<double> rgb;for(std::size_t i=0;i<processed.bytes().size();i+=4) for(std::size_t c=0;c<3;++c) rgb.push_back(processed.bytes()[i+c]);
        std::cout<<"Intensity integration nominal "<<int(coordinates)<<std::endl;verify(CellRequest(document,object,transform,NominalCellSampling(source,0,0),parameters),rgb,false);
    }
    const auto old=Project::open((std::filesystem::path(QPX_CELL_STATISTICS_FIXTURE)/"legacy-project.qpxproj").u8string());const auto legacy=old->document(1);
    check(legacy->cell_records().size()==6);
    for(const auto& record:legacy->cell_records()) {
        const auto result=record->result;check(!result->intensity_statistics_format());
        if(!result->features().empty()) check(!result->intensity_statistics(1));
        const auto encoded=CellArchiveCodec::encode(*result);const auto replay=CellRequest::replay(*legacy,record->id);
        std::cout<<"Intensity integration legacy "<<record->id<<std::endl;const auto rerun=detect_cells(replay);check(!rerun->intensity_statistics_format());
        check(legacy->verify_cells(record->id,replay,*rerun));check(CellArchiveCodec::encode(*result)==encoded);
    }
    const auto tight=Project::open((std::filesystem::path(QPX_CELL_STATISTICS_FIXTURE)/"legacy-tight-grid.qpxproj").u8string());
    const auto tight_doc=tight->document(1);const auto tight_record=tight_doc->cell_records().back();
    const auto tight_request=CellRequest::replay(*tight_doc,tight_record->id);
    check(tight_request.estimated_work_bytes()==tight_record->result->grid_parameters()->work_bytes);
    std::cout<<"Intensity integration tight legacy budget "<<tight_request.estimated_work_bytes()<<std::endl;check(tight_doc->verify_cells(tight_record->id,tight_request,*detect_cells(tight_request)));
    std::cout<<"Retained intensity statistics: four natural/area/two nominal recipes agree with independent two-pass moments; QCL10/11 canonical replay and six old mean-only recipe replays passed\n";
}
