#include "qupathxx/tma.hpp"
#include "sha256.hpp"
#include "utf8_text.hpp"
#include <algorithm>
#include <charconv>
#include <cmath>
#include <limits>
#include <stdexcept>
#include <string_view>

namespace qpx {
namespace {
void limit(std::size_t n) {if(!n || n>256*1024*1024) throw std::invalid_argument("invalid TMA CSV byte limit");}
struct Csv {
    std::string value;std::size_t maximum;
    explicit Csv(std::size_t n):maximum(n) {limit(n);}
    void append(std::string_view s) {if(s.size()>maximum-value.size()) throw std::length_error("TMA CSV exceeds byte limit");value.append(s.data(),s.size());}
};
std::string quote(const std::string& s) {std::string out="\"";for(char c:s) {if(c=='"') out+='"';out+=c;}return out+'"';}
std::string real(double v) {if(!std::isfinite(v)) throw std::invalid_argument("nonfinite TMA geometry");char buffer[128];const auto r=std::to_chars(buffer,buffer+sizeof(buffer),v,std::chars_format::general,std::numeric_limits<double>::max_digits10);if(r.ec!=std::errc{}) throw std::overflow_error("TMA number formatting failed");return {buffer,r.ptr};}
const char* context_header="tma_table_schema,tma_metadata_document_revision,tma_grid_name,tma_row_index,tma_column_index,tma_row_label,tma_column_label,tma_core_name,tma_case_id,tma_missing,tma_core_revision,";
std::string context(const TmaGrid& grid,const Annotation& core) {
    const auto& t=*core.tma_core;return quote("qpx.tma-context/1")+','+std::to_string(grid.document_revision())+','+quote(t.grid_name())+','+std::to_string(t.row())+','+std::to_string(t.column())+','+quote(t.row_label())+','+quote(t.column_label())+','+quote(core.name)+','+quote(t.case_id())+','+(t.missing()?"1":"0")+','+std::to_string(core.revision)+',';
}
// Field spans retain the original encoding, including embedded quoted newlines.
// One bounded vector is reused for all rows; no per-cell feature copies occur.
template<class F> void records(const std::string& csv,F&& consume,const std::atomic<bool>* cancelled=nullptr) {
    std::vector<std::pair<std::size_t,std::size_t>> fields;fields.reserve(128);std::size_t row=0,field=0;bool quoted=false,closed=false,started=false;
    auto add=[&](std::size_t end) {if(fields.size()>=1024) throw std::length_error("TMA context input exceeds column limit");fields.emplace_back(field,end-field);};
    for(std::size_t i=0;i<csv.size();++i) {
        if((i&65535)==0 && cancelled && cancelled->load(std::memory_order_relaxed)) throw std::runtime_error("TMA case summary cancelled");
        const auto c=csv[i];
        if(quoted) {if(c=='"') {if(i+1<csv.size() && csv[i+1]=='"') ++i;else {quoted=false;closed=true;}}continue;}
        if(c=='"') {if(started || closed) throw std::invalid_argument("invalid CSV quote");quoted=true;started=true;continue;}
        if(c==',') {add(i);field=i+1;closed=started=false;continue;}
        if(c=='\r' || c=='\n') {
            if(c=='\r' && (i+1>=csv.size() || csv[i+1]!='\n')) throw std::invalid_argument("invalid CSV line ending");add(i);if(c=='\r') ++i;consume(row,i+1,fields);fields.clear();row=field=i+1;closed=started=false;continue;
        }
        if(closed) throw std::invalid_argument("data after CSV closing quote");started=true;
    }
    if(quoted || row!=csv.size() || !fields.empty() || csv.empty()) throw std::invalid_argument("CSV must contain complete terminated records");
}
std::string decode(const std::string& source,const std::pair<std::size_t,std::size_t>& span,std::size_t maximum) {
    std::string out;auto begin=span.first,end=begin+span.second;const bool quoted=begin<end && source[begin]=='"';if(quoted) {++begin;--end;}
    for(auto i=begin;i<end;++i) {if(out.size()>=maximum) throw std::length_error("TMA CSV selector exceeds limit");const auto c=source[i];out+=c;if(quoted && c=='"') ++i;}return out;
}
std::uint64_t number(const std::string& s) {std::uint64_t n=0;const auto r=std::from_chars(s.data(),s.data()+s.size(),n);if(s.empty() || r.ec!=std::errc{} || r.ptr!=s.data()+s.size() || std::to_string(n)!=s) throw std::invalid_argument("invalid TMA CSV object selector");return n;}
}
std::string tma_metadata_csv(const TmaGrid& grid,std::size_t maximum) {
    Csv csv(maximum);csv.append("table_schema,document_revision,grid_name,row_index,column_index,row_label,column_label,slot_state,object_id,object_revision,object_name,case_id,missing,center_x_px,center_y_px,width_px,height_px,area_px2\r\n");
    const auto& info=*grid.info();for(std::size_t row=0;row<info.rows();++row) for(std::size_t column=0;column<info.columns();++column) {
        auto text=quote("qpx.tma-metadata/1")+','+std::to_string(grid.document_revision())+','+quote(info.name())+','+std::to_string(row)+','+std::to_string(column)+','+quote(info.row_labels()[row])+','+quote(info.column_labels()[column])+',';
        const auto a=grid.core(row,column);if(!a) text+="\"vacant\",,,,,,,,,,";
        else {const auto& t=*a->tma_core;const auto b=a->bounds();text+=quote(t.missing()?"missing":"present")+','+std::to_string(a->id)+','+std::to_string(a->revision)+','+quote(a->name)+','+quote(t.case_id())+','+(t.missing()?"1":"0")+','+real(b.x+b.width/2)+','+real(b.y+b.height/2)+','+real(b.width)+','+real(b.height)+','+real(a->geometry.area());}
        text+="\r\n";csv.append(text);
    }return std::move(csv.value);
}
std::string tma_context_csv(const TmaGrid& grid,const std::string& table,std::size_t maximum,bool require_revision) {
    limit(maximum);if(table.size()>maximum) throw std::length_error("TMA input table exceeds byte limit");Csv csv(maximum);
    std::map<std::uint64_t,const Annotation*> cores;for(const auto& core:grid.cores()) cores.emplace(core.id,&core);
    std::size_t id=std::numeric_limits<std::size_t>::max(),revision=id,columns=0;bool first=true;
    records(table,[&](std::size_t begin,std::size_t end,const auto& fields) {
        if(first) {columns=fields.size();std::set<std::string> names;for(std::size_t i=0;i<fields.size();++i) {const auto name=decode(table,fields[i],128);if(!names.insert(name).second || name.rfind("tma_",0)==0) throw std::invalid_argument("duplicate or prejoined TMA CSV column");if(name=="source_object_id" || (name=="object_id" && id==std::numeric_limits<std::size_t>::max())) id=i;if(name=="source_object_revision") revision=i;}
            if(id==std::numeric_limits<std::size_t>::max() || (require_revision && revision==std::numeric_limits<std::size_t>::max())) throw std::invalid_argument("TMA input table lacks core selectors");csv.append(context_header);csv.append(std::string_view(table.data()+begin,end-begin));first=false;return;}
        if(fields.size()!=columns) throw std::invalid_argument("TMA input row has a different column count");const auto core_id=number(decode(table,fields[id],32));const auto found=cores.find(core_id);if(found==cores.end()) throw std::invalid_argument("table references an object outside the TMA snapshot");
        const auto& core=*found->second;if(require_revision && number(decode(table,fields[revision],32))!=core.revision) throw std::invalid_argument("TMA metadata does not match the recorded core revision");
        csv.append(context(grid,core));csv.append(std::string_view(table.data()+begin,end-begin));
    });return std::move(csv.value);
}

std::string tma_case_summary_csv(const TmaGrid& grid,const std::string& table,const std::string& value_column,
                               const std::string& weight_column,std::size_t maximum,bool allow_unversioned,
                               const std::atomic<bool>* cancelled) {
    limit(maximum);if(table.size()>maximum) throw std::length_error("TMA input table exceeds byte limit");
    detail::bounded_utf8(value_column,128);detail::bounded_utf8(weight_column,128);
    if(table.empty()!=value_column.empty() || (table.empty() && !weight_column.empty()))
        throw std::invalid_argument("TMA case measurements require both a table and value column");
    auto checkpoint=[&] {if(cancelled && cancelled->load(std::memory_order_relaxed)) throw std::runtime_error("TMA case summary cancelled");};
    struct Sum {
        double value=0,correction=0;
        void add(double v) {
            const auto next=value+v;
            if(!std::isfinite(next)) throw std::overflow_error("TMA case sum exceeds numeric range");
            // Neumaier compensation retains a small contribution even when
            // a later, larger term cancels the running sum.
            const auto delta=std::abs(value)>=std::abs(v)?(value-next)+v:(v-next)+value;
            const auto corrected=correction+delta;
            if(!std::isfinite(corrected)) throw std::overflow_error("TMA case compensation exceeds numeric range");
            correction=corrected;value=next;
        }
        double total() const {const auto result=value+correction;if(!std::isfinite(result)) throw std::overflow_error("TMA case sum exceeds numeric range");return result;}
    };
    struct Stats {
        std::uint64_t cores=0,present=0,missing=0,measured=0,rows=0,ignored=0;
        Sum values,weights,weighted;
        double minimum=0,maximum=0;
    };
    using Key=std::pair<std::string,std::uint64_t>;
    std::map<Key,Stats> groups;
    struct Core {const Annotation* annotation;Stats* stats;bool measured=false;};
    std::map<std::uint64_t,Core> cores;
    for(const auto& core:grid.cores()) {
        checkpoint();const auto& info=*core.tma_core;
        auto& stats=groups[{info.case_id(),info.case_id().empty()?core.id:0}];
        ++stats.cores;if(info.missing()) ++stats.missing;else ++stats.present;
        cores.emplace(core.id,Core{&core,&stats,false});
    }
    const auto absent=std::numeric_limits<std::size_t>::max();
    std::size_t columns=0,id=absent,revision=absent,value=absent,weight=absent;
    bool first=true,versioned=false;
    auto numeric=[&](const auto& span) {
        const auto text=decode(table,span,128);double result=0;
        const auto parsed=std::from_chars(text.data(),text.data()+text.size(),result,std::chars_format::general);
        if(text.empty() || parsed.ec!=std::errc{} || parsed.ptr!=text.data()+text.size() || !std::isfinite(result))
            throw std::invalid_argument("TMA case measurement must be a finite decimal number");
        return result;
    };
    if(!table.empty()) records(table,[&](std::size_t,std::size_t,const auto& fields) {
        checkpoint();
        if(first) {
            columns=fields.size();std::map<std::string,std::size_t> headers;
            for(std::size_t i=0;i<columns;++i) {const auto name=decode(table,fields[i],128);if(name.empty() || !headers.emplace(name,i).second) throw std::invalid_argument("empty or duplicate TMA case column");}
            auto find=[&](const std::string& name) {const auto at=headers.find(name);return at==headers.end()?absent:at->second;};
            id=find("source_object_id");revision=find("source_object_revision");
            if(id==absent) {id=find("object_id");if(revision==absent) revision=find("object_revision");}
            value=find(value_column);if(!weight_column.empty()) weight=find(weight_column);
            if(id==absent || value==absent || (!weight_column.empty() && weight==absent) || (revision==absent && !allow_unversioned))
                throw std::invalid_argument("TMA case input lacks core revision or measurement columns");
            versioned=revision!=absent;first=false;return;
        }
        if(fields.size()!=columns) throw std::invalid_argument("TMA case input row has a different column count");
        const auto identity=number(decode(table,fields[id],32));const auto at=cores.find(identity);
        if(at==cores.end()) throw std::invalid_argument("case table references an object outside the TMA snapshot");
        auto& core=at->second;
        if(versioned && number(decode(table,fields[revision],32))!=core.annotation->revision)
            throw std::invalid_argument("TMA case input does not match the recorded core revision");
        const auto v=numeric(fields[value]),w=weight==absent?1.:numeric(fields[weight]);
        if(w<0) throw std::invalid_argument("TMA case weight must be nonnegative");
        auto& stats=*core.stats;
        if(core.annotation->tma_core->missing()) {++stats.ignored;return;}
        const auto product=v*w;
        if(!std::isfinite(product)) throw std::overflow_error("TMA weighted value exceeds numeric range");
        if(v!=0 && w!=0 && product==0) throw std::underflow_error("TMA weighted value is below numeric range");
        stats.values.add(v);stats.weights.add(w);stats.weighted.add(product);
        if(!stats.rows) stats.minimum=stats.maximum=v;else {stats.minimum=std::min(stats.minimum,v);stats.maximum=std::max(stats.maximum,v);}
        ++stats.rows;if(!core.measured) {++stats.measured;core.measured=true;}
    },cancelled);
    std::string digest;
    if(!table.empty()) {Sha256 hash;for(std::size_t at=0;at<table.size();) {checkpoint();const auto n=std::min<std::size_t>(65536,table.size()-at);hash.update(table.data()+at,n);at+=n;}digest=hash.finish();}
    Csv csv(maximum);
    csv.append("table_schema,metadata_document_revision,grid_name,grid_slots,grid_vacant_slots,case_id,unassigned_core_id,cores,present_cores,missing_cores,measured_cores,measurement_rows,ignored_missing_rows,value_column,weight_column,input_table_sha256,measurement_revision_status,value_sum,weight_sum,weighted_sum,mean,weighted_mean,minimum,maximum\r\n");
    const auto slots=grid.info()->rows()*grid.info()->columns();
    for(const auto& entry:groups) {
        checkpoint();const auto& key=entry.first;const auto& st=entry.second;
        std::string line=quote("qpx.tma-case-summary/1")+','+std::to_string(grid.document_revision())+','+quote(grid.info()->name())+','+
            std::to_string(slots)+','+std::to_string(slots-grid.cores().size())+','+quote(key.first)+','+(key.second?std::to_string(key.second):"")+','+
            std::to_string(st.cores)+','+std::to_string(st.present)+','+std::to_string(st.missing)+','+std::to_string(st.measured)+','+
            std::to_string(st.rows)+','+std::to_string(st.ignored)+','+quote(value_column)+','+quote(weight_column)+','+quote(digest)+','+
            quote(st.rows?(versioned?"verified":"unversioned"):"no_measurements")+',';
        if(st.rows) {
            const auto values=st.values.total(),weights=st.weights.total(),weighted=st.weighted.total();
            auto divide=[](double numerator,double denominator) {
                const auto result=numerator/denominator;
                if(!std::isfinite(result)) throw std::overflow_error("TMA case mean exceeds numeric range");
                if(numerator!=0 && result==0) throw std::underflow_error("TMA case mean is below numeric range");
                return result;
            };
            line+=real(values)+','+real(weights)+','+real(weighted)+','+real(divide(values,double(st.rows)))+','+
                (weights>0?real(divide(weighted,weights)):"")+','+real(st.minimum)+','+real(st.maximum);
        } else line+=",,,,,,";
        line+="\r\n";csv.append(line);
    }
    return std::move(csv.value);
}
}
