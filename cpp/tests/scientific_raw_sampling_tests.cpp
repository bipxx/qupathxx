#include "qupathxx/scientific_raw_sampling.hpp"
#include "qupathxx/provenance.hpp"
#include "raw_scientific_sampling_reference.hpp"
#include <cstring>
#include <iostream>
#include <iterator>
#include <limits>
#include <sstream>

namespace {
void require(bool value) {if(!value) throw std::runtime_error("scientific raw sampling assertion failed");}
template<class E,class F> void rejects(F action) {
    try {action();} catch(const E&) {return;}throw std::runtime_error("scientific raw sampler did not reject");
}
template<class T> T source_value(std::uint64_t raw) {
    T value;
    if constexpr(sizeof(T)==8) std::memcpy(&value,&raw,8);
    else if constexpr(sizeof(T)==4) {const auto v=std::uint32_t(raw);std::memcpy(&value,&v,4);}
    else if constexpr(sizeof(T)==2) {const auto v=std::uint16_t(raw);std::memcpy(&value,&v,2);}
    else {const auto v=std::uint8_t(raw);std::memcpy(&value,&v,1);}
    return value;
}
std::uint64_t bits(double value) {std::uint64_t out;std::memcpy(&out,&value,8);return out;}
class Reader final:public qpx::ImageServer {
public:
    std::shared_ptr<qpx::RawImageMetadata> raw=std::make_shared<qpx::RawImageMetadata>();
    qpx::ImageMetadata display;
    mutable unsigned reads=0,display_reads=0,metadata_reads=0;
    mutable qpx::RawRegionRequest last;
    unsigned which=0,flip_index=std::numeric_limits<unsigned>::max();int mode=0;
    bool is_closed=false,no_raw=false,huge_name=false;std::atomic<bool>* cancel_on_read=nullptr;
    explicit Reader(unsigned index=0):which(index) {
        raw->width=5;raw->height=4;raw->channels=3;raw->z_planes=2;raw->timepoints=2;
        raw->series=1;raw->series_count=2;raw->sample_type=qpx::SampleType(raw_sampling_reference::cases[index].type);
        raw->levels={{5,4,1}};raw->channel_names={"c0","c1","c2"};raw->dimension_order="XYZCT";
        raw->pixel_width_um=.5;raw->pixel_height_um=.25;raw->z_spacing_um=1.5;raw->time_interval_seconds=2.;
        display.width=1;display.height=1;display.levels={{1,1,1}};
    }
    const qpx::ImageMetadata& metadata() const override {return display;}
    std::string reader_id() const override {return huge_name?std::string(65537,'x'):"raw-reference";}
    qpx::PixelBuffer read_region(const qpx::RegionRequest&) const override {++display_reads;throw std::runtime_error("display IO is forbidden");}
    std::shared_ptr<const qpx::RawImageMetadata> raw_metadata(const std::atomic<bool>*) const override {++metadata_reads;return no_raw?nullptr:raw;}
    qpx::RawPixelBuffer read_raw(const qpx::RawRegionRequest& request,const std::atomic<bool>*) const override {
        ++reads;last=request;
        if(mode==7) throw std::runtime_error("synthetic raw decoder failure");
        if(mode==1) return {request.region.width+1,request.region.height,request.channels,raw->sample_type};
        if(mode==2) return {request.region.width,request.region.height,request.channels+1,raw->sample_type};
        if(mode==3) return {request.region.width,request.region.height,request.channels,qpx::SampleType::Float32};
        qpx::RawPixelBuffer out(request.region.width,request.region.height,request.channels,raw->sample_type);
        auto& support=out.writable_valid();
        std::visit([&](auto& values) {
            using T=typename std::decay_t<decltype(values)>::value_type;
            for(std::int64_t y=0;y<request.region.height;++y) for(std::int64_t x=0;x<request.region.width;++x) {
                const auto sx=request.region.x+x,sy=request.region.y+y;
                require(sx>=0 && sx<5 && sy>=0 && sy<4);
                const auto pixel=unsigned(((request.t*2+request.z)*4+sy)*5+sx);
                const auto local=std::size_t(y*request.region.width+x);support[local]=1;
                for(const auto& invalid:raw_sampling_reference::invalid)
                    if(invalid.which==which && invalid.index==pixel) support[local]=0;
                for(std::uint32_t c=0;c<request.channels;++c) {
                    const auto index=pixel*3+request.first_channel+c;
                    auto value=raw_sampling_reference::banks[raw_sampling_reference::cases[which].type][index];
                    for(const auto& override_value:raw_sampling_reference::overrides)
                        if(override_value.which==which && override_value.index==index) value=override_value.raw;
                    if(index==flip_index) value^=1;
                    values[local*request.channels+c]=source_value<T>(value);
                }
            }
            if(mode==5) values.clear();
            if(mode==8) values.reserve(values.size()+1024);
        },out.writable_storage());
        if(mode==4) support.clear();
        if(mode==6) support[0]=2;
        if(mode==9) out.writable_storage()=std::vector<std::uint8_t>(out.width()*out.height()*out.channels());
        if(mode==10) raw->pixel_width_um=7.;
        if(cancel_on_read) cancel_on_read->store(true);
        return out;
    }
    void close() override {is_closed=true;}
    bool closed() const override {return is_closed;}
};
}
void check_scientific_raw_sampling() {
    using namespace qpx;
    require(std::size(raw_sampling_reference::cases)==94);
    unsigned which=0;std::size_t checked=0;
    for(const auto& c:raw_sampling_reference::cases) {
        Reader reader(which);const ScientificGrid grid(5,4,{c.x,c.w,c.nx,c.cx},{c.y,c.h,c.ny,c.cy});
        ScientificRawOptions options;options.first_channel=c.first;options.channels=c.channels;options.z=c.z;options.t=c.t;
        const auto expected_work=scientific_raw_work_bytes(*reader.raw,grid,options);
        auto result=read_scientific_raw(reader,grid,options);
        const auto reads_before_digest=reader.reads;
        auto digest=digest_scientific_raw(reader,grid,options);
        require(digest.decoded_source_sha256==c.decoded && digest.input_sha256==c.input && digest.reader==result.reader());
        require(digest.metadata.sample_type==result.metadata().sample_type && digest.metadata.pixel_width_um==.5);
        require(digest.estimated_work_bytes<expected_work);
        require(reader.reads==2*reads_before_digest);
        reader.reads=reads_before_digest;
        require(result.estimated_work_bytes()==expected_work && expected_work<=options.work_bytes);
        const auto b=grid.clipped_read_bounds();
        require(reader.display_reads==0 && reader.reads==unsigned(b.width!=0 && b.height!=0));
        if(reader.reads) require(reader.last.region.level==0 && reader.last.region.x==b.x && reader.last.region.y==b.y &&
            reader.last.region.width==b.width && reader.last.region.height==b.height && reader.last.channels==c.channels &&
            reader.last.first_channel==c.first && reader.last.z==c.z && reader.last.t==c.t);
        require(result.width()==c.nx+2*c.cx && result.height()==c.ny+2*c.cy && result.channels()==c.channels);
        require(result.metadata().sample_type==SampleType(c.type) && result.metadata().series==1 && result.metadata().series_count==2);
        if(result.decoded_source_sha256()!=c.decoded) throw std::runtime_error(std::string(c.name)+": decoded hash differs: "+result.decoded_source_sha256()+" expected "+c.decoded);
        if(result.input_sha256()!=c.input) throw std::runtime_error(std::string(c.name)+": input hash differs: "+result.input_sha256()+" expected "+c.input);
        for(std::size_t i=0;i<result.values().size();++i) {
            const auto reason=raw_sampling_reference::reasons[c.offset+i];
            if(bits(result.values()[i])!=raw_sampling_reference::expected[c.offset+i]) {
                std::ostringstream message;message<<c.name<<": sample "<<i<<" expected "<<std::hex<<raw_sampling_reference::expected[c.offset+i]<<" actual "<<bits(result.values()[i]);
                throw std::runtime_error(message.str());
            }
            require(result.reasons()[i]==reason && result.valid()[i]==std::uint8_t(reason==0));++checked;
        }
        reader.close();require(result.input_sha256()==c.input && result.metadata().pixel_width_um==.5);
        ++which;
    }
    require(checked==3732);
    Reader reader(16);const ScientificGrid grid(5,4,{0,5,3},{0,4,3});ScientificRawOptions options;
    std::atomic<bool> cancelled{true};
    rejects<std::runtime_error>([&]{read_scientific_raw(reader,grid,options,&cancelled);});require(reader.reads==0 && reader.metadata_reads==0);
    cancelled.store(false);reader.cancel_on_read=&cancelled;
    rejects<std::runtime_error>([&]{read_scientific_raw(reader,grid,options,&cancelled);});require(reader.reads==1);
    reader.cancel_on_read=nullptr;cancelled.store(false);
    for(int mode=1;mode<=9;++mode) {
        reader.mode=mode;
        if(mode==8) {
            rejects<std::length_error>([&]{read_scientific_raw(reader,grid);});
            rejects<std::length_error>([&]{digest_scientific_raw(reader,grid);});
        } else {
            rejects<std::runtime_error>([&]{read_scientific_raw(reader,grid);});
            rejects<std::runtime_error>([&]{digest_scientific_raw(reader,grid);});
        }
    }
    reader.mode=0;const auto previous=reader.reads;
    options.channels=0;rejects<std::out_of_range>([&]{read_scientific_raw(reader,grid,options);});
    options.channels=1;options.first_channel=3;rejects<std::out_of_range>([&]{read_scientific_raw(reader,grid,options);});
    options.first_channel=0;options.z=2;rejects<std::out_of_range>([&]{read_scientific_raw(reader,grid,options);});
    options.z=0;options.t=2;rejects<std::out_of_range>([&]{read_scientific_raw(reader,grid,options);});
    options.t=0;options.work_bytes=scientific_raw_work_bytes(*reader.raw,grid,options)-1;
    rejects<std::length_error>([&]{read_scientific_raw(reader,grid,options);});
    options.work_bytes=256*1024*1024;reader.raw->levels[0].downsample=1.5;
    rejects<std::invalid_argument>([&]{read_scientific_raw(reader,grid,options);});reader.raw->levels[0].downsample=1;
    reader.raw->series_count=1;rejects<std::invalid_argument>([&]{read_scientific_raw(reader,grid,options);});reader.raw->series_count=2;
    reader.huge_name=true;rejects<std::length_error>([&]{read_scientific_raw(reader,grid,options);});reader.huge_name=false;
    reader.no_raw=true;rejects<std::invalid_argument>([&]{read_scientific_raw(reader,grid,options);});reader.no_raw=false;
    ScientificRawOptions many;many.channels=3;
    rejects<std::length_error>([&]{read_scientific_raw(reader,ScientificGrid(5,4,{0,5,1000000},{0,4,2}),many);});
    require(reader.reads==previous);
    reader.close();rejects<std::runtime_error>([&]{read_scientific_raw(reader,grid);});require(reader.reads==previous);
    unsigned pair=0;while(pair<std::size(raw_sampling_reference::cases) && std::string(raw_sampling_reference::cases[pair].name)!="u64-integer-pair") ++pair;
    require(pair<std::size(raw_sampling_reference::cases));
    Reader values(pair);const ScientificGrid pair_grid(5,4,{0,2,1},{0,1,1});
    auto a=read_scientific_raw(values,pair_grid);values.flip_index=3;auto b=read_scientific_raw(values,pair_grid);
    require(a.values()==b.values() && a.decoded_source_sha256()!=b.decoded_source_sha256() && a.input_sha256()!=b.input_sha256());
    values.flip_index=std::numeric_limits<unsigned>::max();values.raw->pixel_width_um=1.;auto c=read_scientific_raw(values,pair_grid);
    require(a.decoded_source_sha256()==c.decoded_source_sha256() && a.input_sha256()!=c.input_sha256() && a.metadata().pixel_width_um==.5);
    values.raw->pixel_width_um.reset();auto unknown=read_scientific_raw(values,pair_grid);require(!unknown.metadata().pixel_width_um);
    // A decoder changing its exposed metadata cannot alter the captured epoch.
    Reader epoch;const ScientificGrid epoch_grid(5,4,{0,5,3},{0,4,3});
    const auto epoch_expected=digest_scientific_raw(epoch,epoch_grid);
    epoch.mode=10;auto epoch_result=read_scientific_raw(epoch,epoch_grid);
    require(epoch.raw->pixel_width_um==7. && epoch_result.metadata().pixel_width_um==.5 && epoch_result.input_sha256()==epoch_expected.input_sha256);
    epoch.raw->pixel_width_um=.5;
    auto epoch_digest=digest_scientific_raw(epoch,epoch_grid);
    require(epoch_digest.metadata.pixel_width_um==.5 && epoch_digest.input_sha256==epoch_expected.input_sha256);
    epoch.mode=0;epoch.raw->pixel_width_um=.5;
    ScientificRawOptions tight;tight.work_bytes=epoch_expected.estimated_work_bytes;
    require(digest_scientific_raw(epoch,epoch_grid,tight).input_sha256==epoch_expected.input_sha256);
    const auto before_reject=epoch.reads;
    rejects<std::length_error>([&]{read_scientific_raw(epoch,epoch_grid,tight);});
    require(epoch.reads==before_reject);
    --tight.work_bytes;rejects<std::length_error>([&]{digest_scientific_raw(epoch,epoch_grid,tight);});
    require(epoch.reads==before_reject);
    cancelled.store(true);const auto metadata_before=epoch.metadata_reads;
    rejects<std::runtime_error>([&]{digest_scientific_raw(epoch,epoch_grid,{},&cancelled);});
    require(epoch.reads==before_reject && epoch.metadata_reads==metadata_before);
    cancelled.store(false);epoch.cancel_on_read=&cancelled;
    rejects<std::runtime_error>([&]{digest_scientific_raw(epoch,epoch_grid,{},&cancelled);});
    epoch.cancel_on_read=nullptr;epoch.close();
    rejects<std::runtime_error>([&]{digest_scientific_raw(epoch,epoch_grid);});
    std::cout<<"Typed raw hash-only identity, lower memory admission and pre-decoder metadata epoch passed\n";
    std::cout<<"94 independent typed raw grid references, 3732 channel results and canonical source/input hashes passed\n";
    std::cout<<"Native typed scientific raw sampling, per-channel finite/support, source-bit provenance and pre-IO admission passed\n";
    const auto environment=build_environment();
    require(environment && environment->source_tree_sha256.size()==64 && !environment->git_revision.empty());
    std::cout<<"Typed raw sampling engine identity "<<environment->source_tree_sha256<<" "<<environment->git_revision<<"\n";
}
