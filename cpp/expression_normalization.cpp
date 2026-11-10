#include "expression_normalization_internal.hpp"
namespace qpx {
using namespace detail::normalization;
void ExpressionNormalizationOptions::validate()const{if(!std::isfinite(target_sum) || target_sum<1 || target_sum>1e12)throw std::invalid_argument("Library-size target must be finite in1..1e12");}
namespace {
class NormalizedMatrix final:public ExpressionMatrixSource {
    std::shared_ptr<const ExpressionMatrixSource> parent_;
    std::shared_ptr<const ExpressionQcResult> qc_;
    ExpressionNormalizationOptions options_;ExpressionMatrixMetadata metadata_;
    std::string empty_;
    std::size_t retained()const{std::size_t n=sizeof(*this)+qc_->payload_bytes()+metadata_.source_sha256.capacity()+metadata_.value_domain.capacity()+metadata_.input_profile.capacity()+metadata_.spatial_sha256.capacity()+metadata_.input_matrix.capacity()+metadata_.input_spatial.capacity();for(const auto& s:metadata_.input_sha256)n+=s.capacity();if(metadata_.spatial_frame)n+=metadata_.spatial_frame->id.capacity()+metadata_.spatial_frame->source_id.capacity();return n;}
    std::size_t remaining(std::size_t work)const{work_limit(work);const auto n=retained();if(n>=work || work-n<4u*1024*1024)throw WorkflowCaptureLimit("Normalized source leaves no indexed read workspace");return work-n;}
    void feature_identity(ExpressionFeature& f)const{Sha256 h;text(h,"qpx.normalized-feature/1");text(h,metadata_.source_sha256);text(h,f.sha256);f.sha256=h.finish();}
public:
    NormalizedMatrix(std::shared_ptr<const ExpressionMatrixSource> p,std::shared_ptr<const ExpressionQcResult> q,ExpressionNormalizationOptions options,const std::string& recipe):parent_(std::move(p)),qc_(std::move(q)),options_(options),metadata_(parent_->metadata()){
        metadata_.domain=ExpressionValueDomain::Continuous;metadata_.input_profile="qpx.library-size/1";metadata_.value_domain=options_.log1p?"log1p-library-size-normalized":"library-size-normalized";metadata_.source_sha256=recipe.substr(recipe.size()-64);
    }
    const ExpressionMatrixMetadata& metadata()const override{return metadata_;}
    const std::string& path()const override{return empty_;}
    const std::string& file_sha256()const override{return empty_;}
    std::size_t payload_bytes()const override{return retained()+parent_->payload_bytes();}
    void require_current(const std::atomic<bool>* c)const override{parent_->require_current(c);}
    std::vector<ExpressionFeature> feature_page(std::uint64_t first,std::size_t n,std::size_t work,const std::atomic<bool>* c)const override{auto result=parent_->feature_page(first,n,remaining(work),c);for(auto& f:result)feature_identity(f);return result;}
    ExpressionFeaturePage search_features(const std::string& query,std::uint64_t offset,std::size_t n,std::size_t work,const std::atomic<bool>* c)const override{auto result=parent_->search_features(query,offset,n,remaining(work),c);for(auto& f:result.features)feature_identity(f);return result;}
    std::vector<std::string> observation_page(std::uint64_t first,std::size_t n,std::size_t work,const std::atomic<bool>* c)const override{return parent_->observation_page(first,n,remaining(work),c);}
    std::vector<ExpressionObservation> spatial_page(std::uint64_t first,std::size_t n,std::size_t work,const std::atomic<bool>* c)const override{return parent_->spatial_page(first,n,remaining(work),c);}
    std::optional<std::uint64_t> feature_index(const std::string& id,const std::atomic<bool>* c)const override{return parent_->feature_index(id,c);}
    std::optional<std::uint64_t> observation_index(const std::string& id,const std::atomic<bool>* c)const override{return parent_->observation_index(id,c);}
    std::vector<std::uint64_t> observation_indices(const std::vector<std::string>& ids,std::size_t work,const std::atomic<bool>* c)const override{return parent_->observation_indices(ids,remaining(work),c);}
    ExpressionSparseBlock read(const ExpressionMatrixSelection& selection,std::size_t work,const std::atomic<bool>* c)const override{
        const auto budget=remaining(work);if(budget<8u*1024*1024)throw WorkflowCaptureLimit("Normalized read requires space for raw and derived blocks");
        auto out=parent_->read(selection,budget/2,c);const auto count=out.counts.size();
        if(out.domain!=ExpressionValueDomain::Counts || !out.continuous.empty() || out.source_sha256!=qc_->source_sha256() || out.observation_indices.size()!=count)throw std::invalid_argument("Normalized parent block shape/domain differs");
        std::size_t live=payload_bytes()+sizeof(out)+out.source_sha256.capacity()+out.sha256.capacity();charge(live,out.features.capacity()+out.offsets.capacity()+out.observation_indices.capacity()+out.counts.capacity()+count,8,work);
        out.continuous.reserve(count);Sha256 h;text(h,"qpx.normalized-sparse-block/1");text(h,metadata_.source_sha256);text(h,out.sha256);
        for(std::size_t i=0;i<count;++i){if((i&4095)==0)workflow_checkpoint(c);const auto o=out.observation_indices[i];if(o>=qc_->totals().size() || out.counts[i]>qc_->totals()[o])throw std::invalid_argument("Normalized count exceeds captured library total");const auto total=qc_->totals()[o];double value=total?(double(out.counts[i])/double(total))*options_.target_sum:0;if(options_.log1p)value=std::log1p(value);if(!std::isfinite(value))throw std::overflow_error("Normalized expression value is not finite");out.continuous.push_back(value);h.integer(bits(value));}
        std::vector<std::uint64_t>().swap(out.counts);out.domain=ExpressionValueDomain::Continuous;out.source_sha256=metadata_.source_sha256;out.sha256=h.finish();parent_->require_current(c);return out;
    }
};
}
struct ExpressionNormalization::Data {
    std::shared_ptr<const ExpressionMatrixSource> source;
    std::shared_ptr<const ExpressionQcResult> qc;
    ExpressionNormalizationOptions options;
    std::string recipe;
};
ExpressionNormalization::ExpressionNormalization(std::shared_ptr<const Data> d):data_(std::move(d)){}
const std::shared_ptr<const ExpressionMatrixSource>& ExpressionNormalization::source()const{return data_->source;}
const std::shared_ptr<const ExpressionQcResult>& ExpressionNormalization::qc()const{return data_->qc;}
const ExpressionNormalizationOptions& ExpressionNormalization::options()const{return data_->options;}
const std::string& ExpressionNormalization::recipe()const{return data_->recipe;}
ExpressionNormalization normalize_expression(std::shared_ptr<const ExpressionMatrixSource> source,std::shared_ptr<const ExpressionQcResult> qc,ExpressionNormalizationOptions options,std::size_t work,const std::atomic<bool>* cancelled){
    if(!source || !qc)throw std::invalid_argument("Normalization requires retained source and QC");raw(*source);options.validate();work_limit(work);source->require_current(cancelled);
    if(qc->source_sha256()!=source->metadata().source_sha256 || qc->file_sha256()!=source->file_sha256() || qc->features()!=source->metadata().features || qc->totals().size()!=source->metadata().observations)throw std::invalid_argument("QC does not belong to the exact parent matrix");
    std::size_t live=source->payload_bytes();workflow_capture_charge(live,qc->payload_bytes(),work);if(work-live<8u*1024*1024)throw WorkflowCaptureLimit("Normalization leaves no read workspace");
    auto d=std::make_shared<ExpressionNormalization::Data>();d->qc=std::move(qc);d->options=options;d->recipe=recipe(*d->qc,options);workflow_capture_charge(live,d->recipe.capacity()+sizeof(*d),work);
    d->source=std::make_shared<const NormalizedMatrix>(std::move(source),d->qc,options,d->recipe);return ExpressionNormalization(std::move(d));
}
ExpressionNormalization replay_expression_normalization(std::shared_ptr<const ExpressionMatrixSource> source,const std::string& saved,std::size_t work,const std::atomic<bool>* cancelled){
    work_limit(work);workflow_checkpoint(cancelled);std::size_t reserved=8u*1024*1024;workflow_capture_charge(reserved,saved.capacity(),work);const auto record=detail::normalization::parse_recipe(saved);if(!source || source->metadata().source_sha256!=record.source || source->file_sha256()!=record.file || source->metadata().features!=record.features || source->metadata().observations!=record.observations)throw std::invalid_argument("Saved normalization parent source differs");
    auto options=record.qc_options;workflow_capture_charge(reserved,group_bytes(options.groups),work);options.work_bytes=work-reserved;auto qc=expression_qc(source,options,cancelled);if(qc->sha256()!=record.qc)throw std::invalid_argument("Saved normalization QC identity differs");auto result=normalize_expression(std::move(source),std::move(qc),record.options,work-reserved,cancelled);if(result.recipe()!=saved)throw std::invalid_argument("Saved normalization recipe is noncanonical");return result;
}
}
