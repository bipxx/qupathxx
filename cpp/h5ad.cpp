#include "qupathxx/h5ad.hpp"
#include "h5ad_identity.hpp"
#include "qupathxx/file_fingerprint.hpp"
#include "qupathxx/workflow.hpp"
#include "reader_file_guard.hpp"
#include "atomic_export.hpp"
#include "spatial_count_text.hpp"
#include "sha256.hpp"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <cstdlib>
#include <numeric>
#include <string_view>
#include <mutex>
#include <chrono>
#ifdef QUPATHXX_HAS_H5AD
#include <hdf5.h>
#include <H5VLconnector.h>
#endif
namespace qpx {
void H5adOptions::validate() const {
    frame.validate();
    const auto key=[](const std::string& s){return !s.empty() && s.size()<=256 && s.find_first_of("/\\\0")==std::string::npos && s!="." && s!=".." && s.find(char(0))==std::string::npos;};
    if(!(matrix=="X" || (matrix.rfind("layers/",0)==0 && key(matrix.substr(7)))) || spatial.rfind("obsm/",0)!=0 || !key(spatial.substr(5)) ||
       (domain!=ExpressionValueDomain::Counts && domain!=ExpressionValueDomain::Continuous) ||
       value_domain.empty() || value_domain.size()>128 || (domain==ExpressionValueDomain::Counts && value_domain!="raw-counts") ||
       (domain==ExpressionValueDomain::Continuous && value_domain=="raw-counts") ||
       !file_bytes || file_bytes>16ull*1024*1024*1024 || work_bytes<16u*1024*1024 || work_bytes>1024ull*1024*1024 ||
       !chunk_bytes || chunk_bytes>work_bytes/4 || !max_features || max_features>1000000 || !max_observations || max_observations>1000000 || max_entries>1000000000)
        throw std::invalid_argument("Invalid H5AD domain, keys, coordinate frame or limits");
    detail::bounded_utf8(value_domain,128);
}
bool has_h5ad(){
#ifdef QUPATHXX_HAS_H5AD
return true;
#else
return false;
#endif
}
#ifdef QUPATHXX_HAS_H5AD
namespace {
std::recursive_timed_mutex library_mutex;
std::unique_lock<std::recursive_timed_mutex> library_lock(const std::atomic<bool>* c){
    std::unique_lock<std::recursive_timed_mutex> lock(library_mutex,std::defer_lock);
    while(!lock.try_lock_for(std::chrono::milliseconds(10)))workflow_checkpoint(c);
    workflow_checkpoint(c);return lock;
}
void require(bool okay,const std::string& message){if(!okay)throw std::invalid_argument("H5AD: "+message);}
void status(herr_t rc,const std::string& operation){
    if(rc>=0)return;
    struct Trace {char bytes[2048]{};std::size_t used=0;} trace;
    H5Ewalk2(H5E_DEFAULT,H5E_WALK_DOWNWARD,[](unsigned,const H5E_error2_t* e,void* context)->herr_t{
        auto& t=*static_cast<Trace*>(context);if(t.used && t.used+2<sizeof(t.bytes)){t.bytes[t.used++]=';';t.bytes[t.used++]=' ';}
        for(std::size_t i=0;e->desc && e->desc[i] && i<384 && t.used+1<sizeof(t.bytes);++i)t.bytes[t.used++]=e->desc[i];return 0;
    },&trace);
    require(false,operation+": "+trace.bytes);
}
class Handle {
    hid_t id_=-1;
public:
    explicit Handle(hid_t id):id_(id){require(id>=0,"cannot open requested element/type/space");}
    ~Handle(){switch(H5Iget_type(id_)){case H5I_FILE:H5Fclose(id_);break;case H5I_GROUP:H5Gclose(id_);break;case H5I_DATASET:H5Dclose(id_);break;case H5I_DATATYPE:H5Tclose(id_);break;case H5I_DATASPACE:H5Sclose(id_);break;case H5I_ATTR:H5Aclose(id_);break;case H5I_VOL:H5VLclose(id_);break;case H5I_GENPROP_LST:H5Pclose(id_);break;default:break;}}
    Handle(const Handle&)=delete;operator hid_t()const{return id_;}
};
struct Errors {
    H5E_auto2_t old=nullptr;void* data=nullptr;
    Errors(){H5Eget_auto2(H5E_DEFAULT,&old,&data);H5Eset_auto2(H5E_DEFAULT,nullptr,nullptr);}
    ~Errors(){H5Eset_auto2(H5E_DEFAULT,old,data);}
};
Handle open_file(const std::string& path){
    status(H5PLset_loading_state(0),"disable external plugins");Handle fapl(H5Pcreate(H5P_FILE_ACCESS));
    status(H5Pset_vol(fapl,H5VL_NATIVE,H5P_DEFAULT),"select native local connector");status(H5Pset_fapl_sec2(fapl),"select local file driver");
    status(H5Pset_cache(fapl,0,521,65536,.75),"bound chunk cache");
    H5AC_cache_config_t cache{};cache.version=H5AC__CURR_CACHE_CONFIG_VERSION;status(H5Pget_mdc_config(fapl,&cache),"get metadata cache");
    cache.set_initial_size=true;cache.initial_size=1024*1024;cache.min_size=1024*1024;cache.max_size=4u*1024*1024;status(H5Pset_mdc_config(fapl,&cache),"bound metadata cache");
    return Handle(H5Fopen(path.c_str(),H5F_ACC_RDONLY,fapl));
}
void hard_path(hid_t file,const std::string& path){
    std::size_t end=0;while(end<path.size()){end=path.find('/',end+1);const auto prefix=path.substr(0,end);H5L_info2_t link{};status(H5Lget_info2(file,prefix.c_str(),&link,H5P_DEFAULT),"missing selected link: "+prefix);require(link.type==H5L_TYPE_HARD,"external/soft selected links are unsupported: "+prefix);if(end==std::string::npos)break;}
}
std::vector<hsize_t> dimensions(hid_t space){const auto rank=H5Sget_simple_extent_ndims(space);require(rank>=0 && rank<=2,"unsupported rank");std::vector<hsize_t> dims(rank);status(H5Sget_simple_extent_dims(space,dims.data(),nullptr),"read dimensions");return dims;}
struct Strings {
    std::size_t used=0;
    static void* alloc(std::size_t n,void* context){auto& self=*static_cast<Strings*>(context);if(n>257 || self.used+n+sizeof(std::size_t)>256u*1024)return nullptr;auto* p=static_cast<std::size_t*>(std::malloc(n+sizeof(std::size_t)));if(!p)return nullptr;*p=n;self.used+=n+sizeof(std::size_t);return p+1;}
    static void free(void* ptr,void* context){if(!ptr)return;auto* p=static_cast<std::size_t*>(ptr)-1;static_cast<Strings*>(context)->used-=*p+sizeof(std::size_t);std::free(p);}
};
std::string attribute(hid_t object,const char* name){
    Handle a(H5Aopen(object,name,H5P_DEFAULT)),space(H5Aget_space(a)),type(H5Aget_type(a));require(dimensions(space).empty() && H5Tget_class(type)==H5T_STRING,"expected scalar string attribute");
    std::string out;
    if(H5Tis_variable_str(type)>0){
        Handle transfer(H5Pcreate(H5P_DATASET_XFER)),connector(H5VLget_connector_id(a));Strings strings;
        status(H5Pset_vlen_mem_manager(transfer,Strings::alloc,&strings,Strings::free,&strings),"bound attribute allocation");
        // H5Aread has no DXPL. The pinned native VOL API accepts it without
        // changing HDF5 defaults, adding a connector or relying on h5py converters.
        char* value=nullptr;
        try{
            struct Context {void* state=nullptr;Context(){status(H5VLopen_lib_context(&state),"open native attribute context");}~Context(){H5VLclose_lib_context(state);}};
            {Context context;status(H5VLattr_read(H5VLobject(a),connector,type,&value,transfer,nullptr),std::string("read bounded string attribute ")+name);}
            require(value!=nullptr && strings.used>0,"attribute allocation bypassed native limit");out=value;detail::bounded_utf8(out,256);}
        catch(...){if(value && !strings.used)H5free_memory(value);else H5Treclaim(type,space,transfer,&value);throw;}
        status(H5Treclaim(type,space,transfer,&value),"release bounded attribute");
    }else{
        const auto n=H5Tget_size(type);require(n && n<=256,"fixed attribute exceeds256 bytes");std::array<char,258> value{};
        status(H5Aread(a,type,value.data()),std::string("read fixed string attribute ")+name);out.assign(value.data(),std::find(value.begin(),value.end(),'\0')-value.begin());
        if(H5Tget_strpad(type)==H5T_STR_SPACEPAD)while(!out.empty() && out.back()==' ')out.pop_back();detail::bounded_utf8(out,256);
    }return out;
}
void encoding(hid_t object,const std::string& type,const std::string& version){require(attribute(object,"encoding-type")==type && attribute(object,"encoding-version")==version,"unsupported "+type+" encoding/version");}
void dataset(hid_t data,const H5adOptions& p){
    Handle dcpl(H5Dget_create_plist(data));const auto layout=H5Pget_layout(dcpl);require(layout==H5D_CONTIGUOUS || layout==H5D_COMPACT || layout==H5D_CHUNKED,"virtual/unknown storage is unsupported");
    require(H5Pget_external_count(dcpl)==0,"external dataset storage is unsupported");
    const auto filters=H5Pget_nfilters(dcpl);require(filters>=0 && filters<=3,"filter count exceeds admitted subset");
    for(int i=0;i<filters;++i){unsigned flags=0;std::size_t count=16;unsigned values[16];const auto f=H5Pget_filter2(dcpl,i,&flags,&count,values,0,nullptr,nullptr);require(f==H5Z_FILTER_DEFLATE || f==H5Z_FILTER_SHUFFLE || f==H5Z_FILTER_FLETCHER32,"unreviewed dataset filter");require(count<=16,"filter parameters exceed limit");}
    if(layout==H5D_CHUNKED){hsize_t dims[2];const auto n=H5Pget_chunk(dcpl,2,dims);require(n>0 && n<=2,"unsupported chunk rank");Handle type(H5Dget_type(data));std::uint64_t bytes=H5Tget_size(type);for(int i=0;i<n;++i){require(dims[i] && bytes<=p.chunk_bytes/dims[i],"decoded chunk exceeds byte limit");bytes*=dims[i];}}
}
struct Number {bool integer=false,signed_integer=false;std::uint64_t u=0;std::int64_t i=0;double d=0;};
std::uint64_t index(const Number& n){require(n.integer,"sparse index/shape is not integer");if(n.signed_integer){require(n.i>=0,"negative sparse index/shape");return std::uint64_t(n.i);}return n.u;}
double real(const Number& n){if(!n.integer){require(std::isfinite(n.d),"nonfinite numeric value/missing encoding unsupported");return n.d;}if(n.signed_integer){require(n.i>=-9007199254740992LL && n.i<=9007199254740992LL,"integer to continuous precision loss");return double(n.i);}require(n.u<=9007199254740992ULL,"integer to continuous precision loss");return double(n.u);}
std::uint64_t count(const Number& n){if(n.integer)return index(n);require(std::isfinite(n.d) && n.d>=0 && n.d<std::ldexp(1.,64) && n.d==std::floor(n.d),"count domain needs finite exact nonnegative integers");return std::uint64_t(n.d);}
void numeric_type(hid_t type){const auto kind=H5Tget_class(type);if(kind==H5T_INTEGER){require(H5Tget_size(type)<=8 && H5Tget_precision(type)<=64 && H5Tget_offset(type)==0,"unsupported integer dtype");}else require(kind==H5T_FLOAT && (H5Tequal(type,H5T_IEEE_F32LE)>0 || H5Tequal(type,H5T_IEEE_F32BE)>0 || H5Tequal(type,H5T_IEEE_F64LE)>0 || H5Tequal(type,H5T_IEEE_F64BE)>0),"unsupported numeric dtype");}
void numbers(hid_t data,const std::vector<hsize_t>& offset,const std::vector<hsize_t>& size,const std::function<void(std::size_t,const Number&)>& sink){
    Handle space(H5Dget_space(data)),type(H5Dget_type(data));numeric_type(type);std::size_t n=1;for(auto x:size){require(x<=65536 && n<=65536/std::max<hsize_t>(1,x),"numeric read exceeds bounded block");n*=x;}if(!n)return;
    status(H5Sselect_hyperslab(space,H5S_SELECT_SET,offset.data(),nullptr,size.data(),nullptr),"select numeric block");Handle memory(H5Screate_simple(int(size.size()),size.data(),nullptr));
    if(H5Tget_class(type)==H5T_FLOAT){std::vector<double> v(n);status(H5Dread(data,H5T_NATIVE_DOUBLE,memory,space,H5P_DEFAULT,v.data()),"read floating block");for(std::size_t i=0;i<n;++i)sink(i,{false,false,0,0,v[i]});}
    else if(H5Tget_sign(type)==H5T_SGN_2){std::vector<std::int64_t> v(n);status(H5Dread(data,H5T_NATIVE_INT64,memory,space,H5P_DEFAULT,v.data()),"read signed block");for(std::size_t i=0;i<n;++i)sink(i,{true,true,0,v[i],0});}
    else {std::vector<std::uint64_t> v(n);status(H5Dread(data,H5T_NATIVE_UINT64,memory,space,H5P_DEFAULT,v.data()),"read unsigned block");for(std::size_t i=0;i<n;++i)sink(i,{true,false,v[i],0,0});}
}
std::vector<hsize_t> shape(hid_t data){Handle s(H5Dget_space(data));return dimensions(s);}

void charge(std::size_t& used,std::size_t n,std::size_t limit){workflow_capture_charge(used,n,limit);}
std::vector<std::string> catalogue(hid_t file,const char* name,const H5adOptions& p,std::size_t& used,const std::atomic<bool>* c){
    hard_path(file,name);Handle group(H5Gopen2(file,name,H5P_DEFAULT));encoding(group,"dataframe","0.2.0");require(H5Aexists(group,"column-order")>0,"dataframe column-order absent");
    {Handle order(H5Aopen(group,"column-order",H5P_DEFAULT)),space(H5Aget_space(order)),type(H5Aget_type(order));const auto dims=dimensions(space);require(dims.size()==1 && dims[0]<=10000 && H5Tget_class(type)==H5T_STRING,"unsupported dataframe column-order metadata");}
    const auto key=attribute(group,"_index");detail::spatial_identifier(key);require(key.find('/')==std::string::npos && key!="." && key!="..","invalid dataframe index key");const auto path=std::string(name)+"/"+key;hard_path(file,path);Handle data(H5Dopen2(file,path.c_str(),H5P_DEFAULT));dataset(data,p);encoding(data,"string-array","0.2.0");const auto dims=shape(data);const auto max=std::string(name)=="obs"?p.max_observations:p.max_features;require(dims.size()==1 && dims[0] && dims[0]<=max,"catalogue axis exceeds declared limit");
    Handle type(H5Dget_type(data));require(H5Tget_class(type)==H5T_STRING && H5Tis_variable_str(type)>0 && H5Tget_cset(type)==H5T_CSET_UTF8,"index requires variable-length UTF8 strings");
    charge(used,std::size_t(dims[0])*sizeof(std::string),p.work_bytes);std::vector<std::string> out;out.reserve(std::size_t(dims[0]));
    Handle transfer(H5Pcreate(H5P_DATASET_XFER));Strings strings;status(H5Pset_vlen_mem_manager(transfer,Strings::alloc,&strings,Strings::free,&strings),"bound UTF8 allocation");
    for(hsize_t first=0;first<dims[0];first+=256){workflow_checkpoint(c);const hsize_t n=std::min<hsize_t>(256,dims[0]-first);Handle space(H5Dget_space(data)),memory(H5Screate_simple(1,&n,nullptr));status(H5Sselect_hyperslab(space,H5S_SELECT_SET,&first,nullptr,&n,nullptr),"select index block");std::array<char*,256> values{};const auto rc=H5Dread(data,type,memory,space,transfer,values.data());
        try{status(rc,"read bounded UTF8 catalogue");for(std::size_t i=0;i<n;++i){require(values[i]!=nullptr,"null index string");auto id=detail::spatial_identifier(values[i]);charge(used,id.capacity(),p.work_bytes);out.push_back(std::move(id));}}catch(...){H5Treclaim(type,memory,transfer,values.data());throw;}status(H5Treclaim(type,memory,transfer,values.data()),"release index strings");
    }
    std::size_t check=used;charge(check,out.size()*sizeof(std::string_view),p.work_bytes);std::vector<std::string_view> sorted;sorted.reserve(out.size());for(const auto& id:out)sorted.push_back(id);std::sort(sorted.begin(),sorted.end());require(std::adjacent_find(sorted.begin(),sorted.end())==sorted.end(),"duplicate observation/feature IDs");return out;
}
std::array<std::uint64_t,2> sparse_shape(hid_t group){Handle a(H5Aopen(group,"shape",H5P_DEFAULT)),s(H5Aget_space(a)),type(H5Aget_type(a));require(dimensions(s)==std::vector<hsize_t>{2} && H5Tget_class(type)==H5T_INTEGER && H5Tget_size(type)<=8,"sparse shape must contain two integers");std::array<std::uint64_t,2> result;if(H5Tget_sign(type)==H5T_SGN_2){std::int64_t v[2];status(H5Aread(a,H5T_NATIVE_INT64,v),"read shape");require(v[0]>0 && v[1]>0,"negative/empty sparse shape");result={std::uint64_t(v[0]),std::uint64_t(v[1])};}else status(H5Aread(a,H5T_NATIVE_UINT64,result.data()),"read shape");return result;}
}
struct H5adReader::Data {
    std::string path;H5adOptions options;H5adMetadata meta;std::shared_ptr<const FileVersion> version;
    Data(const std::string& input,H5adOptions p,const std::atomic<bool>* c):path(detail::export_path(input)),options(std::move(p)){
        options.validate();auto lock=library_lock(c);detail::ReaderFileGuard guard(path);require(guard.version()->bytes<=options.file_bytes,"file exceeds byte limit");Errors errors;auto file=open_file(path);encoding(file,"anndata","0.1.0");
        std::size_t used=8u*1024*1024+options.chunk_bytes+2u*1024*1024;charge(used,sizeof(Data)+path.capacity()+options.matrix.capacity()+options.spatial.capacity()+sizeof(CoordinateFrame)+options.frame.id.capacity()+options.frame.source_id.capacity(),options.work_bytes);
        meta.domain=*options.domain;meta.value_domain=options.value_domain;meta.frame=options.frame;
        meta.observations=catalogue(file,"obs",options,used,c);meta.features=catalogue(file,"var",options,used,c);
        if(options.matrix!="X"){hard_path(file,"layers");Handle parent(H5Gopen2(file,"layers",H5P_DEFAULT));encoding(parent,"dict","0.1.0");}
        hard_path(file,options.matrix);Handle object(H5Oopen(file,options.matrix.c_str(),H5P_DEFAULT));meta.encoding=attribute(object,"encoding-type");
        if(meta.encoding=="array"){encoding(object,"array","0.2.0");Handle data(H5Dopen2(file,options.matrix.c_str(),H5P_DEFAULT));dataset(data,options);{Handle type(H5Dget_type(data));numeric_type(type);}const auto dims=shape(data);require(dims==std::vector<hsize_t>{meta.observations.size(),meta.features.size()},"dense matrix axes differ from obs/var");require(dims[0]<=options.max_entries/dims[1],"dense entries exceed declared limit");meta.entries=dims[0]*dims[1];}
        else {require(meta.encoding=="csr_matrix" || meta.encoding=="csc_matrix","unsupported matrix encoding");encoding(object,meta.encoding,"0.1.0");require(sparse_shape(object)==std::array<std::uint64_t,2>{meta.observations.size(),meta.features.size()},"sparse matrix axes differ from obs/var");
            std::vector<hsize_t> nnz;for(const char* key:{"data","indices","indptr"}){const auto component=options.matrix+"/"+key;hard_path(file,component);Handle data(H5Dopen2(file,component.c_str(),H5P_DEFAULT));dataset(data,options);{Handle type(H5Dget_type(data));numeric_type(type);if(std::string(key)!="data")require(H5Tget_class(type)==H5T_INTEGER,"sparse pointers/indices require integer dtype");}const auto dims=shape(data);require(dims.size()==1,"sparse components must be vectors");if(std::string(key)=="data"){nnz=dims;meta.entries=dims[0];require(meta.entries<=options.max_entries,"sparse entries exceed declared limit");}else if(std::string(key)=="indices")require(dims==nnz,"sparse indices/data sizes differ");else require(dims[0]==(meta.encoding=="csr_matrix"?meta.observations.size():meta.features.size())+1,"sparse pointer axis differs");}
        }
        hard_path(file,"obsm");{Handle parent(H5Gopen2(file,"obsm",H5P_DEFAULT));encoding(parent,"dict","0.1.0");}
        hard_path(file,options.spatial);Handle spatial(H5Dopen2(file,options.spatial.c_str(),H5P_DEFAULT));dataset(spatial,options);encoding(spatial,"array","0.2.0");require(shape(spatial)==std::vector<hsize_t>{meta.observations.size(),2},"spatial requires exactly Nx2 in observation order");charge(used,meta.observations.size()*sizeof(Point),options.work_bytes);meta.positions.resize(meta.observations.size());
        for(std::size_t first=0;first<meta.positions.size();first+=32768){workflow_checkpoint(c);const auto n=std::min<std::size_t>(32768,meta.positions.size()-first);numbers(spatial,{first,0},{n,2},[&](std::size_t i,const Number& v){auto& point=meta.positions[first+i/2];(i%2?point.y:point.x)=real(v);});}
        meta.ignored_metadata={"obs columns except index","var columns except index","unselected layers/obsm entries","uns/obsp/varm/varp/raw and other unselected metadata"};
        FileFingerprintOptions fp;fp.byte_limit=options.file_bytes;const auto fingerprint=fingerprint_file(path,fp,c);meta.input_sha256=fingerprint->sha256;
        meta.source_sha256=detail::h5ad_input_identity(meta.input_sha256,options);meta.payload_bytes=used;guard.verify();version=guard.version();
    }
};
H5adReader::H5adReader(const std::string& path,H5adOptions options,const std::atomic<bool>* c):data_(std::make_shared<Data>(path,std::move(options),c)){}
const H5adMetadata& H5adReader::metadata()const{return data_->meta;}
std::size_t H5adReader::stream_work_bytes() const {
    const auto& d=*data_;std::size_t used=d.meta.payload_bytes;
    if(d.meta.encoding!="array"){const bool csr=d.meta.encoding=="csr_matrix";const auto outer=csr?d.meta.observations.size():d.meta.features.size(),inner=csr?d.meta.features.size():d.meta.observations.size();charge(used,(outer+1+2*inner)*sizeof(std::uint64_t),d.options.work_bytes);}return used;
}
void H5adReader::require_current(const std::atomic<bool>* c)const{workflow_checkpoint(c);detail::ReaderFileGuard guard(data_->path);const auto& a=*guard.version();const auto& b=*data_->version;if(a.identity!=b.identity || a.identity_scheme!=b.identity_scheme || a.bytes!=b.bytes || a.modified_token!=b.modified_token || a.change_token!=b.change_token || a.stamp_scheme!=b.stamp_scheme)throw WorkflowInputChanged("H5AD source changed");guard.verify();}
void H5adReader::stream(const std::function<void(const H5adEntry&)>& sink,const std::atomic<bool>* c)const {
    require(bool(sink),"native entry sink required");auto lock=library_lock(c);require_current(c);const auto& d=*data_;const auto& m=d.meta;Errors errors;auto file=open_file(d.path);
    const auto emit=[&](std::uint64_t obs,std::uint64_t feature,const Number& n){H5adEntry e;e.observation=obs;e.feature=feature;if(m.domain==ExpressionValueDomain::Counts)e.count=count(n);else e.continuous=real(n);sink(e);};
    if(m.encoding=="array"){
        Handle data(H5Dopen2(file,d.options.matrix.c_str(),H5P_DEFAULT));for(std::size_t row=0;row<m.observations.size();++row)for(std::size_t first=0;first<m.features.size();first+=65536){workflow_checkpoint(c);const auto n=std::min<std::size_t>(65536,m.features.size()-first);numbers(data,{row,first},{1,n},[&](std::size_t i,const Number& v){emit(row,first+i,v);});}
    }else {
        const bool csr=m.encoding=="csr_matrix";const auto outer=csr?m.observations.size():m.features.size(),inner=csr?m.features.size():m.observations.size();
        (void)stream_work_bytes();
        Handle pointers(H5Dopen2(file,(d.options.matrix+"/indptr").c_str(),H5P_DEFAULT)),indices(H5Dopen2(file,(d.options.matrix+"/indices").c_str(),H5P_DEFAULT)),values(H5Dopen2(file,(d.options.matrix+"/data").c_str(),H5P_DEFAULT));
        std::vector<std::uint64_t> ptr(outer+1);for(std::size_t first=0;first<ptr.size();first+=65536){workflow_checkpoint(c);const auto n=std::min<std::size_t>(65536,ptr.size()-first);numbers(pointers,{first},{n},[&](std::size_t i,const Number& v){ptr[first+i]=index(v);});}
        require(ptr.front()==0 && ptr.back()==m.entries && std::is_sorted(ptr.begin(),ptr.end()),"sparse pointers must be monotone0..nnz");
        for(std::size_t row=0;row<outer;++row){workflow_checkpoint(c);const auto n=ptr[row+1]-ptr[row];require(n<=inner,"sparse row/column exceeds unique inner axis");std::vector<std::uint64_t> ids(n);
            for(std::uint64_t first=0;first<n;first+=65536){const auto count=std::min<std::uint64_t>(65536,n-first);numbers(indices,{ptr[row]+first},{count},[&](std::size_t i,const Number& v){ids[first+i]=index(v);require(ids[first+i]<inner,"sparse inner index out of bounds");});}
            if(std::is_sorted(ids.begin(),ids.end()))require(std::adjacent_find(ids.begin(),ids.end())==ids.end(),"duplicate sparse coordinates");
            else {auto sorted=ids;std::sort(sorted.begin(),sorted.end());require(std::adjacent_find(sorted.begin(),sorted.end())==sorted.end(),"duplicate sparse coordinates");}
            for(std::uint64_t first=0;first<n;first+=65536){workflow_checkpoint(c);const auto count=std::min<std::uint64_t>(65536,n-first);numbers(values,{ptr[row]+first},{count},[&](std::size_t i,const Number& v){emit(csr?row:ids[first+i],csr?ids[first+i]:row,v);});}
        }
    }require_current(c);
}
#else
struct H5adReader::Data{};
H5adReader::H5adReader(const std::string&,H5adOptions,const std::atomic<bool>*){throw std::runtime_error("H5AD requires QUPATHXX_BUILD_H5AD");}
const H5adMetadata& H5adReader::metadata()const{throw std::runtime_error("H5AD unavailable");}
std::size_t H5adReader::stream_work_bytes()const{throw std::runtime_error("H5AD unavailable");}
void H5adReader::require_current(const std::atomic<bool>*)const{throw std::runtime_error("H5AD unavailable");}
void H5adReader::stream(const std::function<void(const H5adEntry&)>&,const std::atomic<bool>*)const{throw std::runtime_error("H5AD unavailable");}
#endif
}
