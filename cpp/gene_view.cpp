#include "qupathxx/gene_view.hpp"
#include "qupathxx/workflow.hpp"
#include "utf8_text.hpp"
#include <algorithm>
#include <charconv>
#include <cmath>
#include <future>
#include <limits>
#include <string_view>

namespace qpx {
namespace {
void feature_name(const std::string& id) {
    detail::bounded_utf8(id,4096);
    if(id.find_first_of("\t\r\n")!=std::string::npos)throw std::invalid_argument("Invalid gene-view feature ID");
}
template<class T> std::string text(T value) {
    char buffer[96];const auto result=std::to_chars(buffer,buffer+sizeof(buffer),value);
    if(result.ec!=std::errc{})throw std::invalid_argument("Gene-view preference number overflow");return {buffer,result.ptr};
}
template<class T> T number(std::string_view value) {
    T result{};const auto parsed=std::from_chars(value.data(),value.data()+value.size(),result);
    if(value.empty() || parsed.ec!=std::errc{} || parsed.ptr!=value.data()+value.size())throw std::invalid_argument("Invalid gene-view preference number");return result;
}
std::string folded(std::string value) {for(auto& c:value)if(c>='A' && c<='Z')c=char(c-'A'+'a');return value;}
}
void GeneViewOptions::validate() const {
    for(const auto& gene:genes)feature_name(gene);display.validate();
    if(!std::isfinite(radius) || radius<1 || radius>20)throw std::invalid_argument("Gene glyph radius must be1..20 screen pixels");
}
std::string gene_view_preference(const GeneViewOptions& options) {
    options.validate();std::vector<std::string> fields{"qpx.gene-view/1",text(int(options.enabled)),text(int(options.compare)),text(int(options.shared_range)),options.genes[0],options.genes[1],
        text(int(options.display.transform)),text(int(options.display.range)),text(int(options.display.palette)),text(options.display.lower),text(options.display.upper),
        text(options.display.lower_percentile),text(options.display.upper_percentile),text(options.display.opacity),text(options.radius)};
    for(auto v:options.display.missing_color)fields.push_back(text(int(v)));
    for(auto v:options.display.zero_color)fields.push_back(text(int(v)));
    std::string result;for(const auto& field:fields) {if(!result.empty())result+='\t';result+=field;}return result;
}
GeneViewOptions parse_gene_view_preference(const std::string& input) {
    if(input.size()>16384 || input.find_first_of("\r\n")!=std::string::npos)throw std::invalid_argument("Invalid gene-view preference line");
    std::vector<std::string_view> fields;std::string_view remaining=input;
    for(;;) {const auto at=remaining.find('\t');fields.push_back(remaining.substr(0,at));if(at==std::string_view::npos)break;remaining.remove_prefix(at+1);}
    if(fields.size()!=23 || fields[0]!="qpx.gene-view/1")throw std::invalid_argument("Unknown gene-view preference schema/fields");
    const auto flag=[&](std::size_t at) {const auto n=number<int>(fields[at]);if(n!=0 && n!=1)throw std::invalid_argument("Invalid gene-view flag");return bool(n);};
    GeneViewOptions options;options.enabled=flag(1);options.compare=flag(2);options.shared_range=flag(3);
    options.genes={std::string(fields[4]),std::string(fields[5])};options.display.transform=static_cast<GeneDisplayTransform>(number<int>(fields[6]));
    options.display.range=static_cast<GeneDisplayRange>(number<int>(fields[7]));options.display.palette=static_cast<GeneDisplayPalette>(number<int>(fields[8]));
    options.display.lower=number<double>(fields[9]);options.display.upper=number<double>(fields[10]);options.display.lower_percentile=number<double>(fields[11]);
    options.display.upper_percentile=number<double>(fields[12]);options.display.opacity=number<double>(fields[13]);options.radius=number<double>(fields[14]);
    for(std::size_t i=0;i<8;++i) {const auto n=number<int>(fields[15+i]);if(n<0 || n>255)throw std::invalid_argument("Invalid gene-view color");
        (i<4?options.display.missing_color[i]:options.display.zero_color[i-4])=static_cast<std::uint8_t>(n);}
    options.validate();return options;
}
GeneFeaturePage gene_feature_page(const ExpressionDataset& dataset,const std::string& query,std::size_t page,std::size_t size) {
    return gene_feature_page(dataset.features(),query,page,size);
}
GeneFeaturePage gene_feature_page(const std::vector<std::string>& features,const std::string& query,std::size_t page,std::size_t size) {
    feature_name(query);if(!size || size>256 || page>std::numeric_limits<std::size_t>::max()/size)throw std::invalid_argument("Invalid gene catalogue page");
    const auto needle=folded(query);GeneFeaturePage result;result.page=page;const auto start=page*size;
    for(std::size_t i=0;i<features.size();++i)if(folded(features[i]).find(needle)!=std::string::npos) {
        if(result.matches>=start && result.indices.size()<size)result.indices.push_back(i);++result.matches;
    }
    return result;
}
std::string gene_workspace_preference(const GeneViewOptions& view,const std::string& source) {
    if(!source.empty() && (source.size()!=64 || source.find_first_not_of("0123456789abcdef")!=std::string::npos))
        throw std::invalid_argument("Invalid workspace expression-source identity");
    return "qpx.gene-workspace/1\t"+source+"\t"+gene_view_preference(view);
}
GeneWorkspacePreference parse_gene_workspace_preference(const std::string& input) {
    const std::string prefix="qpx.gene-workspace/1\t";
    if(input.compare(0,prefix.size(),prefix)!=0)return {parse_gene_view_preference(input),{}};
    if(input.size()>16384)throw std::invalid_argument("Gene workspace preference exceeds limit");
    const auto end=input.find('\t',prefix.size());if(end==std::string::npos)throw std::invalid_argument("Truncated gene workspace preference");
    GeneWorkspacePreference result{parse_gene_view_preference(input.substr(end+1)),input.substr(prefix.size(),end-prefix.size())};
    gene_workspace_preference(result.view,result.source_id);return result;
}
struct GeneViewSession::Data {
    std::thread::id owner=std::this_thread::get_id();std::size_t work;
    GeneViewOptions options;std::optional<ExpressionDataset> dataset;std::uint64_t generation=0,running=0;
    bool pending=false;std::atomic<bool> cancelled{false};std::future<GeneDisplayPair> future;
    std::shared_ptr<const GeneDisplayPair> result;std::string error;
    void changed() {
        if(generation==std::numeric_limits<std::uint64_t>::max())throw std::overflow_error("Gene-view generation exhausted");
        ++generation;cancelled.store(true);result.reset();error.clear();pending=options.enabled && bool(dataset);
    }
};
GeneViewSession::GeneViewSession(std::size_t work):data_(std::make_unique<Data>()) {
    if(!work || work>1024ULL*1024*1024)throw std::invalid_argument("Invalid gene-view work budget");data_->work=work;
}
GeneViewSession::~GeneViewSession(){data_->cancelled.store(true);if(data_->future.valid())data_->future.wait();}
void GeneViewSession::bind(ExpressionDataset dataset) {
    workflow_require_owner(data_->owner);if(data_->dataset && data_->dataset->sha256()==dataset.sha256())return;
    for(std::size_t i=0;i<2;++i)if(std::find(dataset.features().begin(),dataset.features().end(),data_->options.genes[i])==dataset.features().end())
        data_->options.genes[i]=dataset.features()[std::min(i,dataset.features().size()-1)];
    data_->dataset=std::move(dataset);data_->changed();
}
void GeneViewSession::clear() {workflow_require_owner(data_->owner);data_->dataset.reset();data_->changed();}
void GeneViewSession::configure(GeneViewOptions options) {
    workflow_require_owner(data_->owner);options.validate();
    if(data_->dataset)for(auto& feature:options.genes)if(!feature.empty())data_->dataset->feature_index(feature);
    if(data_->dataset)for(std::size_t i=0;i<2;++i)if(options.genes[i].empty())options.genes[i]=data_->dataset->features()[std::min(i,data_->dataset->features().size()-1)];
    if(gene_view_preference(options)==gene_view_preference(data_->options))return;
    data_->options=std::move(options);data_->changed();
}
void GeneViewSession::refresh(){workflow_require_owner(data_->owner);data_->changed();}
void GeneViewSession::cancel(){workflow_require_owner(data_->owner);data_->changed();data_->pending=false;}
void GeneViewSession::poll() {
    workflow_require_owner(data_->owner);
    if(data_->future.valid() && data_->future.wait_for(std::chrono::seconds(0))==std::future_status::ready) {
        try {auto result=data_->future.get();if(data_->running==data_->generation)data_->result=std::make_shared<const GeneDisplayPair>(std::move(result));}
        catch(...) {if(data_->running==data_->generation)data_->error=workflow_failure_message(std::current_exception());}
    }
    if(!data_->future.valid() && data_->pending) {
        data_->pending=false;data_->running=data_->generation;data_->cancelled.store(false);
        try {
            data_->future=std::async(std::launch::async,[this,dataset=*data_->dataset,options=data_->options] {
                return gene_display_pair(dataset,options.genes,options.display,options.compare,options.shared_range,data_->work,&data_->cancelled);
            });
        }catch(...) {data_->error=workflow_failure_message(std::current_exception());}
    }
}
void GeneViewSession::wait(){workflow_require_owner(data_->owner);poll();while(data_->future.valid()) {data_->future.wait();poll();}}
bool GeneViewSession::busy() const {workflow_require_owner(data_->owner);return data_->pending || data_->future.valid();}
const GeneViewOptions& GeneViewSession::options() const {workflow_require_owner(data_->owner);return data_->options;}
const std::optional<ExpressionDataset>& GeneViewSession::dataset() const {workflow_require_owner(data_->owner);return data_->dataset;}
std::shared_ptr<const GeneDisplayPair> GeneViewSession::result() const {workflow_require_owner(data_->owner);return data_->result;}
const std::string& GeneViewSession::error() const {workflow_require_owner(data_->owner);return data_->error;}
std::uint64_t GeneViewSession::generation() const {workflow_require_owner(data_->owner);return data_->generation;}
}
