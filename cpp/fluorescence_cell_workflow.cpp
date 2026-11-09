#include "qupathxx/fluorescence_cell_workflow.hpp"
#include "qupathxx/raw_analysis.hpp"
#include "qupathxx/dark_flat_workflow.hpp"
#include "qupathxx/image_data_properties.hpp"
#include "qupathxx/workflow.hpp"
#include "fluorescence_cell_internal.hpp"
#include "raw_pixel_workflow_snapshot.hpp"
#include "scientific_cell_numeric.hpp"
#include "geometry_archive.hpp"
#include "sha256.hpp"
#include <future>
#include <cstring>

namespace qpx {
namespace {
void text(Sha256& sha,const std::string& value){sha.integer(value.size());sha.update(value.data(),value.size());}
void number(Sha256& sha,double value){std::uint64_t bits;std::memcpy(&bits,&value,8);sha.integer(bits);}
void optional(Sha256& sha,std::optional<double> value){sha.integer(bool(value));if(value)number(sha,*value);}
void region_hash(Sha256& sha,const RawRegionRequest& r){for(auto v:{std::uint64_t(r.region.x),std::uint64_t(r.region.y),std::uint64_t(r.region.width),std::uint64_t(r.region.height),std::uint64_t(r.region.level),std::uint64_t(r.first_channel),std::uint64_t(r.channels),std::uint64_t(r.z),std::uint64_t(r.t)})sha.integer(v);}
std::optional<std::array<double,2>> calibration(const RawPixelSourceSnapshot& source){if(source.calibration.x && source.calibration.y)return std::array<double,2>{*source.calibration.x,*source.calibration.y};return {};}
std::string run_hash(const FluorescenceCellResult& raster,const std::string& input,const std::atomic<bool>* cancelled) {
    Sha256 sha;text(sha,"qpx.fluorescence-cell-run/1");text(sha,input);text(sha,raster.recipe_sha256);number(sha,raster.resolved_threshold);
    for(auto n:{raster.segmentation_finite_pixels,raster.segmentation_nonfinite_pixels,raster.invalid_pixels})sha.integer(n);
    sha.integer(raster.nuclei.size());for(std::size_t i=0;i<raster.nuclei.size();++i){if((i&4095)==0)workflow_checkpoint(cancelled);sha.integer(raster.nuclei[i]);sha.integer(raster.bodies[i]);}
    const auto compartment=[&](const RawCellCompartment& c){sha.integer(c.pixels);sha.integer(c.valid_pixels);sha.integer(c.channels.size());for(const auto& v:c.channels){sha.integer(v.finite_samples);sha.integer(v.nonfinite_samples);for(const auto& x:{v.sum,v.mean,v.minimum,v.maximum,v.sample_standard_deviation})optional(sha,x);}};
    sha.integer(raster.measurements.size());for(std::size_t i=0;i<raster.measurements.size();++i){workflow_checkpoint(cancelled);const auto& cell=raster.measurements[i];sha.integer(cell.label);compartment(cell.nucleus);compartment(cell.cell);sha.integer(bool(cell.cytoplasm));if(cell.cytoplasm)compartment(*cell.cytoplasm);sha.integer(raster.boundaries[i].nucleus);sha.integer(raster.boundaries[i].cell);}
    return sha.finish();
}
}
struct FluorescenceCellRequest::Data {
    const std::thread::id owner=std::this_thread::get_id();
    const std::shared_ptr<const char> capture=std::make_shared<const char>();
    RawAnalysisRequest authority;
    std::shared_ptr<ImageServer> source;
    RawRegionRequest core,region;
    FluorescenceCellOptions options;
    std::array<std::uint32_t,2> context;
    std::optional<ScientificRawLattice> lattice;
    RawPixelSourceSnapshot snapshot;
    std::optional<DarkFlatRequest> correction;
    const RawImageMetadata& analysis_metadata() const {return correction?correction->source()->metadata():snapshot.metadata;}
    std::string correction_id;
    mutable std::shared_ptr<const ImageDataProperties> correction_properties;
    std::size_t fixed=0,work=0;
    Data(const Document& doc,RawRegionRequest selection,std::uint64_t object,FluorescenceCellOptions p,std::array<std::uint32_t,2> c,const DarkFlatRequest* derived=nullptr):
        authority(doc,selection,object),source(doc.server()),core(authority.region()),region(core),options(p),context(c) {
        const auto& m=doc.metadata();const auto& r=core.region;
        if(r.level!=0 || r.x<0 || r.y<0 || r.x>=m.width || r.y>=m.height || r.width>m.width-r.x || r.height>m.height-r.y)throw std::invalid_argument("Fluorescence core crop must lie inside level zero");
        const auto x=std::max<std::int64_t>(0,r.x-context[0]),y=std::max<std::int64_t>(0,r.y-context[1]);
        const auto right=r.x+r.width+std::min<std::int64_t>(context[0],m.width-r.x-r.width),bottom=r.y+r.height+std::min<std::int64_t>(context[1],m.height-r.y-r.height);
        region.region={x,y,0,right-x,bottom-y};
        lattice.emplace(m.width,m.height,ScientificGridAxis(x,right-x,right-x),ScientificGridAxis(y,bottom-y,bottom-y));
        snapshot=raw_pixel_capture_lattice_source(doc,*lattice,options.work_bytes);
        if(object)snapshot.provenance=capture_provenance(*source,object,authority.object_revision(),authority.document_revision(),false,snapshot.calibration.x,snapshot.calibration.y);
        if(snapshot.metadata.width!=m.width || snapshot.metadata.height!=m.height || snapshot.metadata.levels.empty() || snapshot.metadata.levels[0].downsample!=1 ||
            snapshot.metadata.levels[0].width!=m.width || snapshot.metadata.levels[0].height!=m.height)throw std::invalid_argument("Fluorescence raw/display source dimensions differ");
        if(derived){
            derived->require_current(doc);correction_id=derived->recipe().sha256();
            if(load_dark_flat_recipe(doc,correction_id).sha256()!=correction_id)throw WorkflowInputChanged("Correction recipe is not retained in this image");
            correction=*derived;correction_properties=doc.image_data_properties();
        }
        region.validate(snapshot.metadata);if(options.segmentation_channel>=region.channels)throw std::out_of_range("Fluorescence segmentation channel is outside selected block");
        auto spacing=std::array<double,2>{1,1};if(options.units==FluorescenceCellUnits::Micrometres){const auto scale=calibration(snapshot);if(!scale)throw std::invalid_argument("Physical fluorescence cells require both calibration axes");spacing=*scale;}
        const auto sigmas=cell_detail::PhysicalGridMetric(spacing[0],spacing[1]).sigma_pixels(options.sigma);
        const auto rx=cell_detail::scientific_gaussian_radius(sigmas.first),ry=cell_detail::scientific_gaussian_radius(sigmas.second);
        if(context[0]<std::max(std::min<std::int64_t>(r.x,rx),std::min<std::int64_t>(m.width-r.x-r.width,rx)) ||
            context[1]<std::max(std::min<std::int64_t>(r.y,ry),std::min<std::int64_t>(m.height-r.y-r.height,ry)))throw std::invalid_argument("Fluorescence context does not cover Gaussian support");
        fixed=sizeof(Data)+2*raw_pixel_source_snapshot_bytes(snapshot)+2*authority.object_context().payload_bytes()+32768;
        if(authority.mask())fixed+=3*authority.mask()->payload_bytes();
        if(correction)fixed+=correction->recipe().payload_bytes()+correction->source()->payload_bytes()+65536;
        work=detail::fluorescence_cell_work_bytes(region.region.width,region.region.height,region.channels,analysis_metadata().sample_type,options,calibration(snapshot),0,0,fixed);
        if(correction){auto read_peak=fixed;workflow_capture_charge(read_peak,correction->source()->read_work_bytes(region),options.work_bytes);work=std::max(work,read_peak);}
    }
};
FluorescenceCellRequest::FluorescenceCellRequest(const Document& doc,RawRegionRequest region,std::uint64_t object,FluorescenceCellOptions p,std::array<std::uint32_t,2> context) {
    p.validate();if(region.region.level!=0 || context[0]>4096 || context[1]>4096)throw std::invalid_argument("Fluorescence cells require level zero and context<=4096");
    data_=std::make_shared<const Data>(doc,region,object,p,context);if(!matches(doc))throw WorkflowInputChanged("Fluorescence input changed during capture");
}
FluorescenceCellRequest::FluorescenceCellRequest(const Document& doc,const DarkFlatRequest& correction,RawRegionRequest region,std::uint64_t object,FluorescenceCellOptions p,std::array<std::uint32_t,2> context){
    p.validate();if(region.region.level!=0 || context[0]>4096 || context[1]>4096)throw std::invalid_argument("Fluorescence cells require level zero and context<=4096");
    data_=std::make_shared<const Data>(doc,region,object,p,context,&correction);if(!matches(doc))throw WorkflowInputChanged("Corrected fluorescence input changed during capture");
}
bool FluorescenceCellRequest::matches(const Document& doc) const {
    workflow_require_owner(data_->owner);if(!data_->authority.matches(doc) || !raw_pixel_source_matches(*doc.server(),data_->snapshot))return false;
    if(data_->correction){if(!data_->correction->matches(doc))return false;
        if(data_->correction_properties!=doc.image_data_properties()){
            try{if(load_dark_flat_recipe(doc,data_->correction_id).sha256()!=data_->correction_id)return false;data_->correction_properties=doc.image_data_properties();}
            catch(const std::invalid_argument&){return false;}catch(const std::out_of_range&){return false;}
        }
    }return true;
}
bool FluorescenceCellRequest::publication_matches(const Document& doc) const {
    if(!matches(doc) || !raw_pixel_publication_lattice_source_matches(*doc.server(),data_->snapshot,*data_->lattice))return false;
    if(data_->correction){try{data_->correction->source()->require_current();}catch(const std::runtime_error&){return false;}catch(const std::invalid_argument&){return false;}}return true;
}
const RawRegionRequest& FluorescenceCellRequest::region() const{return data_->region;}
const RawRegionRequest& FluorescenceCellRequest::core_region() const{return data_->core;}
const std::optional<Roi>& FluorescenceCellRequest::roi() const{return data_->authority.mask();}
const FluorescenceCellOptions& FluorescenceCellRequest::options() const{return data_->options;}
const RawImageMetadata& FluorescenceCellRequest::source_metadata() const{return data_->analysis_metadata();}
std::array<std::uint32_t,2> FluorescenceCellRequest::context() const{return data_->context;}
std::uint64_t FluorescenceCellRequest::object_id() const{return data_->authority.object_id();}
std::size_t FluorescenceCellRequest::estimated_work_bytes() const{return data_->work;}
struct FluorescenceCellRun::Data {
    FluorescenceCellResult raster;
    RawRegionRequest region,core;
    RawImageMetadata metadata;
    std::optional<Roi> roi;
    std::shared_ptr<const AnalysisProvenance> provenance;
    std::shared_ptr<const char> capture;
    std::string input,sha,correction_id;
    double elapsed=0;
    std::array<std::uint32_t,2> context{};
};
FluorescenceCellRun::FluorescenceCellRun(std::shared_ptr<const Data> data):data_(std::move(data)){}
const FluorescenceCellResult& FluorescenceCellRun::raster() const{return data_->raster;}
const RawRegionRequest& FluorescenceCellRun::region() const{return data_->region;}
const RawRegionRequest& FluorescenceCellRun::core_region() const{return data_->core;}
const RawImageMetadata& FluorescenceCellRun::source_metadata() const{return data_->metadata;}
const std::optional<Roi>& FluorescenceCellRun::roi() const{return data_->roi;}
const std::shared_ptr<const AnalysisProvenance>& FluorescenceCellRun::provenance() const{return data_->provenance;}
const std::string& FluorescenceCellRun::input_sha256() const{return data_->input;}
const std::string& FluorescenceCellRun::sha256() const{return data_->sha;}
const std::string& FluorescenceCellRun::measurement_domain() const {static const std::string original="original-channel-samples",corrected="qpx.dark-flat-corrected-samples/1";return data_->correction_id.empty()?original:corrected;}
const std::string& FluorescenceCellRun::correction_record() const{return data_->correction_id;}
double FluorescenceCellRun::elapsed_ms() const{return data_->elapsed;}
std::array<std::uint32_t,2> FluorescenceCellRun::context() const{return data_->context;}
std::size_t FluorescenceCellRun::payload_bytes() const {
    const auto& r=data_->raster;std::size_t bytes=sizeof(*this)+sizeof(Data)+data_->metadata.payload_bytes()+data_->input.capacity()+data_->sha.capacity()+data_->correction_id.capacity()+
        r.input_sha256.capacity()+r.recipe_sha256.capacity()+(r.nuclei.capacity()+r.bodies.capacity())*sizeof(std::uint32_t)+
        r.measurements.capacity()*sizeof(RawCellMeasurements)+r.boundaries.capacity()*sizeof(FluorescenceCellBoundary)+1024;
    if(data_->roi)bytes+=data_->roi->payload_bytes();if(data_->provenance)bytes+=data_->provenance->payload_bytes();
    for(const auto& c:r.measurements){bytes+=(c.nucleus.channels.capacity()+c.cell.channels.capacity())*sizeof(RawChannelIntensity);if(c.cytoplasm)bytes+=c.cytoplasm->channels.capacity()*sizeof(RawChannelIntensity);}return bytes;
}
bool FluorescenceCellRun::matches(const FluorescenceCellRequest& request) const{return data_->capture==request.data_->capture;}
Point FluorescenceCellRun::pixel_center(std::int64_t x,std::int64_t y) const {
    if(x<0 || y<0 || x>=data_->raster.width || y>=data_->raster.height)throw std::out_of_range("Fluorescence sample index outside result");
    return {double(data_->region.region.x)+x+.5,double(data_->region.region.y)+y+.5};
}
struct FluorescenceCellKernel {
    static std::shared_ptr<const FluorescenceCellRun> run(const FluorescenceCellRequest& request,const std::atomic<bool>* cancelled,std::atomic<int>* phase=nullptr) {
        const auto began=std::chrono::steady_clock::now();workflow_checkpoint(cancelled);const auto& c=*request.data_;
        if(!raw_pixel_publication_lattice_source_matches(*c.source,c.snapshot,*c.lattice))throw WorkflowInputChanged("Fluorescence source changed before reading");
        if(phase)phase->store(int(FluorescenceCellPhase::Reading));
        auto raw=c.correction?c.correction->source()->read(c.region,c.options.work_bytes-c.fixed,cancelled):c.source->read_raw(c.region,cancelled);workflow_checkpoint(cancelled);
        if(raw.width()!=c.region.region.width || raw.height()!=c.region.region.height || raw.channels()!=c.region.channels || raw.sample_type()!=c.analysis_metadata().sample_type)throw WorkflowInputChanged("Fluorescence decoder returned another sample lattice");
        if(!raw_pixel_publication_lattice_source_matches(*c.source,c.snapshot,*c.lattice))throw WorkflowInputChanged("Fluorescence source/metadata changed during reading");
        const auto n=std::size_t(raw.width())*std::size_t(raw.height());std::vector<std::uint8_t> domain(n);
        const auto& core=c.core.region;const auto& origin=c.region.region;
        for(std::int64_t y=0;y<raw.height();++y){workflow_checkpoint(cancelled);for(std::int64_t x=0;x<raw.width();++x){if((x&4095)==0)workflow_checkpoint(cancelled);
            const auto px=origin.x+x,py=origin.y+y;domain[std::size_t(y)*std::size_t(raw.width())+std::size_t(x)]=px>=core.x && px-core.x<core.width && py>=core.y && py-core.y<core.height &&
                (!c.authority.mask() || c.authority.mask()->contains_sample({double(px)+.5,double(py)+.5},RoiSamplingPolicy::GeometryCoverage));}}
        if(phase)phase->store(int(FluorescenceCellPhase::Segmenting));
        auto output=std::make_shared<FluorescenceCellRun::Data>();output->raster=detail::segment_fluorescence_cells_reserved(raw,domain,c.options,calibration(c.snapshot),c.fixed,cancelled);
        output->region=c.region;output->core=c.core;output->roi=c.authority.mask();output->metadata=c.analysis_metadata();output->correction_id=c.correction_id;output->provenance=c.snapshot.provenance;output->capture=c.capture;output->context=c.context;
        Sha256 input;text(input,c.correction?"qpx.fluorescence-cell-source-input/2":"qpx.fluorescence-cell-source-input/1");text(input,c.snapshot.epoch);if(c.correction){text(input,c.correction_id);text(input,c.correction->source()->sha256());}text(input,output->raster.input_sha256);region_hash(input,c.region);region_hash(input,c.core);
        optional(input,c.snapshot.calibration.x);optional(input,c.snapshot.calibration.y);input.integer(bool(output->roi));
        if(output->roi){const auto bytes=GeometryArchiveCodec::encode(*output->roi);input.integer(bytes.size());input.update(bytes.data(),bytes.size());}
        output->input=input.finish();output->sha=run_hash(output->raster,output->input,cancelled);
        if(!raw_pixel_publication_lattice_source_matches(*c.source,c.snapshot,*c.lattice))throw WorkflowInputChanged("Fluorescence source changed during segmentation");
        if(c.correction)c.correction->source()->require_current(cancelled);
        workflow_checkpoint(cancelled);output->elapsed=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-began).count();
        if(phase)phase->store(int(FluorescenceCellPhase::Ready));return std::shared_ptr<const FluorescenceCellRun>(new FluorescenceCellRun(std::move(output)));
    }
};
std::shared_ptr<const FluorescenceCellRun> detect_fluorescence_cells(const FluorescenceCellRequest& request,const std::atomic<bool>* cancelled){return FluorescenceCellKernel::run(request,cancelled);}
void require_current_fluorescence_cells(const Document& doc,const FluorescenceCellRequest& request,const FluorescenceCellRun& run,const std::atomic<bool>* cancelled) {
    workflow_checkpoint(cancelled);if(!run.matches(request) || !request.publication_matches(doc))throw WorkflowInputChanged("Fluorescence cells no longer match their source/ROI/calibration");workflow_checkpoint(cancelled);
}
struct FluorescenceCellJob::Impl {
    const std::thread::id owner=std::this_thread::get_id();
    std::future<std::shared_ptr<const FluorescenceCellRun>> future;
    std::optional<FluorescenceCellRequest> request;
    std::shared_ptr<const FluorescenceCellRun> result;
    std::atomic<bool> cancelled{false};std::atomic<int> phase{0};
    FluorescenceCellStats stats;
    std::chrono::steady_clock::time_point started;
};
FluorescenceCellJob::FluorescenceCellJob():impl_(std::make_unique<Impl>()){}
FluorescenceCellJob::~FluorescenceCellJob(){impl_->cancelled.store(true);if(impl_->future.valid())impl_->future.wait();}
bool FluorescenceCellJob::busy() const {workflow_require_owner(impl_->owner);return impl_->future.valid() && impl_->future.wait_for(std::chrono::seconds(0))!=std::future_status::ready;}
void FluorescenceCellJob::cancel(){impl_->cancelled.store(true);}
void FluorescenceCellJob::await_completion(){workflow_require_owner(impl_->owner);if(impl_->future.valid())impl_->future.wait();}
bool FluorescenceCellJob::request(const FluorescenceCellRequest& request) {
    workflow_require_owner(impl_->owner);workflow_require_owner(request.data_->owner);if(busy())return false;
    if(impl_->stats.generation==UINT64_MAX)throw std::overflow_error("Fluorescence job generation exhausted");
    const auto old_cancelled=impl_->cancelled.load();const auto old_phase=impl_->phase.load();impl_->cancelled.store(false);impl_->phase.store(0);
    std::future<std::shared_ptr<const FluorescenceCellRun>> future;
    try {future=std::async(std::launch::async,[state=impl_.get(),request]{return FluorescenceCellKernel::run(request,&state->cancelled,&state->phase);});}
    catch(...){impl_->cancelled.store(old_cancelled);impl_->phase.store(old_phase);throw;}
    impl_->future=std::move(future);impl_->request=request;impl_->result.reset();impl_->started=std::chrono::steady_clock::now();
    const auto generation=impl_->stats.generation+1;impl_->stats={};impl_->stats.generation=generation;impl_->stats.state="running";return true;
}
FluorescenceCellStats FluorescenceCellJob::poll(const Document& doc) {
    workflow_require_owner(impl_->owner);auto& stats=impl_->stats;
    if(impl_->future.valid() && impl_->future.wait_for(std::chrono::seconds(0))==std::future_status::ready){
        try {auto result=impl_->future.get();
            if(impl_->cancelled.load())stats.state="cancelled";
            else if(!impl_->request->publication_matches(doc))stats.state="stale";
            else {impl_->result=std::move(result);stats.state="ready";}}
        catch(const WorkflowInputChanged& e){stats.state=impl_->cancelled.load()?"cancelled":"stale";stats.error=e.what();}
        catch(...){stats.state=impl_->cancelled.load()?"cancelled":"failed";stats.error=workflow_failure_message(std::current_exception());}
        if(stats.state!="ready"){impl_->request.reset();impl_->result.reset();}
        stats.elapsed_ms=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-impl_->started).count();
    }
    if(stats.state=="ready" && impl_->request && !impl_->request->matches(doc)){stats.state="stale";impl_->request.reset();impl_->result.reset();}
    if(impl_->future.valid())stats.elapsed_ms=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-impl_->started).count();
    stats.phase=static_cast<FluorescenceCellPhase>(impl_->phase.load());return stats;
}
FluorescenceCellStats FluorescenceCellJob::wait(const Document& doc){await_completion();return poll(doc);}
std::shared_ptr<const FluorescenceCellRun> FluorescenceCellJob::result() const {workflow_require_owner(impl_->owner);return impl_->result;}
}
