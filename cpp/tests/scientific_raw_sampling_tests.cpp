#include "qupathxx/scientific_raw_sampling.hpp"
#include "qupathxx/provenance.hpp"
#include "qupathxx/raw_analysis.hpp"
#include "qupathxx/source_identity.hpp"
#include "raw_grid_statistics_reference.hpp"
#include "raw_grid_archive_reference.hpp"
#include "../raw_analysis_internal.hpp"
#include "../sha256.hpp"
#include "qupathxx/raw_tables.hpp"
#include "raw_scientific_sampling_reference.hpp"
#include "raw_streaming_reference.hpp"
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
class Reader:public qpx::ImageServer {
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
class AnalysisReader final:public Reader {
    std::shared_ptr<qpx::ReaderSource> observed,cold;
public:
    bool lazy_identity=false;
    std::shared_ptr<qpx::TiffView> view=std::make_shared<qpx::TiffView>();
    explicit AnalysisReader(unsigned which=0):Reader(which),observed(std::make_shared<qpx::ReaderSource>()) {
        display.width=5;display.height=4;display.levels={{5,4,1}};
        display.pixel_width_um=.5;display.pixel_height_um=.25;view->series=1;
        auto file=std::make_shared<qpx::FileVersion>();file->path="/raw-analysis-synthetic.tif";
        file->identity_scheme="posix.device-inode/1";file->identity="1:2";
        file->stamp_scheme="posix.seconds-nanoseconds/1";file->modified_token="1:0";file->change_token="2:0";file->bytes=100;
        observed->generation="raw-analysis-synthetic";observed->reader="tiff:synthetic";observed->locator=file->path;
        observed->scope="single_file";observed->primary=file;observed->validate();
        cold=std::make_shared<qpx::ReaderSource>(*observed);cold->primary.reset();
    }
    std::string source_locator() const override {return observed->locator;}
    std::shared_ptr<const qpx::ReaderSource> reader_source() const override {return lazy_identity && !metadata_reads?cold:observed;}
    std::shared_ptr<const qpx::TiffView> tiff_view() const override {return view;}
    void set_companion_stamp(unsigned stamp=1) {
        auto next=std::make_shared<qpx::ReaderSource>(*observed);auto companion=*observed->primary;
        companion.path="/raw-analysis-companion.tif";companion.identity="1:3";companion.modified_token=std::to_string(stamp)+":0";
        next->scope="compound_files";next->companions={std::move(companion)};next->validate();observed=std::move(next);
    }
};

class StreamingReader:public qpx::ImageServer {
public:
    std::shared_ptr<qpx::RawImageMetadata> raw=std::make_shared<qpx::RawImageMetadata>();
    qpx::ImageMetadata display;
    mutable unsigned reads=0,metadata_reads=0;
    mutable std::size_t peak_payload=0;
    int fault=0;bool mutate_epoch=false,patterned_support=true;
    unsigned change_after=0;bool change_support=false,change_values=true;std::atomic<bool>* cancel_after_capture=nullptr;
    std::atomic<bool>* cancel_on_second=nullptr;
    StreamingReader(std::int64_t w,std::int64_t h) {
        raw->width=w;raw->height=h;raw->channels=3;raw->sample_type=qpx::SampleType::UInt64;
        raw->levels={{w,h,1}};raw->channel_names={"c0","c1","c2"};raw->dimension_order="XYC";
        raw->pixel_width_um=.5;raw->pixel_height_um=.25;
        display.width=w;display.height=h;display.levels={{w,h,1}};
    }
    const qpx::ImageMetadata& metadata() const override {return display;}
    std::string reader_id() const override {return "stream-reference";}
    qpx::PixelBuffer read_region(const qpx::RegionRequest&) const override {throw std::runtime_error("display IO forbidden");}
    std::shared_ptr<const qpx::RawImageMetadata> raw_metadata(const std::atomic<bool>*) const override {++metadata_reads;return raw;}
    qpx::RawPixelBuffer read_raw(const qpx::RawRegionRequest& r,const std::atomic<bool>*) const override {
        ++reads;const auto& b=r.region;require(b.level==0 && b.x>=0 && b.y>=0 && b.x+b.width<=raw->width && b.y+b.height<=raw->height);
        if(reads==2 && fault==1) throw std::runtime_error("later decoder failure");
        if(reads==2 && fault==4) return {b.width+1,b.height,r.channels,qpx::SampleType::UInt64};
        qpx::RawPixelBuffer out(b.width,b.height,r.channels,qpx::SampleType::UInt64);
        auto& data=std::get<std::vector<std::uint64_t>>(out.writable_storage());
        for(std::int64_t y=0;y<b.height;++y) for(std::int64_t x=0;x<b.width;++x) {
            const auto sx=b.x+x,sy=b.y+y,pixel=y*b.width+x;
            out.writable_valid()[std::size_t(pixel)]=std::uint8_t(!patterned_support || (sx+sy)%19!=0);
            for(std::uint32_t c=0;c<r.channels;++c)
                data[std::size_t(pixel)*r.channels+c]=(1ULL<<63)+std::uint64_t((sy*raw->width+sx)*3+r.first_channel+c)*7+1;
        }
        if(change_values && change_after && reads>change_after) {
            if(change_support) out.writable_valid()[0]^=1;else data[0]^=1;
        }
        if(cancel_after_capture && reads>change_after) cancel_after_capture->store(true);
        peak_payload=std::max(peak_payload,out.payload_bytes());
        if(reads==2 && fault==2) out.writable_valid()[0]=2;
        if(reads==2 && fault==3) data.reserve(data.size()+1024);
        if(reads==2 && fault==5) out.writable_storage()=std::vector<std::uint8_t>(data.size());
        if(reads==2 && cancel_on_second) cancel_on_second->store(true);
        if(mutate_epoch) raw->pixel_width_um=7.;
        return out;
    }
    void close() override {}bool closed() const override {return false;}
};
void check_raw_streaming() {
    using namespace qpx;
    for(const auto& reference:raw_streaming_reference::cases) {
        const ScientificGrid grid(reference.width,reference.height,{0,reference.width,7},{0,reference.height,5});
        ScientificRawOptions options;options.first_channel=1;options.channels=2;
        StreamingReader reader(reference.width,reference.height);reader.mutate_epoch=true;
        const auto digest=digest_scientific_raw(reader,grid,options);
        require(reader.reads>1 && reader.metadata_reads==1 && reader.peak_payload<=65536);
        require(digest.decoded_source_sha256==reference.decoded && digest.input_sha256==reference.input);
        require(digest.metadata.pixel_width_um==.5 && reader.raw->pixel_width_um==7.);
        StreamingReader full(reference.width,reference.height);
        const auto sampled=read_scientific_raw(full,grid,options);
        require(full.reads>1 && full.peak_payload<=65536 && sampled.input_sha256()==digest.input_sha256 && sampled.decoded_source_sha256()==digest.decoded_source_sha256);
        require(digest.estimated_work_bytes<sampled.estimated_work_bytes());
        options.work_bytes=digest.estimated_work_bytes;
        StreamingReader exact(reference.width,reference.height);
        require(digest_scientific_raw(exact,grid,options).input_sha256==digest.input_sha256);
        StreamingReader rejected(reference.width,reference.height);
        --options.work_bytes;rejects<std::length_error>([&]{digest_scientific_raw(rejected,grid,options);});require(rejected.reads==0);
        ++options.work_bytes;rejects<std::length_error>([&]{read_scientific_raw(rejected,grid,options);});require(rejected.reads==0);
        options.work_bytes=256*1024*1024;
        for(int fault=1;fault<=5;++fault) {
            StreamingReader broken(reference.width,reference.height);broken.fault=fault;
            rejects<std::exception>([&]{digest_scientific_raw(broken,grid,options);});require(broken.reads==2);
        }
        StreamingReader cancelled(reference.width,reference.height);std::atomic<bool> stop{false};cancelled.cancel_on_second=&stop;
        rejects<std::runtime_error>([&]{digest_scientific_raw(cancelled,grid,options,&stop);});require(cancelled.reads==2);
    }
    for(const auto& reference:raw_streaming_reference::cases) {
        const ScientificGrid grid(reference.width,reference.height,{0,reference.width,7},{0,reference.height,5});
        ScientificRawOptions options;options.first_channel=1;options.channels=2;
        StreamingReader reader(reference.width,reference.height);reader.patterned_support=false;
        const auto result=read_scientific_raw(reader,grid,options);
        require(reader.metadata_reads==1 && reader.peak_payload<=65536);
        require(result.input_sha256()==reference.valid_input && result.decoded_source_sha256()==reference.valid_decoded);
        require(result.values().size()==70);
        for(std::size_t i=0;i<70;++i) require(bits(result.values()[i])==reference.means[i] && result.valid()[i]==1 && result.reasons()[i]==0);
        StreamingReader baseline(reference.width,reference.height);baseline.patterned_support=false;
        const auto digest=digest_scientific_raw(baseline,grid,options);require(reader.reads>baseline.reads);
        options.work_bytes=result.estimated_work_bytes();StreamingReader exact(reference.width,reference.height);exact.patterned_support=false;
        require(read_scientific_raw(exact,grid,options).input_sha256()==result.input_sha256());
        --options.work_bytes;StreamingReader refused(reference.width,reference.height);
        rejects<std::length_error>([&]{read_scientific_raw(refused,grid,options);});require(refused.reads==0);
        options.work_bytes=256*1024*1024;
        for(bool support:{false,true}) {
            StreamingReader changing(reference.width,reference.height);changing.patterned_support=false;changing.change_after=baseline.reads;changing.change_support=support;
            rejects<std::runtime_error>([&]{read_scientific_raw(changing,grid,options);});require(changing.reads==baseline.reads+1);
        }
        StreamingReader cancelled(reference.width,reference.height);cancelled.patterned_support=false;cancelled.change_after=baseline.reads;cancelled.change_values=false;
        std::atomic<bool> stop{false};cancelled.cancel_after_capture=&stop;
        rejects<std::runtime_error>([&]{read_scientific_raw(cancelled,grid,options,&stop);});require(cancelled.reads==baseline.reads+1);
        const ScientificGrid dense(reference.width,reference.height,{0,reference.width,128},{0,reference.height,128});
        StreamingReader compatible(reference.width,reference.height);compatible.patterned_support=false;
        const auto dense_result=read_scientific_raw(compatible,dense,options);
        require(compatible.reads==1 && bits(dense_result.values()[0])==reference.dense_first);
        require(dense_result.decoded_source_sha256()==reference.valid_decoded);
    }
    std::cout<<"Streamed raw materialization exact means, pinned chunk bits/support, cancellation, admission and dense fallback passed\n";
    std::cout<<"Streamed raw digest canonical HWC/validity hashes, bounded chunks, metadata epoch, late faults and admission passed\n";
}

}
void check_scientific_raw_sampling() {
    using namespace qpx;
    check_raw_streaming();
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

void check_scientific_raw_analysis() {
    using namespace qpx;
    require(std::size(raw_grid_statistics_reference::cases)==188);
    for(bool explicit_grid:{false,true}) {
        auto source=std::make_shared<AnalysisReader>();source->set_companion_stamp();Document doc(source);
        RawRegionRequest region;region.region=RegionRequest(0,0,0,5,4);region.channels=2;
        ScientificRawOptions options;options.channels=2;
        RawAnalysisRequest request=explicit_grid?RawAnalysisRequest(doc,ScientificGrid(5,4,{0,5,3,0},{0,4,2,0}),options):RawAnalysisRequest(doc,region,0,false,16);
        const auto result=analyze_raw(request);const auto id=doc.keep_raw_analysis(request,result);
        require(result->source()->companions.size()==1 && doc.raw_analysis_status(id)=="current");
        const auto bytes=RawAnalysisCodec::encode(*doc.raw_analysis_record(id),doc.metadata());require(bytes[3]==(explicit_grid?4:3));
        const auto decoded=RawAnalysisCodec::decode(bytes.data(),bytes.size(),doc.metadata());
        require(reader_source_json(*decoded->result->source())==reader_source_json(*result->source()));
        require(RawAnalysisCodec::encode(*decoded,doc.metadata())==bytes);
        auto legacy=bytes;legacy[3]=explicit_grid?2:1;Sha256 checksum;checksum.update(legacy.data(),legacy.size()-64);const auto hash=checksum.finish();std::copy(hash.begin(),hash.end(),legacy.end()-64);
        rejects<std::invalid_argument>([&]{RawAnalysisCodec::decode(legacy.data(),legacy.size(),doc.metadata());});
        const auto& file=result->source()->companions[0];std::size_t suffix=8+16;
        for(const auto* text:{&file.path,&file.identity_scheme,&file.identity,&file.stamp_scheme,&file.modified_token,&file.change_token}) suffix+=8+text->size();
        for(auto count:{0,64}) {auto damaged=bytes;damaged[damaged.size()-64-suffix]=std::uint8_t(count);Sha256 digest;digest.update(damaged.data(),damaged.size()-64);const auto sum=digest.finish();std::copy(sum.begin(),sum.end(),damaged.end()-64);rejects<std::invalid_argument>([&]{RawAnalysisCodec::decode(damaged.data(),damaged.size(),doc.metadata());});}
        source->set_companion_stamp(7);require(doc.raw_analysis_status(id)=="source_changed");
        require(result->source()->companions[0].modified_token=="1:0");
        auto replay=RawAnalysisRequest::replay(doc,id);const auto repeated=analyze_raw(replay);
        require(doc.verify_raw_analysis(id,replay,repeated) && doc.raw_analysis_status(id)=="current");
    }
    std::cout<<"Compound TIFF observations, QRA3/QRA4 retention, downgrade refusal and companion-only stale status passed\n";
    {
        auto source=std::make_shared<AnalysisReader>();Document doc(source);
        const auto id=doc.add_rectangle({.2,.3,2.1,1.1},"fractional ROI");
        RawGridSampling sampling;sampling.samples_x=3;sampling.samples_y=2;sampling.context_x=1;sampling.context_y=2;
        const auto request=RawAnalysisRequest::for_grid_roi(doc,id,sampling);
        require(source->reads==0 && source->metadata_reads==0 && request.grid());
        require(request.grid()->x().origin()==0 && request.grid()->x().extent()==3 && request.grid()->y().origin()==0 && request.grid()->y().extent()==2);
        const auto result=analyze_raw(request);require(result->statistics().pixels==2 && result->statistics().means[0]==.5);
        rejects<std::invalid_argument>([&]{RawAnalysisRequest::for_grid_roi(doc,id,sampling,{},true);});
        sampling.samples_x=0;rejects<std::invalid_argument>([&]{RawAnalysisRequest::for_grid_roi(doc,id,sampling);});
        rejects<std::invalid_argument>([&]{doc.add_rectangle({-1,0,2,2});});
        const auto outside_result=analyze_raw(RawAnalysisRequest(doc,
            ScientificGrid(5,4,{-1,2,3,1},{0,2,2,2})));
        require(outside_result->statistics().pixels==6 && outside_result->statistics().valid_pixels==2 && outside_result->statistics().means[0]==64.);
    }
    std::cout<<"Shared ROI grid raw count/context planning and pre-IO admission passed\n";

    {
        auto source=std::make_shared<AnalysisReader>();Document doc(source);
        const auto object=doc.add_rectangle({0,0,5,4},"captured workspace");
        const ScientificGrid grid(5,4,{0,5,16,64},{0,4,12,64});ScientificRawOptions options;
        const auto initial=RawAnalysisRequest(doc,grid,options,object);
        const auto reserve=raw_grid_analysis_reserve(grid,options,true,initial.mask(),initial.object_context(),source->reader_source().get(),1);
        const auto hash_reserve=raw_grid_analysis_reserve(grid,options,false,initial.mask(),initial.object_context(),source->reader_source().get(),1);
        const auto required=detail::scientific_raw_reserved_work_bytes(*source->raw,grid,options,true,reserve);
        const auto hash_required=detail::scientific_raw_reserved_work_bytes(*source->raw,grid,options,false,hash_reserve);
        require(required>hash_required && reserve.fixed_bytes>hash_reserve.fixed_bytes);
        // The hash budget must not silently allocate sampled outputs or means.
        options.work_bytes=hash_required;
        rejects<std::length_error>([&]{analyze_raw(RawAnalysisRequest(doc,grid,options,object));});
        require(source->reads==0);
        const auto hashed=raw_analysis_input_digest(RawAnalysisRequest(doc,grid,options,object));
        require(source->reads==1 && source->metadata_reads==2);
        options.work_bytes=required-1;
        rejects<std::length_error>([&]{analyze_raw(RawAnalysisRequest(doc,grid,options,object));});require(source->reads==1);
        options.work_bytes=required;const auto before_metadata=source->metadata_reads;
        const auto result=analyze_raw(RawAnalysisRequest(doc,grid,options,object));
        require(source->reads==2 && source->metadata_reads==before_metadata+1 && result->input_sha256()==hashed.input_sha256);
        const auto base_reserve=raw_grid_analysis_reserve(grid,options,true,{},initial.object_context(),source->reader_source().get(),1);
        require(reserve.fixed_bytes-base_reserve.fixed_bytes==8*initial.mask()->payload_bytes());
        // Changed reader axes reject against this one metadata epoch before IO.
        source->raw->series=0;
        rejects<std::invalid_argument>([&]{analyze_raw(initial);});require(source->reads==2);
    }
    {
        auto source=std::make_shared<AnalysisReader>();source->lazy_identity=true;Document doc(source);
        const auto object=doc.add_rectangle({0,0,5,4});RawGridSampling sampling;sampling.samples_x=sampling.samples_y=1;
        const auto request=RawAnalysisRequest::for_grid_roi(doc,object,sampling);
        require(!source->reader_source()->primary && source->metadata_reads==0 && source->reads==0);
        const auto result=analyze_raw(request);
        require(source->metadata_reads==1 && source->reads==1 && result->source()->primary);
        auto hashed_source=std::make_shared<AnalysisReader>();hashed_source->lazy_identity=true;Document other(hashed_source);
        const auto id=other.add_rectangle({0,0,5,4});
        const auto digest=raw_analysis_input_digest(RawAnalysisRequest::for_grid_roi(other,id,sampling));
        require(hashed_source->metadata_reads==1 && hashed_source->reads==1 && digest.input_sha256==result->input_sha256());
    }
    std::cout<<"Raw grid captured ROI/context workspace, exact budget boundary, hash-only lower admission and single metadata epoch passed\n";

    RawRegionStatistics positive;positive.pixels=positive.valid_pixels=1;positive.finite_samples={1};positive.nonfinite_samples={0};positive.means={0.};positive.minima={0.};positive.maxima={0.};
    auto negative=positive;negative.means[0]=-0.;require(!same_raw_statistics(positive,negative,true) && same_raw_statistics(positive,negative,false));
    negative=positive;negative.minima[0]=-0.;require(!same_raw_statistics(positive,negative,true));
    negative=positive;negative.maxima[0]=-0.;require(!same_raw_statistics(positive,negative,true));

    for(const auto& reference:raw_grid_statistics_reference::cases) {
        const auto& c=raw_sampling_reference::cases[reference.source];
        auto reader=std::make_shared<AnalysisReader>(reference.source);Document document(reader);
        const auto object=reference.roi?document.add_rectangle({1,1,3,2},"captured ROI"):0;
        const ScientificGrid grid(5,4,{c.x,c.w,c.nx,c.cx},{c.y,c.h,c.ny,c.cy});
        ScientificRawOptions options;options.first_channel=c.first;options.channels=c.channels;options.z=c.z;options.t=c.t;
        RawAnalysisRequest request(document,grid,options,object);
        require(reader->metadata_reads==0 && reader->reads==0 && request.matches(document));
        rejects<std::logic_error>([&]{request.region();});
        const auto result=analyze_raw(request);const auto& stats=result->statistics();
        require(result->grid() && result->grid()->source_metadata.sample_type==SampleType(c.type));
        require(result->grid()->sampling_input_sha256==c.input && result->grid()->decoded_source_sha256==c.decoded);
        require(result->algorithm()=="qpx.raw-grid-statistics/1" && std::isnan(result->metadata().downsample));
        require(stats.pixels==reference.pixels && stats.valid_pixels==reference.valid && stats.means.size()==c.channels);
        for(unsigned channel=0;channel<c.channels;++channel) {
            const auto& expected=raw_grid_statistics_reference::channels[reference.offset+channel];
            if(stats.finite_samples[channel]!=expected.finite || stats.nonfinite_samples[channel]!=expected.nonfinite)
                throw std::runtime_error(std::string(c.name)+": derived statistics counts differ");
            require(stats.means[channel].has_value()==expected.present && stats.minima[channel].has_value()==expected.present && stats.maxima[channel].has_value()==expected.present);
            if(expected.present) require(bits(*stats.means[channel])==expected.mean && bits(*stats.minima[channel])==expected.minimum && bits(*stats.maxima[channel])==expected.maximum);
        }
        const auto digest=raw_analysis_input_digest(request);const auto b=grid.clipped_read_bounds();
        require(digest.input_sha256==result->input_sha256() && digest.tiles==result->tiles() && digest.raw_bytes==std::uint64_t(b.width)*b.height*(std::uint64_t(c.channels)*sample_bytes(SampleType(c.type))+1));
        require(reader->display_reads==0);
        const auto retained=document.keep_raw_analysis(request,result);
        require(document.raw_analysis_status(retained)=="current");
        const auto record=document.raw_analysis_record(retained);
        const auto bytes=RawAnalysisCodec::encode(*record,document.metadata());require(bytes[3]==2);
        const auto decoded=RawAnalysisCodec::decode(bytes.data(),bytes.size(),document.metadata());
        require(decoded->result->grid() && decoded->result->grid()->reader=="raw-reference" && decoded->result->input_sha256()==result->input_sha256());
        require(RawAnalysisCodec::encode(*decoded,document.metadata())==bytes);
        if(object) {
            // A trusted result cannot authenticate a changed public record mask.
            auto changed=*record;changed.mask=record->mask->translated(.1,0);
            rejects<std::invalid_argument>([&]{changed.validate(document.metadata());});
            changed=*decoded;changed.mask=decoded->mask->translated(.1,0);
            rejects<std::invalid_argument>([&]{changed.validate(document.metadata());});
        }
        std::atomic<bool> cancel_validation{true};
        rejects<std::runtime_error>([&]{record->validate(document.metadata(),&cancel_validation);});
        auto damaged=bytes;damaged.back()^=1;rejects<std::invalid_argument>([&]{RawAnalysisCodec::decode(damaged.data(),damaged.size(),document.metadata());});
        damaged=bytes;damaged[4]^=1;Sha256 rehash;rehash.update(damaged.data(),damaged.size()-64);const auto checksum=rehash.finish();std::copy(checksum.begin(),checksum.end(),damaged.end()-64);
        rejects<std::exception>([&]{RawAnalysisCodec::decode(damaged.data(),damaged.size(),document.metadata());});
        const auto replay=RawAnalysisRequest::replay(document,retained);
        require(document.verify_raw_analysis(retained,replay,analyze_raw(replay)));
        Document copy=document;require(copy.raw_analysis_status(retained)=="unverified");
        RawTableRequest table(document,{retained},true);const auto csv=raw_analysis_csv(table);
        require(table.rows()==c.channels && csv.find("qpx.raw-analysis-csv/2")!=std::string::npos && csv.find("explicit_grid,float64")!=std::string::npos && csv.find(",nan,")==std::string::npos);

        rejects<std::logic_error>([&]{result->region();});
        if(object) require(result->object_context().name=="captured ROI" && result->object_id()==object);
    }
    {
        auto source=std::make_shared<AnalysisReader>();Document doc(source);
        for(const auto& fixture:{std::make_pair(raw_grid_archive_reference::legacy,sizeof(raw_grid_archive_reference::legacy)),std::make_pair(raw_grid_archive_reference::grid,sizeof(raw_grid_archive_reference::grid))}) {
            const auto decoded=RawAnalysisCodec::decode(fixture.first,fixture.second,doc.metadata());
            const auto encoded=RawAnalysisCodec::encode(*decoded,doc.metadata());
            require(encoded.size()==fixture.second && std::equal(encoded.begin(),encoded.end(),fixture.first));
            RawAnalysisRequest captured=decoded->result->grid()?RawAnalysisRequest(doc,decoded->result->grid()->grid,decoded->result->grid()->options):RawAnalysisRequest(doc,decoded->result->region(),0,false,16);
            require(analyze_raw(captured)->input_sha256()==decoded->result->input_sha256());
            // A recomputed envelope checksum cannot authenticate false counts.
            const auto offset=decoded->result->grid()?raw_grid_archive_reference::grid_pixels_offset:raw_grid_archive_reference::legacy_pixels_offset;
            auto damaged=encoded;damaged.at(offset)^=1;Sha256 checksum;checksum.update(damaged.data(),damaged.size()-64);
            const auto hash=checksum.finish();std::copy(hash.begin(),hash.end(),damaged.end()-64);
            rejects<std::invalid_argument>([&]{RawAnalysisCodec::decode(damaged.data(),damaged.size(),doc.metadata());});
            if(decoded->result->grid()) {
                const auto& descriptor=*decoded->result->grid();
                const auto old_options=raw_grid_analysis_sampling_options(descriptor.source_metadata,descriptor.grid,descriptor.options,true);
                const auto legacy_required=scientific_raw_work_bytes(descriptor.source_metadata,descriptor.grid,old_options)+descriptor.options.work_bytes-old_options.work_bytes;
                auto historical=encoded;constexpr std::size_t work_offset=4+10*8;
                for(unsigned i=0;i<8;++i) historical[work_offset+i]=std::uint8_t(std::uint64_t(legacy_required)>>(8*i));
                Sha256 envelope;envelope.update(historical.data(),historical.size()-64);const auto checked=envelope.finish();std::copy(checked.begin(),checked.end(),historical.end()-64);
                const auto restored=RawAnalysisCodec::decode(historical.data(),historical.size(),doc.metadata());
                require(RawAnalysisCodec::encode(*restored,doc.metadata())==historical);
                const auto reads=source->reads;
                rejects<std::length_error>([&]{analyze_raw(RawAnalysisRequest(doc,descriptor.grid,restored->result->grid()->options));});
                require(source->reads==reads); // old history survives; new IO must meet the full ledger
            }
        }
        const RawRegionRequest region{{0,0,0,5,4},0,3,0,0};RawAnalysisRequest natural(doc,region,0,false,16);
        const auto natural_id=doc.keep_raw_analysis(natural,analyze_raw(natural));
        const auto old_csv=raw_analysis_csv(RawTableRequest(doc,{natural_id},true));
        require(old_csv.find("qpx.raw-analysis-csv/1")!=std::string::npos && old_csv.find("sampling_kind")==std::string::npos);
        ScientificRawOptions options;options.channels=3;RawAnalysisRequest grid_request(doc,ScientificGrid(5,4,{0,5,5},{0,4,4}),options);
        const auto grid_id=doc.keep_raw_analysis(grid_request,analyze_raw(grid_request));
        const auto mixed=raw_analysis_csv(RawTableRequest(doc,{natural_id,grid_id},true));
        require(mixed.find("qpx.raw-analysis-csv/1")==std::string::npos && mixed.find("natural_level,float64")!=std::string::npos && mixed.find("explicit_grid,float64")!=std::string::npos);
        RawAnalysisJob job;const auto replay=RawAnalysisRequest::replay(doc,grid_id);
        require(job.request(replay) && job.wait(doc).state=="ready" && job.verify_result(doc,grid_id));
    }
    std::cout<<"Independent Python struct/hashlib QRA1 and QRA2 bytes, mixed typed CSV and job replay passed\n";
    std::cout<<"QRA2 private eligible-count authority, changed-mask rejection, checksum-valid count corruption and historical budget compatibility passed\n";
    auto reader=std::make_shared<AnalysisReader>();Document document(reader);
    const auto object=document.add_rectangle({1,1,3,2},"object");
    const ScientificGrid grid(5,4,{0,5,3,1},{0,4,3,1});
    RawAnalysisRequest request(document,grid,{},object);
    const auto original=analyze_raw(request);reader->view->upper={9,9,9};
    require(request.matches(document) && analyze_raw(request)->input_sha256()==original->input_sha256());
    Document copied=document;require(!request.matches(copied));
    RawAnalysisJob job;require(job.request(request));require(job.wait(document).state=="ready" && job.result()->input_sha256()==original->input_sha256());
    require(job.request(request));job.cancel();require(job.wait(document).state=="cancelled" && !job.result());
    require(job.request(request));document.rename_annotation(object,"changed");require(job.wait(document).state=="stale" && !job.result());
    require(!request.matches(document));
    RawAnalysisRequest current(document,grid,{},object);document.set_calibration(1,2);require(!current.matches(document));
    auto recalibrated=analyze_raw(RawAnalysisRequest(document,grid,{},object));
    require(recalibrated->input_sha256()!=original->input_sha256() && recalibrated->grid()->sampling_input_sha256==original->grid()->sampling_input_sha256);
    const auto reads=reader->reads;
    rejects<std::length_error>([&]{RawAnalysisRequest too_many(document,grid,{},object,false,1);});
    ScientificRawOptions tiny;tiny.work_bytes=1;RawAnalysisRequest budget(document,grid,tiny,object);
    rejects<std::length_error>([&]{analyze_raw(budget);});rejects<std::length_error>([&]{raw_analysis_input_digest(budget);});
    std::atomic<bool> cancelled{true};rejects<std::runtime_error>([&]{analyze_raw(current,&cancelled);});
    require(reader->reads==reads);
    reader->close();require(!current.matches(document));rejects<std::runtime_error>([&]{analyze_raw(current);});
    std::cout<<"QRA2 raw grid retention, canonical re-encoding, semantic corruption and fresh replay passed\n";
    std::cout<<"188 independent derived raw grid statistics references, ROI/core support, hash-only digest and native jobs passed\n";
}
