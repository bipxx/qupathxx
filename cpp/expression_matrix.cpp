#include "expression_matrix_internal.hpp"
#include "reader_file_guard.hpp"
#include "qupathxx/file_fingerprint.hpp"
#include <algorithm>
namespace qpx {
#ifdef QUPATHXX_HAS_PROJECT
namespace {
using namespace detail::matrix;
class IndexedMatrix final:public ExpressionMatrixSource {
    ExpressionMatrixMetadata metadata_;
    std::string path_,file_hash_;
    std::shared_ptr<const FileVersion> version_;
    std::optional<std::uint64_t> index(const char* sql,const std::string& id,const std::atomic<bool>* c)const{detail::spatial_identifier(id);require_current(c);Database db(path_,false,64u*1024*1024,c);Statement query(db,sql);query.text(1,id);std::optional<std::uint64_t> result;if(query.step()){result=query.number(0);if(query.step())throw std::invalid_argument("Expression ID index is not unique");}db.admit(payload_bytes()+id.capacity());require_current(c);return result;}
public:
    IndexedMatrix(std::string path,std::string file_hash,ExpressionMatrixMetadata metadata,std::shared_ptr<const FileVersion> version):metadata_(std::move(metadata)),path_(std::move(path)),file_hash_(std::move(file_hash)),version_(std::move(version)){}
    const ExpressionMatrixMetadata& metadata()const override{return metadata_;}
    const std::string& path()const override{return path_;}
    const std::string& file_sha256()const override{return file_hash_;}
    std::size_t payload_bytes()const override{std::size_t n=sizeof(*this)+path_.capacity()+file_hash_.capacity()+version_->payload_bytes()+metadata_.source_sha256.capacity()+metadata_.input_profile.capacity()+metadata_.value_domain.capacity()+metadata_.spatial_sha256.capacity()+metadata_.input_matrix.capacity()+metadata_.input_spatial.capacity();if(metadata_.spatial_frame)n+=metadata_.spatial_frame->id.capacity()+metadata_.spatial_frame->source_id.capacity();for(const auto& s:metadata_.input_sha256)n+=s.capacity();return n;}
    void require_current(const std::atomic<bool>* c)const override{workflow_checkpoint(c);no_sidecars(path_);try{const auto now=detail::workflow_file_version(path_);same_version(*version_,*now);}catch(const WorkflowInputChanged&){throw;}catch(const std::exception& e){throw WorkflowInputChanged(std::string("Expression sidecar unavailable: ")+e.what());}workflow_checkpoint(c);}
    std::vector<ExpressionFeature> feature_page(std::uint64_t first,std::size_t count,std::size_t work,const std::atomic<bool>* c)const override{
        work_limit(work);range(first,count,metadata_.features);require_current(c);Database db(path_,false,work,c);db.admit(payload_bytes()+count*(sizeof(ExpressionFeature)+1024));std::vector<ExpressionFeature> result;result.reserve(count);Statement query(db,"SELECT idx,id,name,type,sha256 FROM features WHERE idx>=? AND idx<? ORDER BY idx");query.number(1,first);query.number(2,first+count);
        while(query.step()){workflow_checkpoint(c);auto f=feature(query);if(f.index!=first+result.size() || result.size()==count)throw std::invalid_argument("Expression feature page index differs");result.push_back(std::move(f));}if(result.size()!=count)throw std::invalid_argument("Expression feature catalogue truncated");db.admit(payload_bytes()+count*(sizeof(ExpressionFeature)+1024));require_current(c);return result;
    }
    std::vector<std::string> observation_page(std::uint64_t first,std::size_t count,std::size_t work,const std::atomic<bool>* c)const override{
        work_limit(work);range(first,count,metadata_.observations);require_current(c);Database db(path_,false,work,c);db.admit(payload_bytes()+count*(sizeof(std::string)+512));std::vector<std::string> result;result.reserve(count);Statement query(db,"SELECT idx,id FROM observations WHERE idx>=? AND idx<? ORDER BY idx");query.number(1,first);query.number(2,first+count);while(query.step()){workflow_checkpoint(c);if(query.number(0)!=first+result.size() || result.size()==count)throw std::invalid_argument("Expression observation page index differs");result.push_back(detail::spatial_identifier(query.text(1)));}if(result.size()!=count)throw std::invalid_argument("Expression observation catalogue truncated");db.admit(payload_bytes()+count*(sizeof(std::string)+512));require_current(c);return result;
    }
    std::vector<ExpressionObservation> spatial_page(std::uint64_t first,std::size_t count,std::size_t work,const std::atomic<bool>* c)const override {
        work_limit(work);range(first,count,metadata_.observations);if(!metadata_.spatial_frame)throw std::invalid_argument("Expression sidecar has no embedded coordinates");require_current(c);Database db(path_,false,work,c);db.admit(payload_bytes()+count*(sizeof(ExpressionObservation)+512));
        const auto frame_sha=metadata_.spatial_frame->sha256();std::vector<ExpressionObservation> result;result.reserve(count);Statement query(db,"SELECT c.observation,o.id,c.x,c.y,c.sha256 FROM coordinates c JOIN observations o ON o.idx=c.observation WHERE c.observation>=? AND c.observation<? ORDER BY c.observation");query.number(1,first);query.number(2,first+count);
        while(query.step()){workflow_checkpoint(c);const auto i=query.number(0);if(i!=first+result.size() || result.size()==count)throw std::invalid_argument("Expression coordinate axis truncated/duplicate");ExpressionObservation row{0,detail::spatial_identifier(query.text(1)),{real_value(query.value(2)),real_value(query.value(3))}};if(coordinate_hash(i,row.id,row.position,frame_sha)!=query.text(4,64))throw std::invalid_argument("Expression coordinate row identity differs");result.push_back(std::move(row));}
        if(result.size()!=count)throw std::invalid_argument("Expression coordinate page incomplete");db.admit(payload_bytes()+count*(sizeof(ExpressionObservation)+512));require_current(c);return result;
    }
    ExpressionFeaturePage search_features(const std::string& term,std::uint64_t offset,std::size_t count,
        std::size_t work,const std::atomic<bool>* c)const override {
        work_limit(work);detail::bounded_utf8(term,256);
        if(!count || count>256 || offset>metadata_.features)throw std::invalid_argument("Invalid expression catalogue search page");
        require_current(c);Database db(path_,false,work,c);
        const auto fixed=payload_bytes()+sizeof(ExpressionFeaturePage)+term.capacity()+count*(sizeof(ExpressionFeature)+1024);
        db.admit(fixed);ExpressionFeaturePage result;result.offset=offset;
        Statement total(db,"SELECT count(*) FROM features WHERE instr(lower(id),lower(?))>0 OR instr(lower(name),lower(?))>0");
        total.text(1,term);total.text(2,term);
        if(!total.step())throw std::invalid_argument("Expression catalogue count unavailable");result.matches=total.number(0);
        if(result.matches>metadata_.features || (term.empty() && result.matches!=metadata_.features))throw std::invalid_argument("Expression catalogue search differs from source axis");
        Statement query(db,"SELECT idx,id,name,type,sha256 FROM features WHERE instr(lower(id),lower(?))>0 OR instr(lower(name),lower(?))>0 ORDER BY idx LIMIT ? OFFSET ?");
        query.text(1,term);query.text(2,term);query.number(3,count);query.number(4,offset);result.features.reserve(count);
        while(query.step()) {
            workflow_checkpoint(c);if(result.features.size()==count)throw std::invalid_argument("Expression catalogue page exceeds request");
            auto item=feature(query);
            if(item.index>=metadata_.features || (!result.features.empty() && item.index<=result.features.back().index))throw std::invalid_argument("Expression catalogue search index differs");
            result.features.push_back(std::move(item));
        }
        if(result.features.size()!=std::min<std::uint64_t>(count,offset<result.matches?result.matches-offset:0))throw std::invalid_argument("Expression catalogue search page incomplete");
        db.admit(fixed);require_current(c);return result;
    }
    std::optional<std::uint64_t> feature_index(const std::string& id,const std::atomic<bool>* c)const override{auto i=index("SELECT idx FROM features WHERE id=?",id,c);if(i && *i>=metadata_.features)throw std::invalid_argument("Expression feature lookup exceeds dimensions");return i;}
    std::optional<std::uint64_t> observation_index(const std::string& id,const std::atomic<bool>* c)const override{auto i=index("SELECT idx FROM observations WHERE id=?",id,c);if(i && *i>=metadata_.observations)throw std::invalid_argument("Expression observation lookup exceeds dimensions");return i;}
    std::vector<std::uint64_t> observation_indices(const std::vector<std::string>& ids,std::size_t work,const std::atomic<bool>* c)const override {
        work_limit(work);workflow_checkpoint(c);
        if(ids.size()>1000000)throw WorkflowCaptureLimit("Expression ID join exceeds one million observations");
        std::size_t fixed=payload_bytes();workflow_capture_charge(fixed,ids.capacity()*sizeof(std::string),work);
        workflow_capture_charge(fixed,ids.size()*sizeof(std::uint64_t),work);
        for(const auto& id:ids){detail::spatial_identifier(id);workflow_capture_charge(fixed,id.capacity(),work);}
        require_current(c);Database db(path_,false,work,c);db.admit(fixed);
        std::vector<std::uint64_t> result;result.reserve(ids.size());
        Statement query(db,"SELECT idx FROM observations WHERE id=?");
        for(const auto& id:ids) {
            workflow_checkpoint(c);query.text(1,id);
            if(!query.step())throw std::invalid_argument("Expression observation ID absent from matrix: "+id);
            const auto value=query.number(0);
            if(value>=metadata_.observations || query.step())throw std::invalid_argument("Expression observation ID index differs");
            result.push_back(value);query.reset();
        }
        db.admit(fixed);require_current(c);return result;
    }
    ExpressionSparseBlock read(const ExpressionMatrixSelection& selection,std::size_t work,const std::atomic<bool>* c)const override{
        work_limit(work);workflow_checkpoint(c);if(selection.features.empty() || selection.features.size()>4096)throw std::invalid_argument("Expression read requires1..4096 explicit features");
        std::size_t fixed=payload_bytes()+sizeof(ExpressionSparseBlock)+65536;workflow_capture_charge(fixed,selection.features.capacity()*sizeof(std::uint64_t),work);if(selection.observations)workflow_capture_charge(fixed,selection.observations->capacity()*sizeof(std::uint64_t),work);
        if(selection.observations){const auto& ids=*selection.observations;if((!ids.empty() && ids.back()>=metadata_.observations) || !std::is_sorted(ids.begin(),ids.end()) || std::adjacent_find(ids.begin(),ids.end())!=ids.end())throw std::invalid_argument("Expression cohort requires sorted unique in-range observations");}
        workflow_capture_charge(fixed,selection.features.size()*3*sizeof(std::uint64_t)+(selection.features.size()+1)*sizeof(std::uint64_t),work);auto sorted=selection.features;std::sort(sorted.begin(),sorted.end());if(sorted.back()>=metadata_.features || std::adjacent_find(sorted.begin(),sorted.end())!=sorted.end())throw std::invalid_argument("Expression feature selection is duplicate/out of range");
        require_current(c);Database db(path_,false,work,c);db.admit(fixed);ExpressionSparseBlock out;out.domain=metadata_.domain;out.source_sha256=metadata_.source_sha256;out.features=selection.features;out.offsets.reserve(out.features.size()+1);out.offsets.push_back(0);
        Statement catalogue(db,"SELECT idx,id,name,type,sha256 FROM features WHERE idx=?");Statement query(db,"SELECT observation,value FROM counts WHERE feature=? AND observation>=? AND observation<=? ORDER BY observation");
        const auto live=[&]{return fixed+(out.observation_indices.capacity()+out.counts.capacity()+out.continuous.capacity())*sizeof(std::uint64_t);};
        const auto grow=[&]{const auto capacity=out.domain==ExpressionValueDomain::Counts?out.counts.capacity():out.continuous.capacity();if(out.observation_indices.size()!=capacity)return;const auto next=std::max<std::size_t>(256,capacity*2);if(next>work/sizeof(std::uint64_t))throw WorkflowCaptureLimit("Expression sparse result exceeds budget");db.admit(live()+next*sizeof(std::uint64_t));out.observation_indices.reserve(next);db.admit(live()+next*sizeof(std::uint64_t));if(out.domain==ExpressionValueDomain::Counts)out.counts.reserve(next);else out.continuous.reserve(next);};
        for(auto id:selection.features){workflow_checkpoint(c);catalogue.number(1,id);if(!catalogue.step())throw std::invalid_argument("Selected expression feature absent");const auto f=feature(catalogue);if(catalogue.step())throw std::invalid_argument("Expression feature index not unique");catalogue.reset();GeneHash hash(f,metadata_.input_sha256[2]);
            if(!selection.observations || !selection.observations->empty()){const std::uint64_t lo=0,hi=metadata_.observations-1;query.number(1,id);query.number(2,lo);query.number(3,hi);std::optional<std::uint64_t> previous;std::size_t seen=0;
                while(query.step()){if((seen++&1023)==0){workflow_checkpoint(c);db.admit(live());}const auto observation=query.number(0),value=query.value(1);if(observation>=metadata_.observations || (previous && observation<=*previous))throw std::invalid_argument("Expression sparse observation index invalid");previous=observation;const double continuous_value=out.domain==ExpressionValueDomain::Continuous?real_value(value):0;hash.add(observation,value);if(selection.observations && !std::binary_search(selection.observations->begin(),selection.observations->end(),observation))continue;grow();out.observation_indices.push_back(observation);if(out.domain==ExpressionValueDomain::Counts)out.counts.push_back(value);else out.continuous.push_back(continuous_value);}
                query.reset();}
            if((!selection.observations || !selection.observations->empty()) && hash.finish()!=f.sha256)throw std::invalid_argument("Expression gene chunk identity differs");out.offsets.push_back(out.observation_indices.size());
        }
        Sha256 digest;text(digest,out.domain==ExpressionValueDomain::Counts?"qpx.expression-sparse-block/1":"qpx.expression-sparse-block/2");text(digest,out.source_sha256);digest.integer(selection.observations.has_value());if(selection.observations){digest.integer(selection.observations->size());for(auto id:*selection.observations)digest.integer(id);}digest.integer(out.features.size());for(auto id:out.features)digest.integer(id);for(auto offset:out.offsets)digest.integer(offset);PairHash pairs(digest);for(std::size_t i=0;i<out.observation_indices.size();++i){if((i&4095)==0)workflow_checkpoint(c);pairs.add(out.observation_indices[i],out.domain==ExpressionValueDomain::Counts?out.counts[i]:real_bits(out.continuous[i]));}pairs.flush();out.sha256=digest.finish();db.admit(live());require_current(c);return out;
    }
};
}
std::shared_ptr<const ExpressionMatrixSource> open_expression_matrix(const std::string& path,const std::string& expected,std::size_t work,std::uint64_t bytes,const std::atomic<bool>* cancelled){
    work_limit(work);if(!bytes || bytes>64ull*1024*1024*1024)throw std::invalid_argument("Expression store byte limit invalid");if(!expected.empty())digest(expected);workflow_checkpoint(cancelled);const auto input=detail::export_path(path);no_sidecars(input);detail::ReaderFileGuard guard(input);if(guard.version()->bytes>bytes)throw WorkflowCaptureLimit("Expression sidecar exceeds file byte limit");
    FileFingerprintOptions options;options.byte_limit=bytes;options.buffer_bytes=65536;const auto fingerprint=fingerprint_file(input,options,cancelled);if(!expected.empty() && fingerprint->sha256!=expected)throw WorkflowInputChanged("Expression sidecar exact file digest differs");guard.verify();Database db(input,false,work,cancelled);auto metadata=load_meta(db);bind_file_identity(metadata,fingerprint->sha256);db.admit(65536+input.capacity()+sizeof(IndexedMatrix));guard.verify();no_sidecars(input);return std::make_shared<const IndexedMatrix>(input,fingerprint->sha256,std::move(metadata),guard.version());
}
#else
std::shared_ptr<const ExpressionMatrixSource> open_expression_matrix(const std::string&,const std::string&,std::size_t,std::uint64_t,const std::atomic<bool>*){throw std::runtime_error("Expression matrix storage requires native project/SQLite support");}
#endif
}
