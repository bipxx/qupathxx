#include "qupathxx/classifier.hpp"
#include "sha256.hpp"
#include "utf8_text.hpp"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <limits>
#include <set>
#include <stdexcept>
#ifdef QUPATHXX_HAS_CLASSIFIER_TRAINING
#include <opencv2/core.hpp>
#include <opencv2/ml.hpp>
#endif

namespace qpx {
static_assert(sizeof(float)==4 && std::numeric_limits<float>::is_iec559,
              "Portable classifiers require IEEE-754 float32");
namespace {
constexpr std::size_t max_bytes=64*1024*1024, max_nodes=2000000;
struct Node {std::uint32_t feature=0,left=0,right=0,label=0,threshold=0;};
float value(std::uint32_t bits) {float f;std::memcpy(&f,&bits,4);return f;}
[[maybe_unused]] std::uint32_t bits(float f) {std::uint32_t b;std::memcpy(&b,&f,4);return b;}
void text(const std::string& s) {detail::bounded_utf8(s,256);if(s.empty()) throw std::invalid_argument("empty classifier name/unit");}
void schema_check(const std::vector<ClassifierFeature>& features,const std::vector<std::string>& classes) {
    if(features.empty() || features.size()>128 || classes.size()<2 || classes.size()>256) throw std::invalid_argument("invalid classifier schema size");
    std::set<std::string> names;
    for(const auto& f:features) {text(f.name);text(f.unit);if(!names.insert(f.name).second) throw std::invalid_argument("duplicate classifier feature");}
    names.clear();for(const auto& c:classes) {text(c);if(!names.insert(c).second) throw std::invalid_argument("duplicate classifier class");}
}
std::size_t rows_check(const std::vector<float>& samples,std::size_t columns) {
    if(samples.empty() || samples.size()%columns || samples.size()/columns>100000) throw std::invalid_argument("invalid classifier matrix shape");
    for(float f:samples) if(!std::isfinite(f) || std::abs(f)>std::numeric_limits<float>::max()/4) throw std::invalid_argument("invalid classifier feature value");
    return samples.size()/columns;
}
struct Writer {
    std::vector<std::uint8_t> b;
    void u32(std::uint32_t n) {for(unsigned i=0;i<4;++i) b.push_back(static_cast<std::uint8_t>(n>>(i*8)));}
    void u64(std::uint64_t n) {u32(static_cast<std::uint32_t>(n));u32(static_cast<std::uint32_t>(n>>32));}
    void str(const std::string& s) {u32(static_cast<std::uint32_t>(s.size()));b.insert(b.end(),s.begin(),s.end());}
};
struct Reader {
    const std::vector<std::uint8_t>& b;std::size_t at=0;
    std::uint32_t u32() {if(b.size()-at<4) throw std::invalid_argument("truncated classifier model");std::uint32_t n=0;for(unsigned i=0;i<4;++i) n|=std::uint32_t(b[at++])<<(8*i);return n;}
    std::uint64_t u64() {auto lo=u32();auto hi=u32();return lo|(std::uint64_t(hi)<<32);}
    std::string str() {auto n=u32();if(n>256 || n>b.size()-at) throw std::invalid_argument("invalid classifier text length");std::string s(b.begin()+at,b.begin()+at+n);at+=n;return s;}
};
}
struct ForestModel::Data {
    std::vector<ClassifierFeature> features;
    std::vector<std::string> classes;
    ForestParameters parameters;
    std::string training_sha256,backend;
    std::uint64_t training_rows=0;
    std::vector<std::uint32_t> roots;
    std::vector<Node> nodes;
};
namespace {
void validate(const ForestModel::Data& d) {
    schema_check(d.features,d.classes);d.parameters.validate();text(d.backend);
    if(d.parameters.active_features>d.features.size() || d.parameters.min_samples>d.training_rows || d.training_rows<2*d.classes.size()) throw std::invalid_argument("inconsistent classifier training metadata");
    if(d.training_sha256.size()!=64 || d.training_sha256.find_first_not_of("0123456789abcdef")!=std::string::npos || d.training_rows<4 || d.training_rows>100000) throw std::invalid_argument("invalid classifier training identity");
    if(d.roots.size()!=d.parameters.trees || d.nodes.empty() || d.nodes.size()>max_nodes) throw std::invalid_argument("invalid classifier tree size");
    std::vector<bool> seen(d.nodes.size(),false);
    std::vector<std::pair<std::uint32_t,unsigned>> stack;
    for(auto root:d.roots) {
        stack.emplace_back(root,0);
        while(!stack.empty()) {
            const auto item=stack.back();stack.pop_back();auto id=item.first;
            if(id>=d.nodes.size() || seen[id] || item.second>d.parameters.max_depth) throw std::invalid_argument("invalid classifier tree topology");
            seen[id]=true;const auto& n=d.nodes[id];
            if(n.label>=d.classes.size()) throw std::invalid_argument("invalid classifier leaf label");
            if(!n.left && !n.right) {if(n.feature || n.threshold) throw std::invalid_argument("noncanonical classifier leaf");continue;}
            if(!n.left || !n.right || n.feature>=d.features.size() || !std::isfinite(value(n.threshold))) throw std::invalid_argument("invalid classifier split");
            stack.emplace_back(n.right,item.second+1);stack.emplace_back(n.left,item.second+1);
        }
    }
    if(std::find(seen.begin(),seen.end(),false)!=seen.end()) throw std::invalid_argument("unreachable classifier nodes");
}
std::vector<ClassifierPrediction> predict(const ForestModel::Data& d,const std::vector<float>& samples) {
    const auto rows=rows_check(samples,d.features.size());
    if(rows*d.classes.size()>8000000) throw std::length_error("classifier prediction output exceeds vote budget");
    std::vector<ClassifierPrediction> result;result.reserve(rows);
    for(std::size_t r=0;r<rows;++r) {
        ClassifierPrediction p;p.votes.resize(d.classes.size());const auto* sample=samples.data()+r*d.features.size();
        for(auto root:d.roots) {auto id=root;while(d.nodes[id].left) {const auto& n=d.nodes[id];id=sample[n.feature]<=value(n.threshold)?n.left:n.right;}++p.votes[d.nodes[id].label];}
        p.class_index=static_cast<std::uint32_t>(std::max_element(p.votes.begin(),p.votes.end())-p.votes.begin());
        p.vote_fraction=double(p.votes[p.class_index])/d.roots.size();result.push_back(std::move(p));
    }
    return result;
}
}
void ForestParameters::validate() const {
    if(!trees || trees>256 || !max_depth || max_depth>16 || min_samples<2 || min_samples>100000 || active_features>128 || !seed || work_bytes<1024*1024 || work_bytes>1024ULL*1024*1024) throw std::invalid_argument("invalid random forest parameters");
}
ForestModel::ForestModel(std::shared_ptr<const Data> d):data_(std::move(d)) {if(!data_) throw std::invalid_argument("null classifier model");validate(*data_);}
const std::vector<ClassifierFeature>& ForestModel::features() const {return data_->features;}
const std::vector<std::string>& ForestModel::classes() const {return data_->classes;}
const ForestParameters& ForestModel::parameters() const {return data_->parameters;}
const std::string& ForestModel::training_sha256() const {return data_->training_sha256;}
const std::string& ForestModel::backend() const {return data_->backend;}
std::uint32_t ForestModel::tree_count() const {return static_cast<std::uint32_t>(data_->roots.size());}
std::size_t ForestModel::node_count() const {return data_->nodes.size();}
std::uint64_t ForestModel::training_rows() const {return data_->training_rows;}
std::vector<ClassifierPrediction> ForestModel::predict(const std::vector<float>& samples,const std::vector<ClassifierFeature>& schema) const {
    if(schema.size()!=features().size()) throw std::invalid_argument("classifier feature schema mismatch");
    for(std::size_t i=0;i<schema.size();++i) if(schema[i].name!=features()[i].name || schema[i].unit!=features()[i].unit) throw std::invalid_argument("classifier feature schema mismatch");
    return qpx::predict(*data_,samples);
}
std::vector<std::uint8_t> ForestModel::bytes() const {
    const auto& d=*data_;Writer w;w.u32(0x46585051);w.u32(1);w.u32(d.parameters.trees);w.u32(d.parameters.max_depth);w.u32(d.parameters.min_samples);w.u32(d.parameters.active_features);w.u64(d.parameters.seed);w.u64(d.parameters.work_bytes);w.u64(d.training_rows);
    w.str(d.training_sha256);w.str(d.backend);w.u32(static_cast<std::uint32_t>(d.features.size()));for(const auto& f:d.features) {w.str(f.name);w.str(f.unit);}
    w.u32(static_cast<std::uint32_t>(d.classes.size()));for(const auto& c:d.classes) w.str(c);
    w.u32(static_cast<std::uint32_t>(d.roots.size()));for(auto r:d.roots) w.u32(r);
    w.u32(static_cast<std::uint32_t>(d.nodes.size()));for(const auto& n:d.nodes) {w.u32(n.feature);w.u32(n.left);w.u32(n.right);w.u32(n.label);w.u32(n.threshold);}
    if(w.b.size()>max_bytes) throw std::length_error("classifier model exceeds byte limit");
    return w.b;
}
std::string ForestModel::sha256() const {const auto b=bytes();Sha256 s;s.update(b.data(),b.size());return s.finish();}
ForestModel ForestModel::from_bytes(const std::vector<std::uint8_t>& bytes) {
    if(bytes.size()>max_bytes) throw std::length_error("classifier model exceeds byte limit");
    Reader r{bytes};
    if(r.u32()!=0x46585051 || r.u32()!=1) throw std::invalid_argument("unsupported classifier model format");
    auto d=std::make_shared<Data>();
    auto& p=d->parameters;p.trees=r.u32();p.max_depth=r.u32();p.min_samples=r.u32();p.active_features=r.u32();p.seed=r.u64();p.work_bytes=r.u64();p.validate();
    d->training_rows=r.u64();d->training_sha256=r.str();d->backend=r.str();auto count=r.u32();if(!count || count>128) throw std::invalid_argument("invalid classifier feature count");
    for(std::uint32_t i=0;i<count;++i) {auto name=r.str();auto unit=r.str();d->features.push_back({name,unit});}
    count=r.u32();if(count<2 || count>256) throw std::invalid_argument("invalid classifier class count");for(std::uint32_t i=0;i<count;++i) d->classes.push_back(r.str());
    count=r.u32();if(count!=p.trees) throw std::invalid_argument("invalid classifier root count");for(std::uint32_t i=0;i<count;++i) d->roots.push_back(r.u32());
    count=r.u32();if(!count || count>max_nodes || std::uint64_t(count)*20!=bytes.size()-r.at) throw std::invalid_argument("invalid classifier node payload");
    d->nodes.reserve(count);for(std::uint32_t i=0;i<count;++i) {Node n;n.feature=r.u32();n.left=r.u32();n.right=r.u32();n.label=r.u32();n.threshold=r.u32();d->nodes.push_back(n);}
    return ForestModel(d);
}
bool has_classifier_training() {
#ifdef QUPATHXX_HAS_CLASSIFIER_TRAINING
    return true;
#else
    return false;
#endif
}
ForestModel train_forest(const std::vector<float>& samples,const std::vector<std::uint32_t>& labels,const std::vector<ClassifierFeature>& schema,const std::vector<std::string>& classes,ForestParameters p) {
    p.validate();schema_check(schema,classes);const auto rows=rows_check(samples,schema.size());
    if(labels.size()!=rows || p.active_features>schema.size() || p.min_samples>rows) throw std::invalid_argument("invalid classifier training dimensions");
    std::vector<std::size_t> counts(classes.size());for(auto c:labels) {if(c>=counts.size()) throw std::invalid_argument("invalid classifier training label");++counts[c];}
    for(auto c:counts) if(c<2) throw std::invalid_argument("each classifier class requires two training rows");
    const auto nodes_per_tree=std::min<std::uint64_t>((1ULL<<(p.max_depth+1))-1,2*rows-1);
    const auto estimated_nodes=nodes_per_tree*p.trees;
    const auto work=estimated_nodes*128+rows*schema.size()*32+rows*128+1024*1024;
    if(estimated_nodes>max_nodes || work>p.work_bytes) throw std::length_error("classifier training exceeds work budget");
#ifndef QUPATHXX_HAS_CLASSIFIER_TRAINING
    throw std::runtime_error("classifier training requires the optional OpenCV backend");
#else
    // RTrees uses the calling thread's RNG. Restore it on all exit paths;
    // never change OpenCV's process-wide thread count or RNG in another thread.
    struct RngGuard {cv::RNG saved=cv::theRNG();~RngGuard() {cv::theRNG()=saved;}} guard;
    cv::theRNG()=cv::RNG(p.seed);
    cv::Mat x(static_cast<int>(rows),static_cast<int>(schema.size()),CV_32F,const_cast<float*>(samples.data()));
    cv::Mat y(static_cast<int>(rows),1,CV_32S);for(std::size_t i=0;i<rows;++i) y.at<int>(static_cast<int>(i))=static_cast<int>(labels[i]);
    auto forest=cv::ml::RTrees::create();forest->setMaxDepth(static_cast<int>(p.max_depth));forest->setMinSampleCount(static_cast<int>(p.min_samples));forest->setActiveVarCount(static_cast<int>(p.active_features));forest->setUseSurrogates(false);forest->setCVFolds(0);forest->setCalculateVarImportance(false);forest->setTermCriteria({cv::TermCriteria::MAX_ITER,static_cast<int>(p.trees),0});
    if(!forest->train(x,cv::ml::ROW_SAMPLE,y)) throw std::runtime_error("OpenCV classifier training failed");
    auto d=std::make_shared<ForestModel::Data>();d->features=schema;d->classes=classes;d->parameters=p;d->training_rows=rows;d->backend="OpenCV/" CV_VERSION "/RTrees/ordered-f32/qpx-1";
    Sha256 digest;const std::string domain="qpx.forest.training/1";digest.update(domain.data(),domain.size());digest.integer(rows);digest.integer(schema.size());digest.integer(classes.size());for(const auto& f:schema) {digest.integer(f.name.size());digest.update(f.name.data(),f.name.size());digest.integer(f.unit.size());digest.update(f.unit.data(),f.unit.size());}for(const auto& c:classes) {digest.integer(c.size());digest.update(c.data(),c.size());}for(float f:samples) digest.integer(bits(f));for(auto c:labels) digest.integer(c);d->training_sha256=digest.finish();
    const auto& nodes=forest->getNodes();const auto& splits=forest->getSplits();
    // Copy only reachable nodes, preserving exact float thresholds. No parser
    // or OpenCV state participates in inference or portable model loading.
    auto copy=[&](auto&& self,int index,unsigned depth)->std::uint32_t {
        if(index<0 || static_cast<std::size_t>(index)>=nodes.size() || depth>p.max_depth || d->nodes.size()>=max_nodes) throw std::runtime_error("invalid OpenCV classifier tree");
        const auto& source=nodes[index];Node n;if(source.classIdx<0 || static_cast<std::size_t>(source.classIdx)>=classes.size()) throw std::runtime_error("invalid OpenCV classifier label");n.label=static_cast<std::uint32_t>(source.classIdx);
        auto id=static_cast<std::uint32_t>(d->nodes.size());d->nodes.push_back(n);
        if(source.split>=0) {if(static_cast<std::size_t>(source.split)>=splits.size()) throw std::runtime_error("invalid OpenCV classifier split");const auto& s=splits[source.split];if(s.inversed || s.next>=0 || s.varIdx<0) throw std::runtime_error("unsupported OpenCV classifier split");n.feature=static_cast<std::uint32_t>(s.varIdx);n.threshold=bits(s.c);n.left=self(self,source.left,depth+1);n.right=self(self,source.right,depth+1);d->nodes[id]=n;}return id;
    };
    for(auto root:forest->getRoots()) d->roots.push_back(copy(copy,root,0));
    ForestModel model(d);
    // Verify every training row against the backend's independent traversal.
    // Batch votes bound temporary allocations to <=1025*256 integers.
    for(int start=0;start<x.rows;start+=1024) {auto batch=x.rowRange(start,std::min(start+1024,x.rows));cv::Mat votes;forest->getVotes(batch,votes,0);std::vector<float> flat(batch.ptr<float>(),batch.ptr<float>()+batch.total());auto result=qpx::predict(*d,flat);if(votes.rows!=batch.rows+1 || votes.cols!=static_cast<int>(classes.size()) || votes.type()!=CV_32S) throw std::runtime_error("unexpected OpenCV classifier votes");for(int c=0;c<votes.cols;++c) {if(votes.at<int>(0,c)!=c) throw std::runtime_error("unexpected OpenCV class mapping");for(int r=0;r<batch.rows;++r) if(result[r].votes[c]!=static_cast<std::uint32_t>(votes.at<int>(r+1,c))) throw std::runtime_error("portable classifier votes differ from OpenCV");}}
    return model;
#endif
}
}
