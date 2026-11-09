#include "expression_normalization_internal.hpp"
#include <set>
namespace qpx {
using namespace detail::normalization;
void ExpressionQcOptions::validate(std::uint64_t features) const {
    work_limit(work_bytes);if(!features || features>1000000 || groups.size()>16)throw std::invalid_argument("QC requires1..1000000 source features and at most16 groups");
    std::set<std::string> names;std::size_t members=0;
    for(const auto& g:groups){detail::bounded_utf8(g.name,128);if(g.name.empty() || !names.insert(g.name).second || g.features.empty() || g.features.back()>=features || !std::is_sorted(g.features.begin(),g.features.end()) || std::adjacent_find(g.features.begin(),g.features.end())!=g.features.end())throw std::invalid_argument("QC groups require unique names and ascending unique in-range feature indices");if(g.features.size()>65536-members)throw std::invalid_argument("QC group memberships exceed65536");members+=g.features.size();}
}
struct ExpressionQcResult::Data {
    std::vector<std::uint64_t> totals,detected,groups;
    std::vector<ExpressionFeatureGroup> definitions;
    std::string source,file,sha;
    std::uint64_t features=0;
    std::size_t bytes=0,peak=0,batches=0;
};
ExpressionQcResult::ExpressionQcResult(std::shared_ptr<const Data> d):data_(std::move(d)){}
const std::vector<std::uint64_t>& ExpressionQcResult::totals()const{return data_->totals;}
const std::vector<std::uint64_t>& ExpressionQcResult::detected_features()const{return data_->detected;}
const std::vector<ExpressionFeatureGroup>& ExpressionQcResult::groups()const{return data_->definitions;}
const std::vector<std::uint64_t>& ExpressionQcResult::group_totals()const{return data_->groups;}
std::optional<double> ExpressionQcResult::group_fraction(std::size_t g,std::uint64_t o)const{if(g>=groups().size() || o>=totals().size())throw std::out_of_range("QC group/observation index exceeds range");if(!totals()[o])return {};return double(group_totals()[g*totals().size()+o])/double(totals()[o]);}
std::uint64_t ExpressionQcResult::features()const{return data_->features;}
const std::string& ExpressionQcResult::source_sha256()const{return data_->source;}
const std::string& ExpressionQcResult::file_sha256()const{return data_->file;}
const std::string& ExpressionQcResult::sha256()const{return data_->sha;}
std::size_t ExpressionQcResult::payload_bytes()const{return data_->bytes;}
std::size_t ExpressionQcResult::peak_work_bytes()const{return data_->peak;}
std::size_t ExpressionQcResult::batches()const{return data_->batches;}
std::shared_ptr<const ExpressionQcResult> expression_qc(std::shared_ptr<const ExpressionMatrixSource> source,const ExpressionQcOptions& options,const std::atomic<bool>* cancelled){
    if(!source)throw std::invalid_argument("QC requires a native matrix source");raw(*source);options.validate(source->metadata().features);workflow_checkpoint(cancelled);source->require_current(cancelled);
    const auto& m=source->metadata();const auto observations=std::size_t(m.observations);const auto work=options.work_bytes;
    std::size_t fixed=sizeof(ExpressionQcResult)+sizeof(ExpressionQcResult::Data)+source->payload_bytes()+2*group_bytes(options.groups)+512;
    charge(fixed,observations,8*(2+options.groups.size()),work);
    if(work-fixed<4u*1024*1024+8192)throw WorkflowCaptureLimit("QC retained totals leave no bounded read workspace");
    auto d=std::make_shared<ExpressionQcResult::Data>();d->source=m.source_sha256;d->file=source->file_sha256();d->features=m.features;d->definitions=options.groups;
    d->totals.resize(observations);d->detected.resize(observations);d->groups.resize(observations*options.groups.size());
    d->bytes=sizeof(ExpressionQcResult)+sizeof(*d)+group_bytes(d->definitions)+(d->totals.capacity()+d->detected.capacity()+d->groups.capacity())*8+d->source.capacity()+d->file.capacity()+64;
    fixed=d->bytes+source->payload_bytes()+group_bytes(options.groups);if(fixed>=work || work-fixed<4u*1024*1024+8192)throw WorkflowCaptureLimit("QC actual retained capacity leaves no read workspace");
    const auto available=work-fixed;
    // Bound a dense worst-case batch as well as vector growth/cache overhead.
    // Sparse real matrices still amortize connection/prepared-statement setup.
    const auto batch=std::max<std::size_t>(1,std::min<std::size_t>(128,(available/8)/(observations*32)));
    const auto read_work=available-batch*8-4096;d->peak=fixed+read_work+batch*8+4096;
    const auto add=[](std::uint64_t& total,std::uint64_t value){if(value>std::numeric_limits<std::uint64_t>::max()-total)throw std::overflow_error("Expression QC exact uint64 total overflow");total+=value;};
    for(std::uint64_t first=0;first<m.features;first+=batch){
        workflow_checkpoint(cancelled);ExpressionMatrixSelection selection;selection.features.reserve(batch);for(auto f=first;f<std::min<std::uint64_t>(m.features,first+batch);++f)selection.features.push_back(f);
        const auto block=source->read(selection,read_work,cancelled);++d->batches;
        if(block.source_sha256!=m.source_sha256 || block.domain!=ExpressionValueDomain::Counts || !block.continuous.empty() || block.features!=selection.features || block.offsets.size()!=block.features.size()+1 || block.offsets.front()!=0 || block.offsets.back()!=block.counts.size() || block.counts.size()!=block.observation_indices.size())throw std::invalid_argument("QC sparse source block shape/domain differs");
        for(std::size_t f=0;f<block.features.size();++f){
            if(block.offsets[f]>block.offsets[f+1])throw std::invalid_argument("QC sparse offsets decrease");std::array<bool,16> member{};
            for(std::size_t g=0;g<options.groups.size();++g)member[g]=std::binary_search(options.groups[g].features.begin(),options.groups[g].features.end(),block.features[f]);
            std::optional<std::uint64_t> previous;
            for(auto i=block.offsets[f];i<block.offsets[f+1];++i){
                if((i&4095)==0)workflow_checkpoint(cancelled);const auto o=block.observation_indices[i],value=block.counts[i];if(o>=m.observations || (previous && o<=*previous))throw std::invalid_argument("QC sparse observation axis invalid");previous=o;
                add(d->totals[o],value);if(value)++d->detected[o];for(std::size_t g=0;g<options.groups.size();++g)if(member[g])add(d->groups[g*observations+o],value);
            }
        }
    }
    source->require_current(cancelled);Sha256 h;text(h,"qpx.expression-qc/1");text(h,d->source);text(h,d->file);h.integer(m.features);h.integer(m.observations);h.integer(d->definitions.size());
    for(const auto& g:d->definitions){text(h,g.name);h.integer(g.features.size());for(auto f:g.features)h.integer(f);}
    for(const auto* values:{&d->totals,&d->detected,&d->groups})for(std::size_t i=0;i<values->size();++i){if((i&4095)==0)workflow_checkpoint(cancelled);h.integer((*values)[i]);}d->sha=h.finish();
    return std::shared_ptr<const ExpressionQcResult>(new ExpressionQcResult(std::move(d)));
}
}
