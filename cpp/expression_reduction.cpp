#include "expression_reduction_internal.hpp"
#include <random>
#include <numeric>
#include <set>
#ifdef QUPATHXX_HAS_LINEAR_ALGEBRA
#include <opencv2/core.hpp>
#endif

namespace qpx {
using namespace detail::reduction;
void ExpressionReductionOptions::validate(const ExpressionMatrixMetadata& m)const{
    work_limit(work_bytes);digest(m.source_sha256);text(m.value_domain);
    if(!m.features || m.features>1000000 || m.observations<2 || m.observations>1000000 || (m.domain!=ExpressionValueDomain::Counts && m.domain!=ExpressionValueDomain::Continuous))throw std::invalid_argument("Reduction requires2..1000000 observations and1..1000000 features in a declared value domain");
    const auto p=features.empty()?m.features:features.size();
    if(features.size()>m.features || (!features.empty() && (features.back()>=m.features || !std::is_sorted(features.begin(),features.end()) || std::adjacent_find(features.begin(),features.end())!=features.end())))throw std::invalid_argument("Reduction features must be ascending unique source indices");
    if(!components || components>32 || components>p || components>m.observations-(center?1:0) || oversampling>32 || components+oversampling>64 || power_iterations>4)throw std::invalid_argument("Reduction component/sketch/power dimensions exceed contract");
    if(!clusters || clusters>32 || clusters>m.observations || !max_iterations || max_iterations>1000 || !std::isfinite(cluster_tolerance) || cluster_tolerance<0 || cluster_tolerance>1 || !max_operations || max_operations>1000000000000ull || !max_distance_tests || max_distance_tests>10000000000ull || normalization_recipe.size()>768u*1024)throw std::invalid_argument("Reduction clustering/operation/recipe limits invalid");
}
bool has_expression_reduction(){
#ifdef QUPATHXX_HAS_LINEAR_ALGEBRA
    return true;
#else
    return false;
#endif
}
namespace {
struct Operations {
    std::uint64_t used=0,maximum;
    void add(std::uint64_t n,std::uint64_t width=1){if(n>(maximum-used)/width)throw WorkflowCaptureLimit("Reduction arithmetic-operation budget exceeded");used+=n*width;}
};
void finite(double x){if(!std::isfinite(x))throw std::overflow_error("Reduction arithmetic is not finite");}
struct Random {
    std::mt19937_64 engine;
    explicit Random(std::uint64_t seed):engine(seed){}
    double uniform(){return double(engine()>>11)*0x1.0p-53;}
    double normal(){const auto u=(double(engine()>>11)+.5)*0x1.0p-53;return std::sqrt(-2*std::log(u))*std::cos(6.283185307179586476925286766559*uniform());}
    std::size_t index(std::size_t n){const auto bound=std::uint64_t(n),threshold=(std::uint64_t(0)-bound)%bound;std::uint64_t x;do{x=engine();}while(x<threshold);return std::size_t(x%bound);}
};
struct SparsePass {
    std::shared_ptr<const ExpressionMatrixSource> source;
    const std::vector<std::uint64_t>& features;
    std::size_t work,batch,batches=0;
    const std::atomic<bool>* cancelled;
    Operations& operations;
    double value(const ExpressionSparseBlock& b,std::size_t i)const{
        if(b.domain==ExpressionValueDomain::Counts){if(b.counts[i]>9007199254740992ull)throw std::invalid_argument("Direct reduction counts exceed2^53; select explicit appropriate derived values");return double(b.counts[i]);}
        const auto x=b.continuous[i];if(!std::isfinite(x))throw std::invalid_argument("Reduction source contains nonfinite values");return x;
    }
    template<class F>void each(F f){
        workflow_checkpoint(cancelled);source->require_current(cancelled);
        for(std::size_t first=0;first<features.size();first+=batch){
            workflow_checkpoint(cancelled);ExpressionMatrixSelection selection;const auto n=std::min(batch,features.size()-first);selection.features.assign(features.begin()+first,features.begin()+first+n);
            const auto b=source->read(selection,work,cancelled);++batches;const auto entries=b.observation_indices.size();
            if(b.source_sha256!=source->metadata().source_sha256 || b.domain!=source->metadata().domain || b.features!=selection.features || b.offsets.size()!=n+1 || b.offsets.front()!=0 || b.offsets.back()!=entries || !std::is_sorted(b.offsets.begin(),b.offsets.end()) || (b.domain==ExpressionValueDomain::Counts?(b.counts.size()!=entries || !b.continuous.empty()):(b.continuous.size()!=entries || !b.counts.empty())))throw std::invalid_argument("Reduction sparse block shape/source/domain differs");
            for(std::size_t k=0;k<n;++k){std::optional<std::uint64_t> previous;for(auto i=b.offsets[k];i<b.offsets[k+1];++i){if((i&4095)==0)workflow_checkpoint(cancelled);const auto o=b.observation_indices[i];if(o>=source->metadata().observations || (previous && o<=*previous))throw std::invalid_argument("Reduction sparse observation axis invalid");previous=o;}f(first+k,b,b.offsets[k],b.offsets[k+1]);}
        }
        source->require_current(cancelled);workflow_checkpoint(cancelled);
    }
};
struct SmallMatrix {std::size_t rows=0,cols=0;std::vector<double> values;};
SmallMatrix product(SparsePass& pass,const std::vector<double>& means,bool center,const SmallMatrix& right,bool transpose){
    const auto n=std::size_t(pass.source->metadata().observations),p=means.size(),r=right.cols;
    if(!r || right.rows!=(transpose?n:p) || right.values.size()!=right.rows*r)throw std::logic_error("Reduction product dimensions differ");
    SmallMatrix out;out.rows=transpose?p:n;out.cols=r;out.values.resize(out.rows*r);std::vector<double> offsets(r);
    pass.operations.add(right.rows,r);
    if(center){if(transpose){for(std::size_t i=0;i<n;++i){if((i&4095)==0)workflow_checkpoint(pass.cancelled);for(std::size_t j=0;j<r;++j)offsets[j]+=right.values[i*r+j];}}
        else{for(std::size_t f=0;f<p;++f)for(std::size_t j=0;j<r;++j)offsets[j]-=means[f]*right.values[f*r+j];for(std::size_t i=0;i<n;++i)std::copy(offsets.begin(),offsets.end(),out.values.begin()+i*r);}}
    pass.each([&](std::size_t f,const ExpressionSparseBlock& block,std::uint64_t begin,std::uint64_t end){
        if(transpose && center)for(std::size_t j=0;j<r;++j)out.values[f*r+j]=-means[f]*offsets[j];
        pass.operations.add(end-begin,r);
        for(auto i=begin;i<end;++i){if((i&4095)==0)workflow_checkpoint(pass.cancelled);const auto o=std::size_t(block.observation_indices[i]);const auto x=pass.value(block,i);auto* dst=out.values.data()+(transpose?f:o)*r;const auto* src=right.values.data()+(transpose?o:f)*r;for(std::size_t j=0;j<r;++j)dst[j]+=x*src[j];}
    });
    for(auto x:out.values)finite(x);return out;
}
SmallMatrix orthogonalize(SmallMatrix input,Operations& ops,const std::atomic<bool>* cancelled){
    const auto n=input.rows,l=input.cols;ops.add(n,4*l*l+2*l);std::size_t rank=0;double largest=0;
    for(std::size_t j=0;j<l;++j){double norm=0;for(std::size_t i=0;i<n;++i)norm=std::hypot(norm,input.values[i*l+j]);largest=std::max(largest,norm);}
    if(!(largest>0) || !std::isfinite(largest))throw std::invalid_argument("Reduction source has no usable numerical range");
    for(std::size_t j=0;j<l;++j){
        workflow_checkpoint(cancelled);
        for(int repeat=0;repeat<2;++repeat)for(std::size_t q=0;q<rank;++q){double dot=0;for(std::size_t i=0;i<n;++i){if((i&4095)==0)workflow_checkpoint(cancelled);dot+=input.values[i*l+q]*input.values[i*l+j];}for(std::size_t i=0;i<n;++i)input.values[i*l+j]-=dot*input.values[i*l+q];}
        double norm=0;for(std::size_t i=0;i<n;++i)norm=std::hypot(norm,input.values[i*l+j]);
        if(norm<=largest*1e-12)continue;finite(norm);for(std::size_t i=0;i<n;++i)input.values[i*l+rank]=input.values[i*l+j]/norm;++rank;
    }
    if(!rank)throw std::invalid_argument("Reduction source is numerically rank deficient");SmallMatrix out;out.rows=n;out.cols=rank;out.values.resize(n*rank);for(std::size_t i=0;i<n;++i)std::copy_n(input.values.begin()+i*l,rank,out.values.begin()+i*rank);return out;
}
struct Clusters {std::vector<std::uint32_t> labels;std::vector<double> centres;double inertia=0;std::uint32_t iterations=0;bool converged=false;std::uint64_t tests=0;};
Clusters cluster(const std::vector<double>& x,std::size_t n,std::size_t dims,const ExpressionReductionOptions& o,Operations& ops,const std::atomic<bool>* cancelled){
    Clusters result;const auto k=std::size_t(o.clusters);result.labels.assign(n,UINT32_MAX);result.centres.resize(k*dims);Random random(o.seed^0x9e3779b97f4a7c15ull);
    const auto distance=[&](std::size_t row,const double* centre){if(result.tests==o.max_distance_tests)throw WorkflowCaptureLimit("Reduction clustering distance budget exceeded");++result.tests;ops.add(dims,3);double sum=0;for(std::size_t d=0;d<dims;++d){const auto delta=x[row*dims+d]-centre[d];sum+=delta*delta;}finite(sum);return sum;};
    const auto first=random.index(n);std::copy_n(x.begin()+first*dims,dims,result.centres.begin());std::vector<double> nearest(n,std::numeric_limits<double>::infinity());
    for(std::size_t c=1;c<k;++c){double total=0;for(std::size_t i=0;i<n;++i){if((i&4095)==0)workflow_checkpoint(cancelled);nearest[i]=std::min(nearest[i],distance(i,result.centres.data()+(c-1)*dims));total+=nearest[i];}finite(total);if(!(total>0))throw std::invalid_argument("Requested clusters exceed distinct fitted observations");const auto threshold=random.uniform()*total;double cumulative=0;std::size_t selected=n;for(std::size_t i=0;i<n;++i)if(nearest[i]>0){cumulative+=nearest[i];selected=i;if(cumulative>threshold)break;}if(selected==n)throw std::logic_error("Clustering seed selection failed");std::copy_n(x.begin()+selected*dims,dims,result.centres.begin()+c*dims);}
    std::vector<double> sums(k*dims),assigned_distance(n);std::vector<std::size_t> counts(k);
    for(std::uint32_t iteration=0;iteration<o.max_iterations;++iteration){
        workflow_checkpoint(cancelled);std::fill(sums.begin(),sums.end(),0);std::fill(counts.begin(),counts.end(),0);bool changed=false;
        for(std::size_t i=0;i<n;++i){if((i&1023)==0)workflow_checkpoint(cancelled);std::uint32_t best=0;double closest=distance(i,result.centres.data());for(std::uint32_t c=1;c<k;++c){const auto d=distance(i,result.centres.data()+c*dims);if(d<closest){closest=d;best=c;}}changed=changed || result.labels[i]!=best;result.labels[i]=best;assigned_distance[i]=closest;++counts[best];for(std::size_t d=0;d<dims;++d)sums[best*dims+d]+=x[i*dims+d];}
        for(std::size_t c=0;c<k;++c)if(!counts[c]){std::size_t farthest=n;double maximum=-1;for(std::size_t i=0;i<n;++i)if(counts[result.labels[i]]>1 && assigned_distance[i]>maximum){maximum=assigned_distance[i];farthest=i;}if(farthest==n || !(maximum>0))throw std::invalid_argument("Cannot retain the requested nonempty cluster count");const auto donor=result.labels[farthest];--counts[donor];++counts[c];for(std::size_t d=0;d<dims;++d){sums[donor*dims+d]-=x[farthest*dims+d];sums[c*dims+d]+=x[farthest*dims+d];}result.labels[farthest]=std::uint32_t(c);assigned_distance[farthest]=0;changed=true;}
        double movement=0,scale=0;for(std::size_t c=0;c<k;++c){double shift=0,norm=0;for(std::size_t d=0;d<dims;++d){const auto value=sums[c*dims+d]/double(counts[c]);finite(value);const auto delta=value-result.centres[c*dims+d];shift+=delta*delta;norm+=value*value;result.centres[c*dims+d]=value;}finite(shift);finite(norm);movement=std::max(movement,shift);scale=std::max(scale,norm);}result.iterations=iteration+1;
        if(!changed || movement<=o.cluster_tolerance*o.cluster_tolerance*(1+scale)){result.converged=true;break;}
    }
    // Canonical labels follow lexicographic fitted-centre order, independent of
    // the order chosen by seeded initialization. Equal distances choose lower ID.
    std::vector<std::size_t> order(k),inverse(k);std::iota(order.begin(),order.end(),0);std::stable_sort(order.begin(),order.end(),[&](auto a,auto b){return std::lexicographical_compare(result.centres.begin()+a*dims,result.centres.begin()+(a+1)*dims,result.centres.begin()+b*dims,result.centres.begin()+(b+1)*dims);});const auto old=result.centres;for(std::size_t c=0;c<k;++c){inverse[order[c]]=c;std::copy_n(old.begin()+order[c]*dims,dims,result.centres.begin()+c*dims);}for(std::size_t i=0;i<n;++i){if((i&4095)==0)workflow_checkpoint(cancelled);result.labels[i]=std::uint32_t(inverse[result.labels[i]]);result.inertia+=distance(i,result.centres.data()+result.labels[i]*dims);}finite(result.inertia);return result;
}
}

std::shared_ptr<const ExpressionReductionResult> reduce_expression(std::shared_ptr<const ExpressionMatrixSource> parent,const ExpressionReductionOptions& options,const std::atomic<bool>* cancelled){
    workflow_checkpoint(cancelled);if(!parent)throw std::invalid_argument("Reduction requires a native matrix source");options.validate(parent->metadata());digest(parent->file_sha256());
#ifndef QUPATHXX_HAS_LINEAR_ALGEBRA
    throw std::runtime_error("Expression reduction requires the optional native OpenCV core backend");
#else
    const auto& m=parent->metadata();const auto n=std::size_t(m.observations),p=options.features.empty()?std::size_t(m.features):options.features.size(),k=std::size_t(options.components),l=std::min<std::size_t>({k+options.oversampling,p,n-(options.center?1:0)}),work=options.work_bytes;
    std::size_t fixed=16u*1024*1024;workflow_capture_charge(fixed,parent->payload_bytes(),work);charge(fixed,n+p,1024,work);charge(fixed,n+p,8*l*10+16*k+32,work);charge(fixed,options.normalization_recipe.capacity(),6,work);charge(fixed,options.features.capacity(),24,work);if(work-fixed<8u*1024*1024)throw WorkflowCaptureLimit("Reduction sketches/results leave no sparse read workspace");
    auto values=parent;if(!options.normalization_recipe.empty()){auto normalized=replay_expression_normalization(parent,options.normalization_recipe,work-fixed,cancelled);values=normalized.source();workflow_capture_charge(fixed,values->payload_bytes(),work);}if(work-fixed<8u*1024*1024)throw WorkflowCaptureLimit("Derived reduction source leaves no read workspace");parent->require_current(cancelled);
    auto d=std::make_shared<ExpressionReductionResult::Data>();d->options=options;d->source=m.source_sha256;d->file=parent->file_sha256();d->value_source=values->metadata().source_sha256;d->domain=values->metadata().value_domain;d->source_features=m.features;d->backend="opencv-core-" CV_VERSION ";native-mgs2;mt19937_64-box-muller/1";d->features=options.features;if(d->features.empty()){d->features.resize(p);std::iota(d->features.begin(),d->features.end(),0);}
    const auto available=work-fixed;const auto batch=std::max<std::size_t>(1,std::min<std::size_t>(128,(available/8)/(n*32)));const auto read_work=available-batch*2048-8192;d->peak=fixed+read_work+batch*2048+8192;Operations operations{0,options.max_operations};SparsePass pass{values,d->features,read_work,batch,0,cancelled,operations};
    d->feature_ids.reserve(p);for(std::size_t first=0;first<p;){workflow_checkpoint(cancelled);std::size_t count=1;while(count<128 && first+count<p && d->features[first+count]==d->features[first]+count)++count;const auto page=parent->feature_page(d->features[first],count,read_work,cancelled);if(page.size()!=count)throw std::invalid_argument("Reduction feature catalogue differs");for(std::size_t i=0;i<count;++i){if(page[i].index!=d->features[first+i])throw std::invalid_argument("Reduction feature catalogue index differs");text(page[i].id);d->feature_ids.push_back(page[i].id);}first+=count;}
    d->observations.reserve(n);for(std::size_t first=0;first<n;first+=512){const auto count=std::min<std::size_t>(512,n-first);auto page=parent->observation_page(first,count,read_work,cancelled);if(page.size()!=count)throw std::invalid_argument("Reduction observation catalogue differs");for(auto& id:page){text(id);d->observations.push_back(std::move(id));}}
    d->means.resize(p);
    pass.each([&](std::size_t f,const ExpressionSparseBlock& b,std::uint64_t begin,std::uint64_t end){
        operations.add(end-begin,8);double mean=0,m2=0;std::size_t count=0;for(auto i=begin;i<end;++i){if((i&4095)==0)workflow_checkpoint(cancelled);const auto x=pass.value(b,i),delta=x-mean;mean+=delta/double(++count);m2+=delta*(x-mean);}if(count){const auto zeros=n-count;m2+=(mean*mean)*(double(count)*double(zeros)/double(n));mean*=double(count)/double(n);}finite(mean);finite(m2);d->means[f]=mean;d->total_energy+=options.center?m2:m2+mean*mean*double(n);finite(d->total_energy);
    });if(!(d->total_energy>0))throw std::invalid_argument("Reduction source has no positive analysis energy");
    Random random(options.seed);SmallMatrix omega;omega.rows=p;omega.cols=l;omega.values.resize(p*l);for(std::size_t i=0;i<omega.values.size();++i){if((i&4095)==0)workflow_checkpoint(cancelled);omega.values[i]=random.normal();}
    auto q=orthogonalize(product(pass,d->means,options.center,omega,false),operations,cancelled);omega.values.clear();omega.values.shrink_to_fit();
    for(std::uint32_t step=0;step<options.power_iterations;++step){auto z=orthogonalize(product(pass,d->means,options.center,q,true),operations,cancelled);q=orthogonalize(product(pass,d->means,options.center,z,false),operations,cancelled);}
    if(q.cols<k)throw std::invalid_argument("Requested components exceed numerical rank");auto sketch=product(pass,d->means,options.center,q,true);operations.add(p,8*sketch.cols*sketch.cols);workflow_checkpoint(cancelled);
    double scale=0;for(auto x:sketch.values)scale=std::max(scale,std::abs(x));if(!(scale>0))throw std::invalid_argument("Reduction sketch is rank deficient");cv::Mat input(int(p),int(sketch.cols),CV_64F);for(std::size_t i=0;i<sketch.values.size();++i)input.ptr<double>()[i]=sketch.values[i]/scale;cv::SVD svd(input,0);workflow_checkpoint(cancelled);if(svd.w.total()<k || svd.w.at<double>(int(k-1))<=svd.w.at<double>(0)*1e-12)throw std::invalid_argument("Requested components exceed small-sketch numerical rank");
    SmallMatrix loading;loading.rows=p;loading.cols=k;loading.values.resize(p*k);d->singular.resize(k);
    for(std::size_t c=0;c<k;++c){std::size_t pivot=0;for(std::size_t f=1;f<p;++f)if(std::abs(svd.u.at<double>(int(f),int(c)))>std::abs(svd.u.at<double>(int(pivot),int(c))))pivot=f;const auto sign=svd.u.at<double>(int(pivot),int(c))<0?-1.:1.;for(std::size_t f=0;f<p;++f)loading.values[f*k+c]=sign*svd.u.at<double>(int(f),int(c));d->singular[c]=scale*svd.w.at<double>(int(c));finite(d->singular[c]);}
    d->embedding=product(pass,d->means,options.center,loading,false).values;d->loadings=std::move(loading.values);d->energy.resize(k);for(std::size_t i=0;i<n;++i){if((i&4095)==0)workflow_checkpoint(cancelled);for(std::size_t c=0;c<k;++c)d->energy[c]+=d->embedding[i*k+c]*d->embedding[i*k+c];}for(auto v:d->energy)finite(v);
    auto fitted=cluster(d->embedding,n,k,options,operations,cancelled);d->labels=std::move(fitted.labels);d->centres=std::move(fitted.centres);d->inertia=fitted.inertia;d->iterations=fitted.iterations;d->converged=fitted.converged;d->distances=fitted.tests;d->operations=operations.used;d->batches=pass.batches;
    parent->require_current(cancelled);auto result=std::shared_ptr<ExpressionReductionResult>(new ExpressionReductionResult(d));ExpressionReductionCodec::identify(*result,work,cancelled);workflow_checkpoint(cancelled);return result;
#endif
}
void replay_expression_reduction(std::shared_ptr<const ExpressionMatrixSource> parent,const ExpressionReductionResult& saved,std::size_t work,const std::atomic<bool>* cancelled,std::uint64_t max_operations,std::uint64_t max_distances){
    if(!parent)throw std::invalid_argument("Reduction replay requires a parent source");detail::reduction::require_parent(saved,*parent,work,cancelled);auto options=saved.options();options.work_bytes=work;options.max_operations=max_operations;options.max_distance_tests=max_distances;const auto fitted=reduce_expression(std::move(parent),options,cancelled);if(fitted->backend()!=saved.backend())throw WorkflowInputChanged("Reduction replay backend differs; stored arrays retain their own identity");if(fitted->id()!=saved.id())throw WorkflowInputChanged("Reduction replay fitted identity differs; numerical closeness does not establish model identity");
}
}
