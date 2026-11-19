#include "json_string.hpp"
#include "qupathxx/project_batch.hpp"
#include "qupathxx/tma.hpp"
#include "qupathxx/input_digest.hpp"
#include "calibrated_cell_numeric.hpp"
#include "qupathxx/pixel_stream_file.hpp"
#include "raw_pixel_stream_internal.hpp"
#include "qupathxx/cell_tables.hpp"
#include "qupathxx/measurement_tables.hpp"
#include "qupathxx/stain_estimation.hpp"
#include "qupathxx/source_identity.hpp"
#include "batch_checkpoint.hpp"
#include "cell_policy_names.hpp"
#include "reader_file_guard.hpp"
#include "project_plugin.hpp"
#include "project_context.hpp"
#include "project_source_internal.hpp"
#include "expression_matrix_project_internal.hpp"
#include "expression_reduction_internal.hpp"
#include "label_project.hpp"
#include "qupathxx/reader_plugin.h"
#include "atomic_export.hpp"
#include "sha256.hpp"
#include "utf8_text.hpp"
#include "filesystem_access.hpp"
#include <algorithm>
#include <charconv>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <future>
#include <iomanip>
#include <locale>
#include <random>
#include <sstream>
#ifndef _WIN32
#include <sys/file.h>
#endif
namespace qpx {
namespace {
namespace fs=std::filesystem;using Clock=std::chrono::steady_clock;using detail::json_string;
std::string token() {std::random_device r;constexpr char hex[]="0123456789abcdef";std::string s;for(int i=0;i<32;++i) s+=hex[r()&15];return s;}
std::string hash(const std::string& s) {Sha256 h;h.update(s.data(),s.size());return h.finish();}
std::string real(double n) {std::ostringstream s;s.imbue(std::locale::classic());s<<std::setprecision(17)<<n;return s.str();}
std::string vector(const ColorVector& v) {return '['+real(v[0])+','+real(v[1])+','+real(v[2])+']';}
std::string error_text(const std::exception& e) {std::string s=e.what();try {detail::bounded_utf8(s,32768);} catch(...) {return "batch operation failed (error text is not bounded UTF-8)";}return s;}
using detail::access_path;
bool raw_classifier_batch(ProjectBatchKind kind) {return kind==ProjectBatchKind::RawPixels || kind==ProjectBatchKind::RawPixelContours;}
bool stream_batch(ProjectBatchKind kind) {return kind==ProjectBatchKind::Pixels || kind==ProjectBatchKind::RawPixels;}
ScientificRawLattice raw_lattice(const Document& doc,const ProjectBatchOptions& o,const std::atomic<bool>* cancelled) {
    if(o.raw_pixel_model_grid) return raw_pixel_model_lattice(o.raw_pixel_model->recipe(),doc,cancelled);
    const auto& m=doc.metadata();return {m.width,m.height,o.raw_pixel_x?*o.raw_pixel_x:ScientificGridAxis(0,m.width,m.width),o.raw_pixel_y?*o.raw_pixel_y:ScientificGridAxis(0,m.height,m.height)};
}
std::string checkpoint_input_sha256(const std::string& snapshot,const std::shared_ptr<const ReaderSource>& observed) {
    if(!observed || !observed->primary) return snapshot;
    Sha256 digest;
    auto text=[&](std::string_view value) {digest.integer(value.size());digest.update(value.data(),value.size());};
    auto file=[&](const FileVersion& version) {
        for(const auto* field:{&version.path,&version.identity_scheme,&version.identity,&version.stamp_scheme,
                &version.modified_token,&version.change_token}) text(*field);
        digest.integer(version.bytes);
    };
    text("qpx.batch-linked-input/1");text(snapshot);text(observed->reader);text(observed->locator);text(observed->scope);
    file(*observed->primary);
    digest.integer(observed->companions.size());
    for(const auto& companion:observed->companions) file(companion);
    text(observed->directory_sha256);
    return digest.finish();
}
std::vector<std::unique_ptr<detail::ReaderFileGuard>> lock_batch_source(const ImageServer& server,
    const std::shared_ptr<const ReaderSource>& observed,const std::atomic<bool>* cancelled) {
    detail::export_checkpoint(cancelled);
    if(!observed || !observed->primary) return {};
    auto current=*observed;
    std::vector<std::unique_ptr<detail::ReaderFileGuard>> guards;
    guards.reserve(1+observed->companions.size());
    guards.push_back(std::make_unique<detail::ReaderFileGuard>(observed->primary->path));
    current.primary=guards.back()->version();
    for(auto& file:current.companions) {
        detail::export_checkpoint(cancelled);
        guards.push_back(std::make_unique<detail::ReaderFileGuard>(file.path));
        file=*guards.back()->version();
    }
    if(!detail::same_reader_files(*observed,current)) throw std::runtime_error("batch image source changed before publication or reuse");
    // The reader owns directory membership and validation of its captured inventory.
    if(observed->scope=="directory_store") server.require_raw_current(cancelled);
    for(const auto& guard:guards) guard->verify();
    detail::export_checkpoint(cancelled);
    return guards;
}
RawPixelStreamFileLimits raw_file_limits(const ProjectBatchOptions& o,std::uint64_t remaining) {return {std::min(o.pixel_file_limit,remaining),o.raw_pixel_file_work_bytes};}

bool plain(const fs::path& path,bool directory) {
#ifdef _WIN32
    const auto flags=GetFileAttributesW(access_path(path).c_str());return flags!=INVALID_FILE_ATTRIBUTES && !(flags&FILE_ATTRIBUTE_REPARSE_POINT) && bool(flags&FILE_ATTRIBUTE_DIRECTORY)==directory;
#else
    std::error_code error;
    const auto s=fs::symlink_status(path,error);
    return !error && (directory?fs::is_directory(s):fs::is_regular_file(s));
#endif
}
struct DirectoryLock {
    int fd=-1;
    explicit DirectoryLock(const fs::path& root) {const auto path=access_path(root/".qupathxx-batch.lock");
#ifdef _WIN32
        if(_wsopen_s(&fd,path.c_str(),_O_CREAT|_O_RDWR|_O_BINARY|_O_NOINHERIT,_SH_DENYRW,_S_IREAD|_S_IWRITE)!=0) fd=-1;
#else
        fd=::open(path.c_str(),O_CREAT|O_RDWR|O_CLOEXEC,0600);if(fd>=0 && ::flock(fd,LOCK_EX|LOCK_NB)!=0) {::close(fd);fd=-1;}
#endif
        if(fd<0) throw std::runtime_error("batch output directory is locked or unavailable");
    }
    ~DirectoryLock() {
#ifdef _WIN32
        if(fd>=0) _close(fd);
#else
        if(fd>=0) ::close(fd);
#endif
    }
};
constexpr std::string_view work_prefix=".qupathxx-batch-work-";
constexpr std::string_view work_marker="qupathxx batch work 1\n";
constexpr std::string_view stream_prefix="pixels-";
constexpr std::string_view stream_marker="qupathxx pixel batch 1\n";

bool hex_token(std::string_view value,std::size_t size) {
    return value.size()==size && value.find_first_not_of("0123456789abcdef")==std::string_view::npos;
}
bool ownership_marker(const fs::path& path,std::string_view prefix,std::string_view marker) {
    const auto name=path.filename().u8string();
    const std::string_view view(name);
    if(view.substr(0,prefix.size())!=prefix || !hex_token(view.substr(prefix.size()),32) ||
       !plain(path,true) || !plain(path/"owner",false)) return false;
    const auto expected=std::string(marker)+name.substr(prefix.size());
    std::ifstream file(access_path(path/"owner"),std::ios::binary);
    std::string bytes(expected.size()+1,'\0');
    file.read(bytes.data(),static_cast<std::streamsize>(bytes.size()));
    return !file.bad() && file.gcount()==static_cast<std::streamsize>(expected.size()) &&
           bytes.compare(0,expected.size(),expected)==0;
}
void reclaim_scratch(const fs::path&,bool,std::size_t&,const std::atomic<bool>*);
struct Work {
    fs::path path;std::string owner;
    explicit Work(const fs::path& root):owner(token()) {path=root/(std::string(work_prefix)+owner);if(!fs::create_directory(access_path(path))) throw std::runtime_error("cannot create batch work directory");std::ofstream f(access_path(path/"owner"),std::ios::binary);f<<work_marker<<owner;f.close();if(!f) {std::error_code e;fs::remove(access_path(path/"owner"),e);fs::remove(access_path(path),e);throw std::runtime_error("cannot mark batch work directory");}}
    ~Work() {
        try {
            std::size_t remaining=100000;
            reclaim_scratch(path,false,remaining,nullptr);
        } catch(...) {}
    }
};
class PixelBatchBundle {
    std::string owner_;
    bool committed_=false,raw_=false;
    std::vector<std::filesystem::path> outputs_;
public:
    std::filesystem::path path;
    PixelBatchBundle(const std::filesystem::path& root,std::size_t count,bool raw=false):owner_(token()),raw_(raw) {
        outputs_.reserve(count);path=access_path(root/(std::string(stream_prefix)+owner_),true);
        if(!std::filesystem::create_directory(path)) throw std::runtime_error("cannot create pixel batch bundle");
        std::ofstream file(path/"owner",std::ios::binary);file<<stream_marker<<owner_;file.close();
        if(!file) {std::error_code error;std::filesystem::remove(path/"owner",error);std::filesystem::remove(path,error);throw std::runtime_error("cannot mark pixel batch bundle");}
    }
    PixelBatchBundle(const PixelBatchBundle&)=delete;
    static bool owned(const std::filesystem::path& root) {
        return ownership_marker(root,stream_prefix,stream_marker);
    }
    std::filesystem::path output(std::uint64_t id) const {return path/("object-"+std::to_string(id)+(raw_?".qws":".qps"));}
    void saved(std::filesystem::path output) {outputs_.push_back(std::move(output));}
    void commit() noexcept {committed_=true;}
    ~PixelBatchBundle() {
        if(committed_) return;
        try {
            if(!owned(path)) return;
            if(std::filesystem::exists(path/"result.qpxproj")) return;
            std::error_code error;
            for(const auto& output:outputs_) {if(!plain(path,true)) return;if(plain(output,false)) {std::filesystem::remove(output,error);if(error) return;}}
            // A published completion marker wins even if an exception prevented
            // the owner's final state update. Unknown files are never removed.
            if(std::filesystem::exists(path/"result.qpxproj")) return;
            for(const auto& entry:std::filesystem::directory_iterator(path)) if(entry.path().filename()!="owner") return;
            std::filesystem::remove(path/"owner",error);if(!error) std::filesystem::remove(path,error);
        } catch(...) {}
    }
};
bool positive_id(std::string_view text) {
    if(text.empty() || text.front()=='0') return false;
    std::uint64_t id=0;
    const auto parsed=std::from_chars(text.data(),text.data()+text.size(),id);
    return parsed.ec==std::errc{} && parsed.ptr==text.data()+text.size() && id>0;
}
bool image_folder(std::string_view name) {
    if(name.substr(0,6)!="image-") return false;
    name.remove_prefix(6);
    const auto separator=name.find('-');
    return separator!=name.npos && positive_id(name.substr(0,separator)) &&
           hex_token(name.substr(separator+1),64);
}
bool owned_stream_file(std::string_view name) {
    if(name.substr(0,7)!="object-") return false;
    name.remove_prefix(7);
    const auto separator=name.find('.');
    if(separator==name.npos || !positive_id(name.substr(0,separator))) return false;
    const auto tail=name.substr(separator);
    if(tail.substr(0,4)!=".qps" && tail.substr(0,4)!=".qws") return false;
    if(tail.size()==4) return true;
    return tail.substr(4,9)==".qpx-tmp-" && hex_token(tail.substr(13),24);
}
bool owned_work_file(std::string_view name) {
    const auto project=name.substr(0,name.find('.'));
    if(project!="input" && project!="result") return false;
    name.remove_prefix(project.size());
    return name==".qpxproj" || name==".qpxproj-journal" ||
           name==".qpxproj-wal" || name==".qpxproj-shm";
}
bool owned_expression_file(std::string_view name,std::string_view extension) {
    if(!hex_token(name.substr(0,64),64)) return false;
    name.remove_prefix(64);
    if(name.substr(0,extension.size())!=extension) return false;
    name.remove_prefix(extension.size());
    return name.empty() || (name.substr(0,9)==".qpx-tmp-" && hex_token(name.substr(9),24));
}
bool collect_expression_scratch(const fs::path& directory,std::string_view extension,
                                std::vector<fs::path>& files,std::size_t& remaining,
                                const std::atomic<bool>* c) {
    if(!plain(directory,true)) return false;
    std::error_code error;
    for(fs::directory_iterator it(access_path(directory),error),end;it!=end && !error;it.increment(error)) {
        detail::export_checkpoint(c);
        if(!remaining) return false;
        --remaining;
        if(!owned_expression_file(it->path().filename().u8string(),extension) || !plain(it->path(),false))
            return false;
        files.push_back(it->path());
    }
    return !error;
}
// Under DirectoryLock, inspect only direct children and the named asset
// directories. A complete bundle, unknown child or link prevents cleanup.
void reclaim_scratch(const fs::path& path,bool stream,std::size_t& remaining,const std::atomic<bool>* c) {
    const auto prefix=stream?stream_prefix:work_prefix,marker=stream?stream_marker:work_marker;
    if(!ownership_marker(path,prefix,marker)) return;
    std::error_code error;
    std::vector<fs::path> files;
    std::vector<fs::path> asset_directories;
    for(fs::directory_iterator it(access_path(path),error),end;it!=end && !error;it.increment(error)) {
        detail::export_checkpoint(c);
        if(!remaining) return;
        --remaining;
        const auto child=it->path().filename().u8string();
        if(child=="owner") continue;
        if(child==".qpx-expression" || child==".qpx-expression-reductions") {
            const auto extension=child==".qpx-expression"?".qmx":".qpr";
            if(!collect_expression_scratch(it->path(),extension,files,remaining,c)) return;
            asset_directories.push_back(it->path());
            continue;
        }
        if(!(stream?owned_stream_file(child):owned_work_file(child)) || !plain(it->path(),false)) return;
        files.push_back(it->path());
    }
    if(error || !ownership_marker(path,prefix,marker)) return;
    for(const auto& file:files) {
        detail::export_checkpoint(c);
        if(!plain(path.parent_path(),true) || !plain(path,true) || !plain(file.parent_path(),true) || !plain(file,false) ||
           (stream && fs::exists(access_path(path/"result.qpxproj"),error)) || error) return;
        fs::remove(access_path(file),error);
        if(error) return;
    }
    for(const auto& directory:asset_directories) {
        detail::export_checkpoint(c);
        if(!plain(path,true) || !plain(directory,true) ||
           (stream && fs::exists(access_path(path/"result.qpxproj"),error)) || error) return;
        if(!fs::remove(access_path(directory),error) || error) return;
    }
    // Keep the marker if any file was added during cleanup.
    std::size_t count=0;
    for(fs::directory_iterator it(access_path(path),error),end;it!=end && !error;it.increment(error)) {
        if(++count!=1 || it->path().filename()!="owner") return;
    }
    if(error || count!=1 || !ownership_marker(path,prefix,marker)) return;
    fs::remove(access_path(path/"owner"),error);
    if(!error) fs::remove(access_path(path),error);
}
void reclaim_batch_scratch(const fs::path& root,const std::atomic<bool>* c) {
    std::size_t remaining=100000;
    std::error_code error;
    for(fs::directory_iterator it(access_path(root),error),end;it!=end && !error && remaining;it.increment(error)) {
        detail::export_checkpoint(c);
        --remaining;
        const auto candidate=it->path();
        const auto name=candidate.filename().u8string();
        if(name.rfind(work_prefix,0)==0) reclaim_scratch(candidate,false,remaining,c);
        else if(image_folder(name) && plain(candidate,true)) {
            std::error_code children;
            for(fs::directory_iterator child(access_path(candidate),children);child!=end && !children && remaining;child.increment(children)) {
                detail::export_checkpoint(c);
                --remaining;
                reclaim_scratch(child->path(),true,remaining,c);
            }
        }
    }
}
void publish(const fs::path& source,const fs::path& destination,const std::atomic<bool>* c) {
    detail::export_checkpoint(c);
#ifdef _WIN32
    if(!MoveFileExW(access_path(source).c_str(),access_path(destination).c_str(),MOVEFILE_WRITE_THROUGH)) throw std::runtime_error("cannot publish batch checkpoint without replacing an existing file");
#else
    if(::link(source.c_str(),destination.c_str())!=0) throw std::runtime_error("cannot publish batch checkpoint without replacing an existing file");::unlink(source.c_str());
#endif
}
std::string csv_quote(const std::string& s) {std::string out="\"";for(char c:s) {if(c=='"') out+='"';out+=c;}return out+'"';}
std::string with_context(const std::string& csv,const std::string& prefix,std::size_t limit) {
    std::string result;auto append=[&](const std::string& s) {if(s.size()>limit-result.size()) throw std::length_error("batch CSV exceeds byte limit");result+=s;};
    const auto first=csv.find("\r\n");if(first==std::string::npos) throw std::logic_error("invalid native CSV header");append("batch_project_identity,batch_project_generation,batch_project_revision,batch_image_id,batch_image_name,");append(csv.substr(0,first+2));
    std::size_t begin=first+2;bool quoted=false;for(std::size_t at=begin;at<csv.size();++at) {if(csv[at]=='"') {if(quoted && at+1<csv.size() && csv[at+1]=='"') ++at;else quoted=!quoted;}
        if(!quoted && csv[at]=='\n') {append(prefix);append(csv.substr(begin,at+1-begin));begin=at+1;}}
    if(quoted || begin!=csv.size()) throw std::logic_error("invalid native CSV rows");return result;
}
std::string parameters(const ProjectBatchOptions& o,const StainTransform& t) {
    // Nominal cells use grid_cells. Keep inactive legacy values canonical so
    // the CLI's shared --threshold flag cannot cause needless resume misses.
    const TissueParameters default_tissue;const CellParameters default_cells;
    const auto e=build_environment();const auto& p=o.cell_nominal?default_tissue:o.tissue;const auto& c=o.cell_nominal?default_cells:o.cells;
    std::ostringstream s;s.imbue(std::locale::classic());s<<std::setprecision(17);
    s<<"{\"schema\":\"qpx.batch-parameters/1\",\"kind\":"<<int(o.kind)<<",\"roi_kind\":"<<int(o.roi_kind)<<",\"object_kind\":"<<int(o.object_kind)<<",\"nucleus\":"<<(o.nucleus?"true":"false")<<",\"publish_cells\":"<<(o.publish_cells?"true":"false")<<",\"roi_tile_size\":"<<o.roi_tile_size;
    if(o.use_saved_stains) s<<",\"use_saved_stains\":true";
    else s<<",\"model_sha256\":"<<json_string(stain_model_sha256(t))<<",\"white\":"<<vector(t.white())<<",\"stains\":["<<vector(t.stains()[0])<<','<<vector(t.stains()[1])<<','<<vector(t.stains()[2])<<']';
    s<<",\"tissue\":{\"feature\":"<<int(p.feature)<<",\"threshold\":"<<p.threshold<<",\"connectivity\":"<<p.connectivity<<",\"level\":"<<p.level<<",\"open_radius\":"<<p.open_radius<<",\"close_radius\":"<<p.close_radius<<",\"fill_holes\":"<<(p.fill_holes?"true":"false")<<",\"max_hole_pixels\":"<<p.max_hole_pixels<<",\"min_pixels\":"<<p.min_pixels<<",\"max_pixels\":"<<p.max_pixels<<",\"tile_size\":"<<p.tile_size<<",\"max_components\":"<<p.max_components<<",\"work_bytes\":"<<p.work_bytes<<'}';
    s<<",\"cells\":{\"feature\":"<<int(c.feature)<<",\"threshold\":"<<c.threshold<<",\"sigma\":"<<c.sigma<<",\"min_peak_radius\":"<<c.min_peak_radius<<",\"min_seed_distance\":"<<c.min_seed_distance<<",\"cell_expansion\":"<<c.cell_expansion<<",\"min_nucleus_pixels\":"<<c.min_nucleus_pixels<<",\"max_nucleus_pixels\":"<<c.max_nucleus_pixels<<",\"connectivity\":"<<c.connectivity<<",\"level\":"<<c.level<<",\"tile_size\":"<<c.tile_size<<",\"max_cells\":"<<c.max_cells<<",\"work_bytes\":"<<c.work_bytes<<",\"contours\":{\"clip_to_roi\":"<<(c.contours.clip_to_roi?"true":"false")<<",\"max_components\":"<<c.contours.max_components<<",\"max_edges\":"<<c.contours.max_edges<<",\"max_vertices\":"<<c.contours.max_vertices<<",\"work_bytes\":"<<c.contours.work_bytes<<"}}";
    if(o.roi_intensity_statistics) s<<",\"roi_intensity_statistics\":true";
    if(o.tissue_grid) {
        const auto& g=*o.tissue_grid;s<<",\"tissue_grid\":{\"samples_x\":"<<g.samples_x<<",\"samples_y\":"<<g.samples_y<<",\"context_x\":"<<g.context_x<<",\"context_y\":"<<g.context_y<<'}';
    }
    if(p.median_cleanup) s<<",\"tissue_median_cleanup\":true";
    if(p.canonical_pyramid) s<<",\"tissue_canonical_pyramid\":true";
    if(p.smoothing_sigma>0) s<<",\"tissue_smoothing_sigma\":"<<p.smoothing_sigma;
    if(o.tissue_min_component_area_um2 || o.tissue_max_component_area_um2) {
        s<<",\"tissue_area_limits\":{\"minimum_um2\":";
        if(o.tissue_min_component_area_um2) s<<*o.tissue_min_component_area_um2;else s<<"null";
        s<<",\"maximum_um2\":";
        if(o.tissue_max_component_area_um2) s<<*o.tissue_max_component_area_um2;else s<<"null";
        s<<'}';
    }
    if(o.cell_min_nucleus_area_um2 || o.cell_max_nucleus_area_um2) {
        s<<",\"cell_area_limits\":{\"minimum_um2\":";
        if(o.cell_min_nucleus_area_um2) s<<*o.cell_min_nucleus_area_um2;else s<<"null";
        s<<",\"maximum_um2\":";
        if(o.cell_max_nucleus_area_um2) s<<*o.cell_max_nucleus_area_um2;else s<<"null";
        s<<'}';
    }
    if(c.algorithm!=CellAlgorithm::DistanceWatershed)
        s<<",\"cell_algorithm\":"<<static_cast<int>(c.algorithm)<<",\"cell_log_prominence\":"<<c.log_prominence<<",\"cell_context_padding\":"<<c.context_padding;
    if(o.kind==ProjectBatchKind::Cells) s<<",\"cell_intensity_statistics_format\":\"qpx.cell-intensity-statistics/1\"";
    if(o.cell_grid || o.cell_nominal) {
        const auto& gp=o.grid_cells;
        if(o.cell_nominal) {
            const auto& n=*o.cell_nominal;
            s<<",\"nominal_cells\":{\"sampling_algorithm\":"<<json_string(n.algorithm())
                <<",\"detector_algorithm\":"<<json_string(cell_detail::nominal_algorithm(n.sampling,gp.processing))<<",\"downsample\":"<<n.downsample
                <<",\"context_x\":"<<n.context_x<<",\"context_y\":"<<n.context_y;
        } else {
            const auto& g=*o.cell_grid;
            s<<",\"grid_cells\":{\"samples_x\":"<<g.samples_x<<",\"samples_y\":"<<g.samples_y<<",\"context_x\":"<<g.context_x<<",\"context_y\":"<<g.context_y;
        }
        if(gp.processing!=CellProcessingPolicy::Quantitative) s<<",\"processing\":"<<json_string(cell_detail::processing_name(gp.processing));
        s<<",\"units\":"<<int(gp.units)<<",\"feature\":"<<int(gp.feature)<<",\"threshold\":"<<gp.threshold<<",\"sigma\":"<<gp.sigma<<",\"log_prominence\":"<<gp.log_prominence
            <<",\"min_peak_radius\":"<<gp.min_peak_radius<<",\"min_seed_distance\":"<<gp.min_seed_distance<<",\"cell_expansion\":"<<gp.cell_expansion
            <<",\"min_nucleus_area\":"<<gp.min_nucleus_area<<",\"max_nucleus_area\":";
        if(gp.max_nucleus_area) s<<*gp.max_nucleus_area;else s<<"null";
        s<<",\"sampling_policy\":"<<int(gp.sampling_policy)<<",\"max_cells\":"<<gp.max_cells<<",\"work_bytes\":"<<gp.work_bytes
            <<",\"contours\":{\"clip_to_roi\":"<<(gp.contours.clip_to_roi?"true":"false")<<",\"max_components\":"<<gp.contours.max_components
            <<",\"max_edges\":"<<gp.contours.max_edges<<",\"max_vertices\":"<<gp.contours.max_vertices<<",\"work_bytes\":"<<gp.contours.work_bytes<<"}}";
    }
    if(o.roi_sampling_policy!=RoiSamplingPolicy::GeometryCoverage)
        s<<",\"roi_sampling_policy\":"<<int(o.roi_sampling_policy);
    if(p.sampling_policy!=RoiSamplingPolicy::GeometryCoverage)
        s<<",\"tissue_sampling_policy\":"<<int(p.sampling_policy);
    if(o.tma_only) s<<",\"tma_scope\":{\"include_missing\":"<<(o.include_missing_tma?"true":"false")<<'}';
    if(o.kind==ProjectBatchKind::Pixels) {const auto& v=o.pixels;
        s<<",\"pixels\":{\"model_sha256\":"<<json_string(o.pixel_model->sha256())<<",\"block_size\":"<<v.block_size<<",\"tile_size\":"<<v.prediction.tile_size<<",\"work_bytes\":"<<v.prediction.work_bytes<<",\"min_vote_fraction\":"<<v.prediction.min_vote_fraction<<",\"max_pixels\":"<<v.max_pixels<<",\"max_blocks\":"<<v.max_blocks<<",\"file_limit\":"<<o.pixel_file_limit<<",\"total_limit\":"<<o.pixel_total_limit<<'}';
    }
    if(raw_classifier_batch(o.kind)) {const auto& v=o.raw_pixels;
        s<<",\"raw_pixels\":{\"model_sha256\":"<<json_string(o.raw_pixel_model->sha256())<<",\"tile_width\":"<<v.tile_width<<",\"tile_height\":"<<v.tile_height<<",\"work_bytes\":"<<v.work_bytes<<",\"min_vote_fraction\":"<<v.min_vote_fraction<<",\"max_samples\":"<<v.max_samples<<",\"max_windows\":"<<v.max_windows<<",\"file_work_bytes\":"<<o.raw_pixel_file_work_bytes<<",\"file_limit\":"<<o.pixel_file_limit<<",\"total_limit\":"<<o.pixel_total_limit;
        auto axis=[&](const char* name,const std::optional<ScientificGridAxis>& a) {s<<",\""<<name<<"\":";if(a) s<<'['<<a->origin()<<','<<a->extent()<<','<<a->samples()<<','<<a->context()<<']';else s<<"null";};
        axis("x",o.raw_pixel_x);axis("y",o.raw_pixel_y);
        if(o.raw_pixel_model_grid) s<<",\"model_grid\":true";
        s<<'}';
    }
    if(o.kind==ProjectBatchKind::RawPixelContours) {const auto& v=o.raw_contours;const auto& g=v.contours;
        s<<",\"raw_contours\":{\"prefix\":"<<json_string(o.raw_contour_prefix)<<",\"publication_bytes\":"<<o.raw_contour_publication_bytes<<",\"boundary_work_bytes\":"<<v.boundary_work_bytes<<",\"max_boundary_runs\":"<<v.max_boundary_runs<<",\"work_bytes\":"<<v.work_bytes<<",\"clip_to_roi\":"<<(g.clip_to_roi?"true":"false")<<",\"max_components\":"<<g.max_components<<",\"max_edges\":"<<g.max_edges<<",\"max_vertices\":"<<g.max_vertices<<",\"contour_work_bytes\":"<<g.work_bytes<<'}';
    }
    if(o.raw_grid) {
        const auto& g=*o.raw_grid;
        s<<",\"raw_grid\":{\"recipe\":\"qpx.raw-grid-statistics/1\",\"input\":\"qpx.raw-grid-statistics-input/1\",\"samples_x\":"<<g.samples_x<<",\"samples_y\":"<<g.samples_y
            <<",\"context_x\":"<<g.context_x<<",\"context_y\":"<<g.context_y<<",\"work_bytes\":"<<o.raw_grid_work_bytes<<'}';
    }
    if(o.kind==ProjectBatchKind::Raw) s<<",\"raw\":{\"hash\":"<<json_string(o.raw_grid?"qpx.raw-grid-statistics-input/1":"qpx.raw-roi-input/1")<<",\"level\":"<<o.raw.region.level<<",\"first_channel\":"<<o.raw.first_channel<<",\"channels\":"<<o.raw.channels<<",\"z\":"<<o.raw.z<<",\"time\":"<<o.raw.t<<",\"follow_display\":"<<(o.raw_follow_display?"true":"false")<<",\"max_pixels\":"<<o.raw_max_pixels<<'}';
    s<<",\"environment\":{\"app_version\":"<<json_string(e->app_version)<<",\"source_tree_sha256\":"<<json_string(e->source_tree_sha256)<<",\"compiler\":"<<json_string(e->compiler)<<",\"compiler_flags\":"<<json_string(e->compiler_flags)<<",\"configuration\":"<<json_string(e->configuration)<<",\"platform\":"<<json_string(e->platform)<<",\"geos_version\":"<<json_string(e->geos_version)<<",\"openslide_version\":"<<json_string(e->openslide_version)<<",\"sqlite_version\":"<<json_string(e->sqlite_version)<<"}}";
    auto value=s.str();if(value.size()>512*1024) throw std::length_error("batch parameter manifest exceeds limit");return value;
}
struct Progress {std::atomic<int> phase{0};std::atomic<std::uint64_t> image{0};std::atomic<std::size_t> images{0},objects{0};};
const char* phase_name(int p) {static const char* names[]={"preparing","snapshotting","hashing","analyzing","saving","verifying","reporting"};return p>=0 && p<7?names[p]:"unknown";}
std::optional<RawRegionRequest> raw_batch_region(const ProjectBatchOptions& options,const ImageServer& server) {
    auto region=options.raw;
    if(options.raw_follow_display) {
        const auto view=server.raw_view();
        if(!view) return {};
        region.z=view->z;
        region.t=view->t;
    }
    return region;
}
}
struct ProjectBatchRequest::Data {
    struct Selection : ProjectBatchSelection {bool filtered_all_raw_objects=false;};
    ProjectBatchOptions options;StainTransform transform;std::unique_ptr<Project> snapshot;std::vector<Selection> selections;
    std::set<std::string> protected_stores;
    std::string json,sha;std::size_t bytes=0,objects=0;double capture=0;
    explicit Data(const StainTransform& t):transform(t) {}
};
void ProjectBatchOptions::validate() const {
    if(use_saved_stains && kind!=ProjectBatchKind::Roi && kind!=ProjectBatchKind::Tissue && kind!=ProjectBatchKind::Cells)
        throw std::invalid_argument("Saved stains require an ROI, tissue or cells batch");
    if(grid_cells.processing!=CellProcessingPolicy::Quantitative && !cell_nominal) throw std::invalid_argument("ImageJ cell processing requires nominal RGB sampling");
    if(cell_grid && cell_nominal) throw std::invalid_argument("cell area-grid and nominal sampling are mutually exclusive");
    if(cell_nominal) {
        cell_nominal->validate();grid_cells.validate();
        if(kind!=ProjectBatchKind::Cells || cell_min_nucleus_area_um2 || cell_max_nucleus_area_um2 ||
           cells.algorithm!=CellAlgorithm::DistanceWatershed || cells.level!=-1 || cells.tile_size!=256)
            throw std::invalid_argument("nominal sampling requires a cells batch with typed grid parameters and no legacy level/tile/area overrides");
    }
    if(raw_grid) {
        raw_grid->validate();ScientificRawOptions options;options.work_bytes=raw_grid_work_bytes;options.validate();
        if(kind!=ProjectBatchKind::Raw || raw.region.level!=0 || roi_tile_size!=256)
            throw std::invalid_argument("raw grid sampling requires a raw batch without natural level/tile overrides");
    } else if(raw_grid_work_bytes!=256u*1024*1024) throw std::invalid_argument("raw grid work budget requires raw grid sampling");
    if(cell_grid) {
        cell_grid->validate();grid_cells.validate();
        if(kind!=ProjectBatchKind::Cells || cell_min_nucleus_area_um2 || cell_max_nucleus_area_um2)
            throw std::invalid_argument("grid sampling requires a cells batch and typed grid area limits");
    }
    validate_sampling_policy(roi_sampling_policy);
    if(kind!=ProjectBatchKind::Cells && cells.algorithm!=CellAlgorithm::DistanceWatershed)
        throw std::invalid_argument("LoG detector requires a cell batch");
    if(kind!=ProjectBatchKind::Roi && roi_sampling_policy!=RoiSamplingPolicy::GeometryCoverage)
        throw std::invalid_argument("ROI sampling policy requires an ROI batch");
    if(kind!=ProjectBatchKind::Tissue && tissue.median_cleanup) throw std::invalid_argument("tissue median cleanup requires a tissue batch");
    if(kind!=ProjectBatchKind::Tissue && tissue.canonical_pyramid) throw std::invalid_argument("canonical pyramid requires a tissue batch");
    if(kind!=ProjectBatchKind::Tissue && tissue.smoothing_sigma!=0) throw std::invalid_argument("tissue smoothing requires a tissue batch");
    if(kind!=ProjectBatchKind::Tissue && tissue.sampling_policy!=RoiSamplingPolicy::GeometryCoverage)
        throw std::invalid_argument("tissue sampling policy requires a tissue batch");
    if(tissue_grid) {
        if(kind!=ProjectBatchKind::Tissue) throw std::invalid_argument("tissue grid requires a tissue batch");
        tissue_grid->validate();
        if(tissue.level>0 || tissue.tile_size!=256) throw std::invalid_argument("tissue grid requires level zero/auto and default tile size");
    }
    if(roi_intensity_statistics && kind!=ProjectBatchKind::Roi) throw std::invalid_argument("ROI intensity statistics require an ROI batch");
    detail::validate_tissue_area_limits(tissue_min_component_area_um2,tissue_max_component_area_um2);
    if(kind!=ProjectBatchKind::Tissue && (tissue_min_component_area_um2 || tissue_max_component_area_um2))
        throw std::invalid_argument("physical tissue area limits require a tissue batch");
    detail::validate_cell_area_limits(cell_min_nucleus_area_um2,cell_max_nucleus_area_um2);
    if(kind!=ProjectBatchKind::Cells && (cell_min_nucleus_area_um2 || cell_max_nucleus_area_um2))
        throw std::invalid_argument("physical nucleus area limits require a cell batch");
    if(kind!=ProjectBatchKind::Roi && kind!=ProjectBatchKind::Tissue && kind!=ProjectBatchKind::Cells && kind!=ProjectBatchKind::Pixels && kind!=ProjectBatchKind::Raw && kind!=ProjectBatchKind::RawPixels && kind!=ProjectBatchKind::RawPixelContours) throw std::invalid_argument("invalid batch kind");
    if(failure!=ProjectBatchFailure::Stop && failure!=ProjectBatchFailure::Continue) throw std::invalid_argument("invalid batch failure policy");
    if(roi_kind!=AnalysisKind::OpticalDensityMeans && roi_kind!=AnalysisKind::StainMeans) throw std::invalid_argument("invalid batch ROI kind");
    if(object_kind!=ObjectKind::Annotation && object_kind!=ObjectKind::Detection && object_kind!=ObjectKind::Cell) throw std::invalid_argument("invalid batch object kind");
    if(roi_tile_size<16 || roi_tile_size>1024 || !snapshot_budget || snapshot_budget>4ULL*1024*1024*1024 || !materialize_budget || materialize_budget>4ULL*1024*1024*1024 || !result_budget || result_budget>768ULL*1024*1024 || !csv_limit || csv_limit>256ULL*1024*1024 || checkpoint_limit<4096 || checkpoint_limit>4ULL*1024*1024*1024) throw std::invalid_argument("invalid batch resource limits");
    if((include_missing_tma && !tma_only) || (tma_only && nucleus)) throw std::invalid_argument("TMA scope requires body cores and an explicit missing-core policy");
    if(kind==ProjectBatchKind::Raw && (raw.region.level<0 || !raw.channels || raw.channels>65536 || raw.first_channel>65536-raw.channels || raw.z>65535 || raw.t>65535 || !raw_max_pixels || raw_max_pixels>1000000000000ULL)) throw std::invalid_argument("invalid raw batch axes or pixel limit");
    pixels.validate();
    if(pixel_file_limit<4096 || pixel_file_limit>(64ULL<<40) || pixel_total_limit<4096 || pixel_total_limit>(64ULL<<40)) throw std::invalid_argument("invalid pixel batch file limits");
    if(kind==ProjectBatchKind::Pixels && (!pixel_model || nucleus || publish_cells)) throw std::invalid_argument("pixel batch requires a model and body ROIs without cell publication");
    if(raw_classifier_batch(kind)) {
        if(!raw_pixel_model || nucleus || publish_cells) throw std::invalid_argument("raw pixel batch requires a model and body ROIs without cell publication");
        if(raw_pixel_model_grid && (raw_pixel_x || raw_pixel_y)) throw std::invalid_argument("Raw pixel model grid conflicts with explicit axes");
        if(bool(raw_pixel_x)!=bool(raw_pixel_y)) throw std::invalid_argument("raw pixel batch requires both axes or the unit lattice");
        raw_pixels.validate();raw_file_limits(*this,pixel_total_limit).validate();
    } else if(raw_pixel_model || raw_pixel_x || raw_pixel_y || raw_pixel_model_grid || raw_pixel_file_work_bytes!=256u*1024*1024) throw std::invalid_argument("raw classifier options require a raw pixel batch");
    if(kind==ProjectBatchKind::RawPixelContours) {
        if(!has_geos()) throw std::runtime_error("raw contour batch requires native geometry");
        raw_contours.validate();detail::bounded_utf8(raw_contour_prefix,256);
        if(raw_contour_prefix.empty() || !raw_contour_publication_bytes || raw_contour_publication_bytes>512u*1024*1024) throw std::invalid_argument("invalid raw contour publication options");
    }
    tissue.validate();cells.validate();if(kind==ProjectBatchKind::Cells && nucleus) throw std::invalid_argument("cell batch uses body source ROIs");if(publish_cells && (kind!=ProjectBatchKind::Cells || !has_geos())) throw std::invalid_argument("cell publication requires a cells batch and native geometry");
}
ProjectBatchRequest::ProjectBatchRequest(const Project& project,const StainTransform& transform,const std::vector<ProjectBatchSelection>& selection,ProjectBatchOptions options) {
    const auto start=Clock::now();if(!has_project()) throw std::runtime_error("project batch requires project storage");options.validate();if(selection.size()>10000 || (selection.empty() && project.entries_.size()>10000)) throw std::length_error("batch exceeds 10000 images");
    if(options.tissue_grid) {options.tissue_grid=std::make_shared<const TissueGridSampling>(*options.tissue_grid);options.tissue.level=0;}
    const auto& actual_transform=options.kind==ProjectBatchKind::Pixels?options.pixel_model->transform():transform;auto d=std::make_shared<Data>(actual_transform);d->options=options;if(options.kind!=ProjectBatchKind::Pixels) d->options.pixel_model.reset();d->json=parameters(options,actual_transform);d->sha=hash(d->json);
    std::vector<Data::Selection> chosen;
    if(selection.empty()) {
        for(const auto& [id,e]:project.entries_) chosen.push_back({{id,{}}});
    } else {
        chosen.reserve(selection.size());
        for(const auto& item:selection) chosen.push_back({item});
    }
    if(options.tma_only && selection.empty()) {chosen.erase(std::remove_if(chosen.begin(),chosen.end(),[&](const auto& item){return !project.entry(item.image_id).document->tma_core_count();}),chosen.end());if(chosen.empty()) throw std::invalid_argument("project has no TMA grids");}
    std::sort(chosen.begin(),chosen.end(),[](const auto& a,const auto& b){return a.image_id<b.image_id;});std::set<std::uint64_t> ids;// Keep legacy unfiltered snapshot accounting inside the existing bookkeeping
    // allowance; smoothed requests fully charge their new sigma metadata.
    std::size_t charge=sizeof(Data)-sizeof(d->protected_stores)-detail::inactive_tissue_parameter_bytes(options.tissue)-
        (options.tissue_grid?0:sizeof(options.tissue_grid))+(options.tissue_grid?sizeof(TissueGridSampling):0)+65536+d->json.capacity()+d->sha.capacity();
    if(options.kind==ProjectBatchKind::Pixels) charge+=4ULL*options.pixel_model->bytes().size()+64ULL*options.pixel_model->forest().node_count()+65536;
    if(raw_classifier_batch(options.kind)) charge+=4ULL*options.raw_pixel_model->bytes().size()+64ULL*options.raw_pixel_model->forest().node_count()+65536;
    const auto protect_store=[&](const std::string& source) {
        const auto position=d->protected_stores.lower_bound(source);
        if(position!=d->protected_stores.end() && *position==source) return;
        if(d->protected_stores.empty()) charge+=sizeof(d->protected_stores);
        const auto bytes=sizeof(std::string)+64+source.size()+1;
        if(charge>=options.snapshot_budget || bytes>options.snapshot_budget-charge)
            throw std::length_error("batch protected store paths exceed snapshot budget");
        d->protected_stores.emplace_hint(position,source);
        charge+=bytes;
    };
    for(const auto& [id,entry]:project.entries_) {
        detail::visit_label_project_source_stores(entry.document->image_data_properties(),protect_store);
        if(entry.info.embedded || entry.info.source_path.empty()) continue;
        if(detail::is_project_image_directory_source(*entry.document->server()))
            protect_store(entry.info.source_path);
    }
    for(auto& item:chosen) {
        if(!ids.insert(item.image_id).second) throw std::invalid_argument("duplicate batch image selection");const auto& doc=*project.entry(item.image_id).document;
        if(options.use_saved_stains) {
            const auto& settings=doc.image_analysis_settings();
            if(!settings || !settings->rgb8_supported())
                throw std::invalid_argument("Selected image "+std::to_string(item.image_id)+" requires saved RGB8 stains");
        }
        if(item.object_ids.size()>100000) throw std::length_error("batch object selection exceeds limit");
        const bool automatic=item.object_ids.empty();
        if(options.tma_only) {
            if(!doc.tma_core_count()) throw std::invalid_argument("selected image has no TMA grid");
            if(item.object_ids.empty()) item.object_ids=doc.tma_core_ids(options.include_missing_tma);
            else {std::sort(item.object_ids.begin(),item.object_ids.end());item.object_ids.erase(std::unique(item.object_ids.begin(),item.object_ids.end()),item.object_ids.end());std::vector<std::uint64_t> filtered;filtered.reserve(item.object_ids.size());for(auto id:item.object_ids) {const auto a=doc.annotation(id);if(a.tma_core && (options.include_missing_tma || !a.tma_core->missing())) filtered.push_back(id);}item.object_ids=std::move(filtered);}
        }
        else if(item.object_ids.empty()) {for(const auto& [id,a]:doc.objects()) if(a.kind==options.object_kind && a.geometry.is_area()) item.object_ids.push_back(id);}
        else {std::sort(item.object_ids.begin(),item.object_ids.end());item.object_ids.erase(std::unique(item.object_ids.begin(),item.object_ids.end()),item.object_ids.end());for(auto id:item.object_ids) if(!doc.annotation(id).geometry.is_area()) throw std::invalid_argument("batch source must have an area ROI");}
        if(options.kind==ProjectBatchKind::Raw && automatic && !item.object_ids.empty()) {
            if(const auto raw=raw_batch_region(options,*doc.server())) {
                const auto& objects=doc.objects();
                item.object_ids.erase(std::remove_if(item.object_ids.begin(),item.object_ids.end(),[&](auto id) {
                    return !objects.at(id).plane.matches_raw(raw->first_channel,raw->channels,raw->z,raw->t);
                }),item.object_ids.end());
                item.filtered_all_raw_objects=item.object_ids.empty();
            }
        }
        if(options.kind==ProjectBatchKind::Raw && item.object_ids.size()>10000-doc.raw_analysis_record_count()) throw std::length_error("raw batch exceeds per-image record limit");
        if(options.kind==ProjectBatchKind::Pixels && item.object_ids.size()>10000-doc.pixel_stream_record_count()) throw std::length_error("pixel batch exceeds per-image stream catalog limit");
        if(options.kind==ProjectBatchKind::RawPixels && item.object_ids.size()>10000-doc.raw_pixel_stream_record_count()) throw std::length_error("raw pixel batch exceeds per-image stream catalog limit");
        if(item.object_ids.size()>100000-d->objects) throw std::length_error("batch exceeds 100000 source objects");d->objects+=item.object_ids.size();charge+=sizeof(Data::Selection)+item.object_ids.capacity()*sizeof(std::uint64_t);
    }
    if(charge>=options.snapshot_budget) throw std::length_error("batch capture exceeds snapshot budget");std::size_t bytes=0;d->snapshot=project.snapshot_for_save(options.snapshot_budget-charge,bytes,&ids);d->bytes=bytes+charge;d->selections=std::move(chosen);d->capture=std::chrono::duration<double,std::milli>(Clock::now()-start).count();data_=std::move(d);
}
std::size_t ProjectBatchRequest::image_count() const {return data_->selections.size();}
std::size_t ProjectBatchRequest::object_count() const {return data_->objects;}
std::size_t ProjectBatchRequest::snapshot_bytes() const {return data_->bytes;}
double ProjectBatchRequest::capture_ms() const {return data_->capture;}
std::string ProjectBatchRequest::parameters_json() const {return data_->json;}
std::string ProjectBatchRequest::parameters_sha256() const {return data_->sha;}
struct ProjectBatchKernel {
    static std::uint64_t cost(const Document& d,bool embedded) {
        auto value=65536+2ULL*(d.object_bytes_+d.analysis_bytes_+d.tissue_bytes_+d.cells_bytes_+d.classifiers_bytes_+d.pixels_bytes_+d.streams_bytes_+d.raw_analysis_bytes_+d.raw_pixels_bytes_+d.raw_streams_bytes_)+(embedded?2ULL*d.image().pixels().size():65536);
        value+=4ULL*(d.metadata().levels.size()*sizeof(ImageLevel)+d.metadata().vendor.size()+d.server()->source_locator().size());
        for(const auto& item:d.classes_) value+=4ULL*(sizeof(ObjectClass)+128+item.first.size()+item.second.name.size());return value;
    }
    static void verify_pixels(const fs::path& candidate,const Document& source,const ProjectBatchSelection& item,const ProjectBatchOptions& o,const std::string& expected,std::size_t budget,ProjectBatchImageResult& stats,const std::atomic<bool>* c) {
        std::set<std::uint64_t> old;for(const auto& r:source.pixel_stream_records()) old.insert(r->id);
        std::size_t index=0;std::uint64_t bytes=0,generated=0;Sha256 inputs;const std::string tag="qpx.batch-pixels/1";inputs.update(tag.data(),tag.size());
        for(const auto& r:detail::batch_pixel_records(access_path(candidate).u8string(),item.image_id,old,budget,c)) {
            detail::export_checkpoint(c);
            if(index>=item.object_ids.size()) throw std::invalid_argument("pixel batch checkpoint has extra output records");
            const auto retained=r->payload_bytes();if(retained>o.result_budget-generated) throw std::length_error("pixel checkpoint descriptors exceed result budget");generated+=retained;
            const auto id=item.object_ids[index++];const auto& link=*r->link;const auto path=access_path(candidate.parent_path()/("object-"+std::to_string(id)+".qps"));
            if(access_path(fs::u8path(link.path())).lexically_normal()!=path.lexically_normal() || link.object_id()!=id || link.model_sha256()!=o.pixel_model->sha256() || link.windowed() || double(link.block_size())!=std::min(double(o.pixels.block_size),std::max(link.level_bounds().width,link.level_bounds().height))) throw std::invalid_argument("pixel batch output descriptor does not match target/model/bundle");
            if(!plain(path,false) || link.summary().bytes>o.pixel_total_limit-bytes) throw std::length_error("pixel batch output is not plain or exceeds total file limit");
            PixelStreamFile file(path.u8string(),std::min(o.pixel_file_limit,o.pixel_total_limit-bytes));file.verify(c);
            if(!file.link()->same_content(link)) throw std::invalid_argument("pixel batch output differs from its saved descriptor");
            bytes+=file.declared_summary().bytes;stats.stream_bytes+=file.declared_summary().bytes;stats.verified_blocks+=file.declared_summary().blocks;const auto input=file.input_sha256();inputs.integer(id);inputs.update(input.data(),input.size());
        }
        if(index!=item.object_ids.size() || inputs.finish()!=expected) throw std::invalid_argument("pixel batch output input digest does not match current source");
    }
    static void verify_raw_pixels(const fs::path& candidate,const Document& source,const ProjectBatchSelection& item,const ProjectBatchOptions& o,const std::optional<ScientificRawLattice>& lattice,const std::string& expected,std::size_t budget,ProjectBatchImageResult& stats,const std::atomic<bool>* c) {
        std::set<std::uint64_t> old;for(const auto& r:source.raw_pixel_stream_records()) old.insert(r->id);
        std::size_t index=0;std::uint64_t bytes=0,generated=0;Sha256 inputs;const std::string tag="qpx.batch-pixels/1";inputs.update(tag.data(),tag.size());
        for(const auto& r:detail::batch_raw_pixel_records(access_path(candidate).u8string(),item.image_id,old,budget,c)) {
            detail::export_checkpoint(c);if(index>=item.object_ids.size()) throw std::invalid_argument("raw pixel checkpoint has extra output records");
            if(r->payload_bytes()>o.result_budget-generated) throw std::length_error("raw pixel descriptors exceed result budget");generated+=r->payload_bytes();
            const auto id=item.object_ids[index++];const auto& link=*r->link;const auto path=access_path(candidate.parent_path()/("object-"+std::to_string(id)+".qws"));
            const RawPixelTiledPredictionRequest request(source,*o.raw_pixel_model,lattice.value(),o.raw_pixels,{},id);
            if(access_path(fs::u8path(link.path())).lexically_normal()!=path.lexically_normal() || link.object_id()!=id || link.model_sha256()!=o.raw_pixel_model->sha256() || link.request_sha256()!=request.sha256() || !link.summary().complete) throw std::invalid_argument("raw pixel descriptor does not match target/model/bundle");
            if(!plain(path,false) || link.summary().bytes>o.pixel_total_limit-bytes) throw std::length_error("raw pixel output is not plain or exceeds total file limit");
            RawPixelStreamFile file(path.u8string(),raw_file_limits(o,o.pixel_total_limit-bytes));file.verify(c);
            if(!file.link()->same_content(link)) throw std::invalid_argument("raw pixel output differs from saved descriptor");
            const auto& summary=file.declared_summary();bytes+=summary.bytes;stats.stream_bytes+=summary.bytes;stats.verified_blocks+=summary.completed_windows;inputs.integer(id);inputs.update(summary.input_sha256.data(),summary.input_sha256.size());
        }
        if(index!=item.object_ids.size() || inputs.finish()!=expected) throw std::invalid_argument("raw pixel output input digest differs from current source");
    }
    static std::string raw_contour_csv(const Document& doc,const std::vector<std::uint64_t>& objects,const ProjectBatchOptions& o) {
        std::string csv="object_id,parent_id,class_name,name,area_px2,perimeter_px\r\n";
        for(auto id:objects) {const auto& a=doc.annotation(id);const auto row=std::to_string(id)+','+std::to_string(a.parent_id)+','+csv_quote(a.class_name)+','+csv_quote(a.name)+','+real(a.geometry.area())+','+real(a.geometry.perimeter())+"\r\n";
            if(csv.size()>o.csv_limit || row.size()>o.csv_limit-csv.size()) throw std::length_error("raw contour CSV exceeds limit");csv+=row;}
        if(csv.size()>o.csv_limit) throw std::length_error("raw contour CSV exceeds limit");return csv;
    }
    static std::string raw_pixel_csv(const Document& doc,const std::vector<std::uint64_t>& records,const ProjectBatchOptions& o) {
        std::string csv="object_id,record_id,path,model_sha256,content_sha256,windows,valid_samples,classified_samples,class_index,class_name,class_samples\r\n";
        auto append=[&](const std::string& row) {if(csv.size()>o.csv_limit || row.size()>o.csv_limit-csv.size()) throw std::length_error("raw pixel CSV exceeds byte limit");csv+=row;};
        for(const auto id:records) {const auto& r=*doc.raw_pixel_stream_record(id);const auto& l=*r.link;const auto& v=l.summary();
            const auto prefix=std::to_string(l.object_id())+','+std::to_string(id)+','+csv_quote(fs::u8path(l.path()).filename().u8string())+','+csv_quote(l.model_sha256())+','+csv_quote(v.content_sha256)+','+std::to_string(v.completed_windows)+','+std::to_string(v.valid_samples)+','+std::to_string(v.classified_samples)+',';
            for(std::size_t i=0;i<v.class_counts.size();++i) append(prefix+std::to_string(i+1)+','+csv_quote(o.raw_pixel_model->forest().classes()[i])+','+std::to_string(v.class_counts[i])+"\r\n");
        }
        if(csv.size()>o.csv_limit) throw std::length_error("raw pixel CSV exceeds byte limit");return csv;
    }
    static std::string pixel_csv(const Document& doc,const std::vector<std::uint64_t>& records,const ProjectBatchOptions& o) {
        std::string csv="object_id,record_id,path,model_sha256,content_sha256,blocks,valid_pixels,classified_pixels,class_index,class_name,class_pixels\r\n";
        auto append=[&](const std::string& row) {if(csv.size()>o.csv_limit || row.size()>o.csv_limit-csv.size()) throw std::length_error("pixel batch CSV exceeds byte limit");csv+=row;};
        for(const auto id:records) {const auto& r=*doc.pixel_stream_record(id);const auto& l=*r.link;const auto& v=l.summary();
            const auto prefix=std::to_string(l.object_id())+','+std::to_string(id)+','+csv_quote(fs::u8path(l.path()).filename().u8string())+','+csv_quote(l.model_sha256())+','+csv_quote(v.sha256)+','+std::to_string(v.blocks)+','+std::to_string(v.valid_pixels)+','+std::to_string(v.classified_pixels)+',';
            for(std::size_t i=0;i<v.class_counts.size();++i) append(prefix+std::to_string(i+1)+','+csv_quote(o.pixel_model->classes()[i])+','+std::to_string(v.class_counts[i])+"\r\n");
        }
        if(csv.size()>o.csv_limit) throw std::length_error("pixel batch CSV exceeds byte limit");return csv;
    }
    static ProjectBatchReport run(const ProjectBatchRequest& request,const std::string& directory,const std::atomic<bool>* c,Progress* progress,const std::function<void(const ProjectBatchImageResult&)>& sink) {
        const auto started=Clock::now();const auto& d=*request.data_;const auto& o=d.options;ProjectBatchReport report;report.parameters_sha256=d.sha;report.source_identity=d.snapshot->identity_;report.source_generation=d.snapshot->generation_;report.source_revision=d.snapshot->revision_;report.snapshot_bytes=d.bytes;report.capture_ms=d.capture;std::size_t report_bytes=65536+d.selections.size()*sizeof(ProjectBatchImageResult);report.images.reserve(d.selections.size());
        const auto context_bytes=detail::project_class_bytes(d.snapshot->class_registry_,d.snapshot->class_defaults_)+
            detail::string_metadata_bytes(d.snapshot->metadata_)-sizeof(d.snapshot->metadata_);
        const auto image_budget=o.materialize_budget-std::min(o.materialize_budget,context_bytes);
        auto phase=[&](int p){if(progress) progress->phase.store(p);};auto check=[&]{detail::export_checkpoint(c);};fs::path root;
        std::optional<DirectoryLock> lock;
        try {
            check();
            const auto output=detail::export_path(directory);
            root=fs::u8path(output);
            const auto protected_output=access_path(root,true).u8string();
            for(const auto& source:d.protected_stores) {
                check();
                detail::guard_directory_output(access_path(fs::u8path(source),true).u8string(),protected_output);
            }
            fs::create_directories(access_path(root));if(!plain(root,true)) throw std::runtime_error("batch output must be a plain directory");
            lock.emplace(root);
            reclaim_batch_scratch(root,c);
            for(const auto& item:d.selections) {
                check();const auto image_start=Clock::now();const auto& input=d.snapshot->entry(item.image_id);ProjectBatchImageResult result;result.image_id=item.image_id;result.image_name=input.info.name;result.objects=item.object_ids.size();if(progress) progress->image.store(item.image_id);
                try {
                    phase(1);if(cost(*input.document,input.info.embedded)>image_budget) throw std::length_error("batch image exceeds materialization budget");Work work(root);
                    const auto input_path=(work.path/"input.qpxproj").u8string();
                    {
                        Project isolated;
                        isolated.path_=d.snapshot->path_;
                        isolated.identity_=d.snapshot->identity_;
                        isolated.next_id_=d.snapshot->next_id_;
                        isolated.active_image_=item.image_id;
                        isolated.metadata_=d.snapshot->metadata_;
                        isolated.class_registry_=d.snapshot->class_registry_;
                        isolated.class_defaults_=d.snapshot->class_defaults_;
                        isolated.entries_.emplace(item.image_id,input);
                        if(const auto view=d.snapshot->views_.find(item.image_id);view!=d.snapshot->views_.end())
                            isolated.views_.emplace(*view);
                        if(const auto verification=d.snapshot->source_verification(item.image_id))
                            isolated.source_verifications_.emplace(item.image_id,verification);
                        isolated.save_impl(input_path,fs::u8path(isolated.path_).parent_path().u8string(),c);
                    }
                    result.input_sha256=fingerprint_file(input_path,{},c)->sha256;
                    Project::LoadProgress load;
                    load.external_cancelled=c;
                    load.temporary_link_base_project=d.snapshot->path_;
                    auto project=Project::open_impl(input_path,false,"",&load);
                    auto doc=project->document(item.image_id);doc->set_history_limits(0,0);check();
                    const auto saved_transform=o.use_saved_stains?
                        std::optional{doc->image_analysis_settings().value().transform()}:std::nullopt;
                    const auto& transform=saved_transform?*saved_transform:d.transform;
                    const auto server=doc->server();
                    std::optional<RawRegionRequest> raw_region;
                    if(o.kind==ProjectBatchKind::Raw && (!item.object_ids.empty() || item.filtered_all_raw_objects)) {
                        raw_region=raw_batch_region(o,*server);
                        if(!raw_region) {
                            if(!server->raw_metadata(c)) throw std::invalid_argument("raw batch requires a quantitative image source");
                            throw std::invalid_argument("raw batch cannot follow display axes without a saved raw view");
                        }
                        if(item.filtered_all_raw_objects) {
                            const auto metadata=server->raw_metadata(c);
                            if(!metadata) throw std::invalid_argument("raw batch requires a quantitative image source");
                            if(raw_region->z>=metadata->z_planes || raw_region->t>=metadata->timepoints ||
                               raw_region->first_channel+raw_region->channels>metadata->channels ||
                               std::size_t(raw_region->region.level)>=metadata->levels.size())
                                throw std::invalid_argument("raw batch axes are outside the source domain");
                        }
                    }
                    auto observed_source=server->reader_source();
                    if(!input.info.embedded && !item.object_ids.empty() && observed_source && observed_source->scope=="unopened") {
                        server->require_raw_current(c);
                        observed_source=server->reader_source();
                    }
                    if(const auto verified=project->source_verification(item.image_id); verified && !item.object_ids.empty()) {
                        if(!observed_source)throw WorkflowInputChanged("The linked batch image has no observed source");
                        detail::require_verified_project_source(*verified,*observed_source,c);
                    }
                    std::optional<ScientificRawLattice> lattice;
                    if(raw_classifier_batch(o.kind) && !item.object_ids.empty()) lattice=raw_lattice(*doc,o,c);
                    const auto checkpoint_input=checkpoint_input_sha256(result.input_sha256,observed_source);
                    // Resume identity includes the project's seal version and actual linked input.
                    const auto spec=hash("qpx.batch-resume/"+std::to_string(detail::batch_checkpoint_version)+':'+d.sha+checkpoint_input+std::to_string(report.source_generation)+":"+std::to_string(stream_batch(o.kind)?0:report.source_revision));const auto folder=root/("image-"+std::to_string(item.image_id)+"-"+spec);fs::create_directories(access_path(folder));if(!plain(folder,true)) throw std::runtime_error("batch image output must be a plain directory");
                    std::unique_ptr<PixelBatchBundle> bundle;std::uint64_t stream_bytes=0;
                    auto pixel_stream=[&](bool compute,std::vector<std::uint64_t>& records,std::uint64_t& generated) {
                        Sha256 pixels;const std::string tag="qpx.batch-pixels/1";pixels.update(tag.data(),tag.size());
                        for(auto id:item.object_ids) {check();std::string digest;std::uint64_t record=0,bytes=0;
                            if(o.kind==ProjectBatchKind::Pixels) {
                                PixelPredictionStream stream(*doc,id,*o.pixel_model,o.pixels);
                                if(compute) {
                                    if(!bundle) bundle=std::make_unique<PixelBatchBundle>(folder,item.object_ids.size());
                                    auto path=bundle->output(id);if(o.pixel_total_limit-stream_bytes<4096) throw std::length_error("pixel batch exceeds total file limit");
                                    const auto value=save_pixel_stream(stream,path.u8string(),false,std::min(o.pixel_file_limit,o.pixel_total_limit-stream_bytes),c);bundle->saved(std::move(path));stream_bytes+=value.bytes;result.inferred_blocks+=value.blocks;
                                    PixelStreamFile file(bundle->output(id).u8string(),o.pixel_file_limit);file.verify(c);digest=file.input_sha256();result.verified_blocks+=file.declared_summary().blocks;result.stream_bytes+=file.declared_summary().bytes;
                                    record=doc->keep_pixel_stream(file.link(),"Pixel batch");bytes=doc->pixel_stream_record(record)->payload_bytes();
                                } else {digest=stream.input_sha256(c);result.hashed_blocks+=stream.total_blocks();}
                            }
                            else if(o.kind==ProjectBatchKind::RawPixelContours) {
                                const RawPixelTiledPredictionRequest request(*doc,*o.raw_pixel_model,lattice.value(),o.raw_pixels,{},id);
                                if(compute) {
                                    const auto value=trace_raw_pixel_stream(request,o.raw_contours,c);digest=value->input_sha256();result.inferred_blocks+=value->completed_windows();
                                    const auto used=cost(*doc,input.info.embedded);
                                    if(used>=image_budget || generated>=o.result_budget) throw std::length_error("raw contour batch exceeds image budget");
                                    const auto budget=std::min<std::uint64_t>({o.raw_contour_publication_bytes,o.result_budget-generated,image_budget-used});
                                    const auto objects=publish_raw_pixel_stream_contours(*doc,request,*value,o.raw_contour_prefix,static_cast<std::size_t>(budget));
                                    bytes=value->payload_bytes()+cost(*doc,input.info.embedded)-used+objects.size()*sizeof(std::uint64_t);
                                    if(bytes>o.result_budget-generated) throw std::length_error("raw contour batch exceeds result budget");
                                    generated+=bytes;records.insert(records.end(),objects.begin(),objects.end());
                                    if(cost(*doc,input.info.embedded)>image_budget) throw std::length_error("raw contour batch exceeds materialization budget");
                                } else {
                                    digest=request.sha256();for(std::uint64_t i=0;i<request.total_windows();++i) {check();digest=raw_pixel_stream_chain("qpx.raw-pixel-stream-input-chain/1",digest,i,request.input_sha256(i,c));}
                                    result.hashed_blocks+=request.total_windows();
                                }
                            }
                            else if(o.kind==ProjectBatchKind::RawPixels) {
                                const RawPixelTiledPredictionRequest request(*doc,*o.raw_pixel_model,lattice.value(),o.raw_pixels,{},id);
                                if(compute) {
                                    if(!bundle) bundle=std::make_unique<PixelBatchBundle>(folder,item.object_ids.size(),true);
                                    auto path=bundle->output(id);if(o.pixel_total_limit-stream_bytes<4096) throw std::length_error("raw pixel batch exceeds total file limit");
                                    RawPixelStreamSaveOptions save;save.limits=raw_file_limits(o,o.pixel_total_limit-stream_bytes);
                                    const auto value=save_raw_pixel_stream(request,path.u8string(),save,c);bundle->saved(std::move(path));stream_bytes+=value.bytes;result.inferred_blocks+=value.completed_windows;
                                    RawPixelStreamFile file(bundle->output(id).u8string(),save.limits);file.verify(c);const auto& summary=file.declared_summary();digest=summary.input_sha256;result.verified_blocks+=summary.completed_windows;result.stream_bytes+=summary.bytes;
                                    record=doc->keep_raw_pixel_stream(file.link(),"Raw pixel batch");bytes=doc->raw_pixel_stream_record(record)->payload_bytes();
                                } else {
                                    digest=request.sha256();for(std::uint64_t i=0;i<request.total_windows();++i) {check();digest=raw_pixel_stream_chain("qpx.raw-pixel-stream-input-chain/1",digest,i,request.input_sha256(i,c));}
                                    result.hashed_blocks+=request.total_windows();
                                }
                            }
                            else if(o.kind==ProjectBatchKind::Raw) {
                                const auto& region=*raw_region;
                                ScientificRawOptions options;options.first_channel=region.first_channel;options.channels=region.channels;options.z=region.z;options.t=region.t;options.work_bytes=o.raw_grid_work_bytes;
                                RawAnalysisRequest r=o.raw_grid?RawAnalysisRequest::for_grid_roi(*doc,id,*o.raw_grid,options,o.nucleus,o.raw_max_pixels):RawAnalysisRequest(*doc,region,id,o.nucleus,o.roi_tile_size,o.raw_max_pixels);
                                if(compute) {auto value=analyze_raw(r,c);digest=value->input_sha256();result.raw_tiles+=value->tiles();record=doc->keep_raw_analysis(r,value);bytes=doc->raw_analysis_record(record)->payload_bytes();}
                                else {auto value=raw_analysis_input_digest(r,c);digest=value.input_sha256;result.raw_hashed_tiles+=value.tiles;}
                            }
                            else if(o.kind==ProjectBatchKind::Roi) {auto r=o.roi_intensity_statistics?AnalysisRequest::with_intensity_statistics(*doc,id,transform,o.roi_kind,o.nucleus,o.roi_tile_size,o.roi_sampling_policy):
                                AnalysisRequest(*doc,id,transform,o.roi_kind,o.nucleus,o.roi_tile_size,o.roi_sampling_policy);if(compute) {auto value=analyze(r,c);digest=value.input_sha256;record=doc->keep_analysis(r,value);bytes=doc->analysis_record(record).payload_bytes();}else digest=input_sha256(r,c);}
                            else if(o.kind==ProjectBatchKind::Tissue) {auto r=o.tissue_grid?TissueRequest::for_grid_roi(*doc,id,transform,*o.tissue_grid,o.tissue,o.nucleus,o.tissue_min_component_area_um2,o.tissue_max_component_area_um2):
                                TissueRequest::with_component_area_limits(*doc,id,transform,o.tissue_min_component_area_um2,o.tissue_max_component_area_um2,o.tissue,o.nucleus);if(compute) {auto value=detect_tissue(r,c);digest=value->input_sha256();record=doc->keep_tissue(r,value);bytes=doc->tissue_record(record)->payload_bytes();}else digest=input_sha256(r,c);}
                            else {auto r=o.cell_nominal?CellRequest::for_nominal_roi(*doc,id,transform,o.cell_nominal->downsample,o.cell_nominal->context_x,o.cell_nominal->context_y,o.grid_cells,o.cell_nominal->sampling):o.cell_grid?CellRequest::for_grid_roi(*doc,id,transform,*o.cell_grid,o.grid_cells):CellRequest::with_nucleus_area_limits(*doc,id,transform,o.cell_min_nucleus_area_um2,o.cell_max_nucleus_area_um2,o.cells);if(compute) {auto value=detect_cells(r,c);digest=value->nuclei()->input_sha256();record=doc->keep_cells(r,value);bytes=doc->cell_record(record)->payload_bytes();if(o.publish_cells) {const auto extra=4ULL*value->retained_bytes()+value->features().size()*512ULL;if(cost(*doc,input.info.embedded)>image_budget || extra>image_budget-cost(*doc,input.info.embedded)) throw std::length_error("cell publication exceeds batch materialization budget");doc->publish_cell_record(record);}}else digest=input_sha256(r,c);}
                            pixels.integer(id);pixels.update(digest.data(),digest.size());
                            if(compute) {if(o.kind==ProjectBatchKind::RawPixelContours) {if(progress) progress->objects.fetch_add(1);continue;}if(bytes>o.result_budget-generated) throw std::length_error("batch image exceeds result budget");generated+=bytes;records.push_back(record);if(cost(*doc,input.info.embedded)>image_budget) throw std::length_error("batch results exceed materialization budget");if(progress) progress->objects.fetch_add(1);}
                        }check();return pixels.finish();
                    };
                    std::optional<detail::BatchCheckpoint> cached_meta;std::unique_ptr<detail::ReaderFileGuard> cached_guard;
                    auto find=[&](const std::string& digest)->fs::path {
                        if(stream_batch(o.kind)) {
                            std::size_t count=0;for(const auto& entry:fs::directory_iterator(access_path(folder))) {check();if(++count>100000) throw std::length_error("pixel batch directory exceeds entry limit");
                                const auto path=access_path(entry.path(),true);if(!PixelBatchBundle::owned(path) || !plain(path/"result.qpxproj",false)) continue;
                                const auto file=path/"result.qpxproj";if(fs::file_size(file)>o.checkpoint_limit) throw std::length_error("pixel batch checkpoint exceeds byte limit");
                                auto guard=std::make_unique<detail::ReaderFileGuard>(file.u8string());auto meta=detail::read_batch_checkpoint(file.u8string(),c,false,o.csv_limit,true);guard->verify();
                                if(meta.pixel_sha256==digest) {cached_meta=std::move(meta);cached_guard=std::move(guard);return file;}
                            }return {};
                        }
                        const auto preferred=folder/(digest+".qpxproj");if(fs::exists(access_path(preferred))) return preferred;
                        std::vector<fs::path> names;std::size_t count=0;for(const auto& e:fs::directory_iterator(access_path(folder))) {check();if(++count>100000) throw std::length_error("batch checkpoint directory exceeds entry limit");const auto n=e.path().filename().u8string();if(n.rfind(digest+"-",0)==0 && n.size()==105 && n.substr(n.size()-8)==".qpxproj" && plain(e.path(),false)) names.push_back(e.path());}
                        if(names.empty()) return {};std::sort(names.begin(),names.end());return names.front();
                    };
                    bool candidates=false;if(o.resume) {std::size_t count=0;for(const auto& e:fs::directory_iterator(access_path(folder))) {check();if(++count>100000) throw std::length_error("batch checkpoint directory exceeds entry limit");const auto n=e.path().filename().u8string();if(stream_batch(o.kind)?(PixelBatchBundle::owned(access_path(e.path())) && plain(access_path(e.path())/"result.qpxproj",false)):((n.size()==72 || n.size()==105) && n.substr(n.size()-8)==".qpxproj")) {candidates=true;break;}}}
                    if(candidates) {
                        phase(2);std::vector<std::uint64_t> unused;std::uint64_t bytes=0;const auto digest=pixel_stream(false,unused,bytes);const auto candidate=find(digest);
                        if(!candidate.empty()) {
                            phase(5);if(!plain(candidate,false) || fs::file_size(access_path(candidate))>o.checkpoint_limit) throw std::length_error("batch checkpoint is not plain or exceeds byte limit");auto guard=cached_guard?std::move(cached_guard):std::make_unique<detail::ReaderFileGuard>(access_path(candidate).u8string());const auto meta=cached_meta?std::move(*cached_meta):detail::read_batch_checkpoint(access_path(candidate).u8string(),c,false,o.csv_limit,true);guard->verify();
                            if(meta.parameters_sha256!=d.sha || meta.input_sha256!=checkpoint_input || meta.pixel_sha256!=digest || meta.image_id!=std::to_string(item.image_id) || meta.source_identity!=report.source_identity || meta.source_generation!=report.source_generation || (!stream_batch(o.kind) && meta.source_revision!=report.source_revision) || meta.objects!=item.object_ids.size() || meta.generated_bytes>o.result_budget) throw std::invalid_argument("batch checkpoint does not match this request");
                            const auto& properties=doc->image_data_properties();
                            const auto checkpoint_directory=candidate.parent_path();
                            detail::verify_owned_expression_files(properties,checkpoint_directory,c);
                            detail::reduction::verify_owned_files(properties,checkpoint_directory,c);
                            if(o.kind==ProjectBatchKind::Pixels) {if(cost(*doc,input.info.embedded)+meta.generated_bytes*2>image_budget) throw std::length_error("pixel checkpoint exceeds materialization budget");verify_pixels(candidate,*doc,item,o,digest,image_budget-cost(*doc,input.info.embedded),result,c);guard->verify();}
                            if(o.kind==ProjectBatchKind::RawPixels) {if(cost(*doc,input.info.embedded)+meta.generated_bytes*2>image_budget) throw std::length_error("raw pixel checkpoint exceeds materialization budget");verify_raw_pixels(candidate,*doc,item,o,lattice,digest,image_budget-cost(*doc,input.info.embedded),result,c);guard->verify();}
                            const auto records=o.kind==ProjectBatchKind::RawPixelContours?detail::batch_added_object_count(access_path(candidate).u8string(),item.image_id,doc->object_count(),c):item.object_ids.size();
                            const auto source_guards=lock_batch_source(*server,observed_source,c);
                            guard->verify();result.pixel_sha256=digest;result.path=access_path(candidate).u8string();result.records=records;result.state="reused";++report.reused;if(progress) progress->objects.fetch_add(item.object_ids.size());
                        }
                    }
                    if(result.state.empty()) {
                        phase(3);std::vector<std::uint64_t> records;std::uint64_t generated=0;result.pixel_sha256=pixel_stream(true,records,generated);result.records=records.size();detail::BatchCheckpoint meta;meta.parameters_sha256=d.sha;meta.input_sha256=checkpoint_input;meta.pixel_sha256=result.pixel_sha256;meta.source_identity=report.source_identity;meta.source_generation=report.source_generation;meta.source_revision=report.source_revision;meta.image_id=std::to_string(item.image_id);meta.image_name=input.info.name;meta.parameters_json=d.json;meta.objects=item.object_ids.size();meta.generated_bytes=generated;meta.max_file_bytes=o.checkpoint_limit;meta.source_locator=input.info.source_path;
                        if(observed_source && observed_source->primary) meta.source_file_identity=observed_source->primary->identity;
                        std::string table;Document empty(Image(1,1,{0,0,0}));const auto& table_doc=records.empty()?empty:*doc;
                        if(o.kind==ProjectBatchKind::Pixels) table=pixel_csv(*doc,records,o);
                        else if(o.kind==ProjectBatchKind::RawPixels) table=raw_pixel_csv(*doc,records,o);
                        else if(o.kind==ProjectBatchKind::RawPixelContours) table=raw_contour_csv(*doc,records,o);
                        else if(o.kind==ProjectBatchKind::Raw) table=raw_analysis_csv(RawTableRequest(table_doc,records,true,std::min<std::size_t>(image_budget,512u*1024*1024)),o.csv_limit,c);
                        else if(o.kind==ProjectBatchKind::Cells) {CellTableOptions options;options.require_current=true;options.byte_limit=o.csv_limit;options.snapshot_budget=std::min<std::size_t>(image_budget,512u*1024*1024);table=cell_measurements_csv(CellTableRequest(table_doc,records,options),c);}
                        else {MeasurementTableOptions options;options.require_current=true;options.byte_limit=o.csv_limit;options.snapshot_budget=std::min<std::size_t>(image_budget,512u*1024*1024);table=measurements_csv(MeasurementTableRequest(table_doc,o.kind==ProjectBatchKind::Roi?MeasurementTableKind::Roi:MeasurementTableKind::Tissue,records,options),c);}
                        if(o.tma_only && o.kind!=ProjectBatchKind::Raw) {const auto used=cost(*doc,input.info.embedded);if(used>=image_budget) throw std::length_error("TMA table snapshot exceeds materialization budget");const auto grid=doc->tma_grid(static_cast<std::size_t>(std::min<std::uint64_t>(image_budget-used,64*1024*1024)));if(!grid) throw std::logic_error("captured TMA grid disappeared");table=tma_context_csv(*grid,table,o.csv_limit,!stream_batch(o.kind));}
                        const auto prefix=csv_quote(report.source_identity)+','+std::to_string(report.source_generation)+','+std::to_string(report.source_revision)+','+std::to_string(item.image_id)+','+csv_quote(input.info.name)+',';meta.csv=with_context(table,prefix,o.csv_limit);
                        phase(4);const auto file=work.path/"result.qpxproj";
                        if(stream_batch(o.kind) && !bundle) bundle=std::make_unique<PixelBatchBundle>(folder,item.object_ids.size(),o.kind==ProjectBatchKind::RawPixels);
                        const auto destination_folder=bundle?bundle->path:folder;project->save_impl(file.u8string(),destination_folder.u8string(),c,nullptr,&meta);check();
                        auto destination=bundle?bundle->path/"result.qpxproj":folder/(result.pixel_sha256+".qpxproj");if(!bundle && fs::exists(access_path(destination))) destination=folder/(result.pixel_sha256+"-"+token()+".qpxproj");
                        const auto source_guards=lock_batch_source(*server,observed_source,c);
                        publish(file,destination,c);if(bundle) bundle->commit();result.path=access_path(destination).u8string();result.state="saved";++report.saved;
                    }
                } catch(const std::exception& e) {if(c && c->load()) throw;result.state="failed";result.error=error_text(e);result.records=0;++report.failed;}
                result.elapsed_ms=std::chrono::duration<double,std::milli>(Clock::now()-image_start).count();const auto bytes=result.image_name.capacity()+result.path.capacity()+result.error.capacity()+result.input_sha256.capacity()+result.pixel_sha256.capacity()+result.state.capacity();if(bytes>64*1024*1024-report_bytes) throw std::length_error("batch report exceeds memory limit");report_bytes+=bytes;report.images.push_back(result);if(progress) progress->images.store(report.images.size());if(sink) sink(result);
                if(result.state=="failed" && o.failure==ProjectBatchFailure::Stop) {report.state="failed";report.error=result.error;break;}
            }
            if(report.state=="idle") report.state=report.failed?"partial":"succeeded";
        } catch(const std::exception& e) {report.state=c && c->load()?"cancelled":"failed";report.error=error_text(e);}
        report.elapsed_ms=std::chrono::duration<double,std::milli>(Clock::now()-started).count();
        // The terminal audit is intentionally written after cancellation. It
        // publishes no additional image result and records committed images.
        if(lock && plain(root,true)) {try {phase(6);report.report_path=access_path(root/("run-"+token()+".json")).u8string();const auto text=project_batch_report_json(report);detail::AtomicExportFile file(access_path(fs::u8path(report.report_path),true).u8string(),64*1024*1024);file.write(text.data(),text.size());file.commit();} catch(const std::exception& e) {report.state="failed";report.error="cannot write batch report: "+error_text(e);report.report_path.clear();}}
        return report;
    }
};
ProjectBatchReport run_project_batch(const ProjectBatchRequest& request,const std::string& output,const std::atomic<bool>* c,const std::function<void(const ProjectBatchImageResult&)>& sink) {return ProjectBatchKernel::run(request,output,c,nullptr,sink);}
std::string project_batch_image_json(const ProjectBatchImageResult& r) {return "{\"schema\":\"qpx.batch-image/1\",\"image_id\":"+std::to_string(r.image_id)+",\"image_name\":"+json_string(r.image_name)+",\"state\":"+json_string(r.state)+",\"path\":"+json_string(r.path)+",\"error\":"+json_string(r.error)+",\"input_sha256\":"+json_string(r.input_sha256)+",\"pixel_sha256\":"+json_string(r.pixel_sha256)+",\"objects\":"+std::to_string(r.objects)+",\"records\":"+std::to_string(r.records)+",\"inferred_blocks\":"+std::to_string(r.inferred_blocks)+",\"hashed_blocks\":"+std::to_string(r.hashed_blocks)+",\"verified_blocks\":"+std::to_string(r.verified_blocks)+",\"stream_bytes\":"+std::to_string(r.stream_bytes)+",\"raw_tiles\":"+std::to_string(r.raw_tiles)+",\"raw_hashed_tiles\":"+std::to_string(r.raw_hashed_tiles)+",\"elapsed_ms\":"+real(r.elapsed_ms)+"}";}
std::string project_batch_report_json(const ProjectBatchReport& r) {
    std::string s="{\"schema\":\"qpx.project-batch/1\",\"state\":"+json_string(r.state)+",\"error\":"+json_string(r.error)+",\"report_path\":"+json_string(r.report_path)+",\"parameters_sha256\":"+json_string(r.parameters_sha256)+",\"source_identity\":"+json_string(r.source_identity)+",\"source_generation\":"+std::to_string(r.source_generation)+",\"source_revision\":"+std::to_string(r.source_revision)+",\"saved\":"+std::to_string(r.saved)+",\"reused\":"+std::to_string(r.reused)+",\"failed\":"+std::to_string(r.failed)+",\"snapshot_bytes\":"+std::to_string(r.snapshot_bytes)+",\"capture_ms\":"+real(r.capture_ms)+",\"elapsed_ms\":"+real(r.elapsed_ms)+",\"images\":[";
    bool first=true;for(const auto& image:r.images) {if(!first) s+=',';first=false;s+=project_batch_image_json(image);if(s.size()>64*1024*1024) throw std::length_error("batch report exceeds byte limit");}return s+"]}\n";
}
struct ProjectBatchJob::Impl {
    std::optional<ProjectBatchRequest> request;std::future<ProjectBatchReport> future;std::atomic<bool> cancelled{false};Progress progress;ProjectBatchStats stats;std::shared_ptr<const ProjectBatchReport> result;
};
ProjectBatchJob::ProjectBatchJob():impl_(std::make_unique<Impl>()) {}
ProjectBatchJob::~ProjectBatchJob() {cancel();try {wait();} catch(...) {}}
bool ProjectBatchJob::busy() const {return impl_->future.valid();}
void ProjectBatchJob::cancel() {impl_->cancelled.store(true);}
std::shared_ptr<const ProjectBatchReport> ProjectBatchJob::result() const {return impl_->result;}
bool ProjectBatchJob::request(const ProjectBatchRequest& request,const std::string& directory) {
    poll();if(busy()) return false;const auto output=detail::export_path(directory);detail::bounded_utf8(output,32768);impl_->request=request;impl_->cancelled.store(false);impl_->result.reset();impl_->progress.phase.store(0);impl_->progress.image.store(0);impl_->progress.images.store(0);impl_->progress.objects.store(0);impl_->stats={};impl_->stats.state="running";impl_->stats.total_images=request.image_count();impl_->stats.total_objects=request.object_count();impl_->stats.capture_ms=request.capture_ms();
    try {impl_->future=std::async(std::launch::async,[state=impl_.get(),output] {return ProjectBatchKernel::run(*state->request,output,&state->cancelled,&state->progress,{});});} catch(...) {impl_->request.reset();impl_->stats.state="failed";throw;}return true;
}
ProjectBatchStats ProjectBatchJob::poll() {
    impl_->stats.phase=phase_name(impl_->progress.phase.load());impl_->stats.image_id=impl_->progress.image.load();impl_->stats.completed_images=impl_->progress.images.load();impl_->stats.completed_objects=impl_->progress.objects.load();if(busy() && impl_->cancelled.load()) impl_->stats.state="cancelling";
    if(busy() && impl_->future.wait_for(std::chrono::seconds(0))==std::future_status::ready) {try {auto report=impl_->future.get();impl_->stats.state=report.state;impl_->stats.error=report.error;impl_->stats.elapsed_ms=report.elapsed_ms;impl_->result=std::make_shared<const ProjectBatchReport>(std::move(report));} catch(const std::exception& e) {impl_->stats.state="failed";impl_->stats.error=error_text(e);}impl_->request.reset();impl_->stats.completed_images=impl_->progress.images.load();impl_->stats.completed_objects=impl_->progress.objects.load();}
    return impl_->stats;
}
ProjectBatchStats ProjectBatchJob::wait() {if(busy()) impl_->future.wait();return poll();}
}
