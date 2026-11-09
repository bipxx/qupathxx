#include "fluorescence_tiled_checkpoint_internal.hpp"
#include "fluorescence_cell_internal.hpp"
#include "raw_pixel_archive_binary.hpp"
#include "atomic_export.hpp"
#include "reader_file_guard.hpp"
#include "qupathxx/workflow.hpp"
#include <fstream>
#include <set>
#include <charconv>

namespace qpx {
void FluorescenceCheckpointOptions::validate(std::size_t work)const {
    if(record_bytes<4096 || record_bytes>64u*1024*1024 || record_bytes>work/16 ||
        total_bytes<record_bytes || total_bytes>64ull*1024*1024*1024)throw std::invalid_argument("Invalid tiled fluorescence checkpoint byte limits");
}
namespace detail {
namespace {
using raw_archive::Reader;using raw_archive::Writer;
void require(bool ok,const char* message){if(!ok)throw std::invalid_argument(message);}
std::optional<std::size_t> tile_index(const std::string& name,std::size_t count) {
    if(name.size()<10 || name.compare(0,5,"tile-") || name.compare(name.size()-4,4,".qft"))return {};
    std::size_t value=0;const auto result=std::from_chars(name.data()+5,name.data()+name.size()-4,value);
    if(result.ec!=std::errc{} || result.ptr!=name.data()+name.size()-4 || value>=count || name!="tile-"+std::to_string(value)+".qft")return {};
    return value;
}
bool temporary(const std::string& name,std::size_t count) {
    const auto at=name.rfind(".qpx-tmp-");if(at==std::string::npos || name.size()!=at+9+24 || name.find_first_not_of("0123456789abcdef",at+9)!=std::string::npos)return false;
    const auto base=name.substr(0,at);return base=="threshold.qft" || base=="complete.qft" || bool(tile_index(base,count));
}
std::string hash(Reader& r){auto value=r.text(64);raw_archive::hash_check(value);return value;}
void header(Writer& w,const std::string& request,const std::string& source){w.text(request);w.text(source);}
void header(Reader& r,const std::string& request,const std::string& source){require(hash(r)==request && hash(r)==source,"Tiled checkpoint source/options differ from request");}
void compartment(Writer& w,const RawCellCompartment& c) {
    w.integer(c.pixels);w.integer(c.valid_pixels);w.integer(c.channels.size());
    for(const auto& v:c.channels){w.integer(v.finite_samples);w.integer(v.nonfinite_samples);for(auto x:{v.sum,v.mean,v.minimum,v.maximum,v.sample_standard_deviation})w.optional(x);}
}
RawCellCompartment compartment(Reader& r,std::size_t channels,std::uint64_t pixels,std::size_t& decoded,std::size_t limit) {
    RawCellCompartment c;c.pixels=r.integer(pixels);c.valid_pixels=r.integer(c.pixels);
    require(r.integer(channels)==channels && channels<=r.left/56,"Invalid checkpoint channel count");
    workflow_capture_charge(decoded,channels*sizeof(RawChannelIntensity),limit);c.channels.resize(channels);
    for(auto& v:c.channels){v.finite_samples=r.integer(c.valid_pixels);v.nonfinite_samples=r.integer(c.valid_pixels);
        require(v.finite_samples==c.valid_pixels-v.nonfinite_samples,"Checkpoint finite/nonfinite counts differ");
        v.sum=r.optional();v.mean=r.optional();v.minimum=r.optional();v.maximum=r.optional();v.sample_standard_deviation=r.optional();
        require(bool(v.sum)==bool(v.finite_samples) && bool(v.mean)==bool(v.finite_samples) && bool(v.minimum)==bool(v.finite_samples) && bool(v.maximum)==bool(v.finite_samples) &&
            bool(v.sample_standard_deviation)==(v.finite_samples>1),"Checkpoint moment missingness differs");
        if(v.minimum)require(*v.minimum<=*v.maximum,"Invalid checkpoint intensity range");if(v.sample_standard_deviation)require(*v.sample_standard_deviation>=0,"Invalid checkpoint standard deviation");}
    return c;
}
void spans(Writer& w,const std::vector<FluorescencePixelSpan>& values){w.integer(values.size());for(const auto& s:values){w.signed_integer(s.y);w.signed_integer(s.x);w.signed_integer(s.width);}}
std::uint64_t spans(Reader& r,std::vector<FluorescencePixelSpan>& out,const RegionRequest& bounds,const RegionRequest& samples,std::size_t& decoded,std::size_t limit) {
    const auto count=r.integer(r.left/24);workflow_capture_charge(decoded,std::size_t(count)*sizeof(FluorescencePixelSpan),limit);out.reserve(std::size_t(count));
    std::uint64_t total=0;const auto maximum=std::uint64_t(samples.width)*std::uint64_t(samples.height);
    for(std::uint64_t i=0;i<count;++i){FluorescencePixelSpan s{r.signed_integer(),r.signed_integer(),r.signed_integer()};
        for(const auto* b:{&bounds,&samples})require(s.y>=b->y && s.y-b->y<b->height && s.x>=b->x && s.x-b->x<b->width && s.width>0 && s.width<=b->width-(s.x-b->x),"Checkpoint span is outside analysis/sample bounds");
        if(!out.empty()){const auto& before=out.back();require(s.y>before.y || (s.y==before.y && s.x>before.x+before.width),"Checkpoint spans overlap or are not canonical");}
        require(std::uint64_t(s.width)<=maximum-total,"Checkpoint geometry exceeds sample lattice");total+=std::uint64_t(s.width);out.push_back(s);}
    return total;
}
}
FluorescenceCheckpointStore::FluorescenceCheckpointStore(const std::string& directory,const std::string& request,const std::string& source,
    std::size_t count,bool otsu,FluorescenceCheckpointOptions options,std::size_t work,const std::atomic<bool>* c):
    root_(std::filesystem::u8path(export_path(directory))),options_(options),request_(request),source_(source),count_(count),otsu_(otsu),present_(count,false),cancelled_(c) {
    options.validate(work);workflow_checkpoint(c);require(count && count<=100000,"Invalid checkpoint tile count");
    manifest_limit_=1024+72*count;reserved_=3*manifest_limit_+4*options.record_bytes+4096;
    require(reserved_<work,"Checkpoint buffers exceed work budget");
    std::filesystem::create_directories(root_);const auto status=std::filesystem::symlink_status(root_);
    require(std::filesystem::is_directory(status) && !std::filesystem::is_symlink(status),"Checkpoint directory must be a regular directory");
    if(!std::filesystem::exists(root_/"threshold.qft"))for(const auto& item:std::filesystem::directory_iterator(root_)) {
        const auto name=item.path().filename().u8string();const std::string prefix="threshold.qft.qpx-tmp-";
        require(name.size()==prefix.size()+24 && name.compare(0,prefix.size(),prefix)==0 && name.find_first_not_of("0123456789abcdef",prefix.size())==std::string::npos,"Nonempty checkpoint directory has no verified threshold record");
        const auto n=std::filesystem::file_size(item.path());require(n<=manifest_limit_ && bytes_<=options_.total_bytes && n<=options_.total_bytes-bytes_,"Orphaned threshold staging exceeds storage limit");bytes_+=n;
    }
}
std::filesystem::path FluorescenceCheckpointStore::tile_path(std::size_t i)const{return root_/std::filesystem::u8path("tile-"+std::to_string(i)+".qft");}
std::vector<std::uint8_t> FluorescenceCheckpointStore::read(const std::filesystem::path& path,std::size_t limit)const {
    workflow_checkpoint(cancelled_);const auto status=std::filesystem::symlink_status(path);
    require(std::filesystem::is_regular_file(status) && !std::filesystem::is_symlink(status),"Checkpoint record is not a regular file");
    ReaderFileGuard guard(path.u8string());const auto n=std::filesystem::file_size(path);require(n>=68 && n<=limit,"Checkpoint record exceeds byte limits");
    std::ifstream file(path,std::ios::binary);std::vector<std::uint8_t> bytes(std::size_t(n),0);
    for(std::size_t at=0;at<bytes.size();){workflow_checkpoint(cancelled_);const auto count=std::min<std::size_t>(65536,bytes.size()-at);file.read(reinterpret_cast<char*>(bytes.data()+at),std::streamsize(count));require(file.gcount()==std::streamsize(count),"Checkpoint record truncated");at+=count;}
    require(file.peek()==std::char_traits<char>::eof() && !file.bad(),"Checkpoint record changed while reading");guard.verify();return bytes;
}
void FluorescenceCheckpointStore::write(const std::filesystem::path& path,const std::vector<std::uint8_t>& bytes) {
    if(bytes_>options_.total_bytes || bytes.size()>options_.total_bytes-bytes_)throw WorkflowCaptureLimit("Tiled checkpoint exceeds total storage limit");
    AtomicExportFile file(path.u8string(),bytes.size(),cancelled_);file.write(reinterpret_cast<const char*>(bytes.data()),bytes.size());file.commit(false);bytes_+=bytes.size();
}
std::optional<FluorescenceThresholdCheckpoint> FluorescenceCheckpointStore::global() {
    const auto path=root_/"threshold.qft";if(!std::filesystem::exists(path))return {};
    const auto bytes=read(path,manifest_limit_);Reader r(bytes.data(),bytes.size(),"QFG\1",cancelled_,manifest_limit_);header(r,request_,source_);
    require(r.integer(count_)==count_,"Checkpoint tile count differs");FluorescenceThresholdCheckpoint result;result.threshold=r.real();result.inputs.reserve(count_);
    for(std::size_t i=0;i<count_;++i){auto value=r.text(64);if(otsu_)raw_archive::hash_check(value);else require(value.empty(),"Explicit checkpoint contains unexpected threshold input hashes");result.inputs.push_back(std::move(value));}r.finish();
    bytes_=bytes.size();require(bytes_<=options_.total_bytes,"Existing threshold checkpoint exceeds storage limit");
    for(const auto& item:std::filesystem::directory_iterator(root_)) {
        workflow_checkpoint(cancelled_);const auto name=item.path().filename().u8string();if(name=="threshold.qft" || name=="complete.qft")continue;
        const auto index=tile_index(name,count_);const bool staging=temporary(name,count_);
        require(index || staging,"Checkpoint directory contains an unrecognized member");const auto status=item.symlink_status();
        require(std::filesystem::is_regular_file(status) && !std::filesystem::is_symlink(status),"Invalid checkpoint tile/staging file");const auto n=item.file_size();
        const auto limit=staging?std::max<std::size_t>(manifest_limit_,options_.record_bytes):options_.record_bytes;
        require(n<=limit && bytes_<=options_.total_bytes && n<=options_.total_bytes-bytes_,"Existing checkpoint/staging files exceed storage limits");bytes_+=n;if(index)present_[*index]=true;
    }
    if(std::filesystem::exists(root_/"complete.qft")){
        const auto completed=read(root_/"complete.qft",2048);Reader done(completed.data(),completed.size(),"QFC\1",cancelled_,2048);header(done,request_,source_);
        require(done.real()==result.threshold,"Completed checkpoint threshold differs");hash(done);hash(done);require(done.integer(count_)==count_,"Completed checkpoint tile count differs");done.integer();done.integer();done.finish();
        require(bytes_<=options_.total_bytes && completed.size()<=options_.total_bytes-bytes_,"Checkpoint completion record exceeds storage limits");bytes_+=completed.size();}
    return result;
}
void FluorescenceCheckpointStore::global(double threshold,const std::vector<std::string>& inputs) {
    require(inputs.size()==count_,"Threshold checkpoint input count differs");Writer w("QFG\1",cancelled_,manifest_limit_);header(w,request_,source_);w.integer(count_);w.real(threshold);for(const auto& h:inputs){if(otsu_)raw_archive::hash_check(h);else require(h.empty(),"Unexpected explicit threshold input hash");w.text(h);}w.finish();write(root_/"threshold.qft",w.bytes);
}
FluorescenceTileResult FluorescenceCheckpointStore::load(std::size_t i,double threshold,const std::string& recipe,const RawRegionRequest& samples,RegionRequest core,RegionRequest bounds,SampleType type) {
    const auto bytes=read(tile_path(i),options_.record_bytes);Reader r(bytes.data(),bytes.size(),"QFT\1",cancelled_,options_.record_bytes);header(r,request_,source_);
    require(r.integer(count_-1)==i && r.real()==threshold,"Checkpoint tile index/threshold differs");FluorescenceTileResult tile;tile.input_sha256=hash(r);tile.recipe_sha256=hash(r);require(tile.recipe_sha256==recipe,"Checkpoint segmentation recipe differs");
    const auto pixels=std::uint64_t(samples.region.width)*std::uint64_t(samples.region.height);tile.finite_segmentation_pixels_in_core=r.integer(std::uint64_t(core.width)*std::uint64_t(core.height));
    const auto cells=r.integer(options_.record_bytes/sizeof(FluorescenceOwnedCell));std::size_t decoded=std::size_t(cells)*sizeof(FluorescenceOwnedCell);const auto limit=2*options_.record_bytes;
    tile.cells.reserve(std::size_t(cells));std::set<std::string> ids;
    for(std::uint64_t j=0;j<cells;++j){workflow_checkpoint(cancelled_);FluorescenceOwnedCell cell;cell.id=hash(r);cell.anchor_x=r.signed_integer();cell.anchor_y=r.signed_integer();
        workflow_capture_charge(decoded,256,limit);
        require(cell.anchor_x>=core.x && cell.anchor_x-core.x<core.width && cell.anchor_y>=core.y && cell.anchor_y-core.y<core.height && ids.insert(cell.id).second,"Invalid checkpoint cell ownership/identity");
        require(cell.id==fluorescence_global_cell_id(source_,recipe,samples,type,cell.anchor_x,cell.anchor_y),"Checkpoint global cell identity differs");
        cell.measurements.nucleus=compartment(r,samples.channels,pixels,decoded,limit);cell.measurements.cell=compartment(r,samples.channels,pixels,decoded,limit);
        if(r.integer(1))cell.measurements.cytoplasm=compartment(r,samples.channels,pixels,decoded,limit);
        const auto nucleus=spans(r,cell.nucleus,bounds,samples.region,decoded,limit),body=spans(r,cell.cell,bounds,samples.region,decoded,limit);
        require(nucleus && body>=nucleus && nucleus==cell.measurements.nucleus.pixels && body==cell.measurements.cell.pixels && cell.nucleus.front().x==cell.anchor_x && cell.nucleus.front().y==cell.anchor_y,"Checkpoint masks/measurements/anchor differ");
        require(cell.measurements.nucleus.valid_pixels==nucleus && cell.measurements.cell.valid_pixels==body,"Complete checkpoint masks contain invalid segmentation support");
        require(bool(cell.measurements.cytoplasm)==(body>nucleus),"Checkpoint cytoplasm missingness differs");if(cell.measurements.cytoplasm)require(cell.measurements.cytoplasm->pixels==body-nucleus && cell.measurements.cytoplasm->valid_pixels==body-nucleus,"Checkpoint cytoplasm size/validity differs");
        std::size_t at=0;for(const auto& n:cell.nucleus){while(at<cell.cell.size() && (cell.cell[at].y<n.y || (cell.cell[at].y==n.y && cell.cell[at].x+cell.cell[at].width<=n.x)))++at;require(at<cell.cell.size() && cell.cell[at].y==n.y && cell.cell[at].x<=n.x && cell.cell[at].x+cell.cell[at].width>=n.x+n.width,"Checkpoint nucleus is outside its cell");}
        tile.cells.push_back(std::move(cell));
    }r.finish();last_index_=i;last_digest_=std::string(reinterpret_cast<const char*>(bytes.data()+bytes.size()-64),64);return tile;
}
std::string FluorescenceCheckpointStore::save(std::size_t i,double threshold,const FluorescenceTileResult& tile) {
    require(tile.complete(),"Unresolved tile cannot be checkpointed as complete");Writer w("QFT\1",cancelled_,options_.record_bytes);header(w,request_,source_);w.integer(i);w.real(threshold);w.text(tile.input_sha256);w.text(tile.recipe_sha256);w.integer(tile.finite_segmentation_pixels_in_core);w.integer(tile.cells.size());
    for(const auto& cell:tile.cells){w.text(cell.id);w.signed_integer(cell.anchor_x);w.signed_integer(cell.anchor_y);compartment(w,cell.measurements.nucleus);compartment(w,cell.measurements.cell);w.integer(bool(cell.measurements.cytoplasm));if(cell.measurements.cytoplasm)compartment(w,*cell.measurements.cytoplasm);spans(w,cell.nucleus);spans(w,cell.cell);}w.finish();
    write(tile_path(i),w.bytes);present_.at(i)=true;last_index_=i;last_digest_=std::string(reinterpret_cast<const char*>(w.bytes.data()+w.bytes.size()-64),64);return last_digest_;
}
std::string FluorescenceCheckpointStore::tile_digest(std::size_t i)const{if(last_index_!=i)throw std::logic_error("Checkpoint tile digest requested out of order");return last_digest_;}
void FluorescenceCheckpointStore::finish(const FluorescenceTiledReport& report) {
    require(report.state=="ready" && report.tiles==count_,"Incomplete tiled run cannot publish a completion record");Writer w("QFC\1",cancelled_,2048);header(w,request_,source_);w.real(report.resolved_threshold);w.text(report.input_sha256);w.text(report.output_sha256);w.integer(report.tiles);w.integer(report.cells);w.integer(report.finite_pixels);w.finish();
    const auto path=root_/"complete.qft";if(std::filesystem::exists(path)){require(read(path,2048)==w.bytes,"Existing checkpoint completion record differs");return;}write(path,w.bytes);
}
}
}
