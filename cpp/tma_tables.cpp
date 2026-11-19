#include "qupathxx/tma.hpp"
#include "tma_internal.hpp"
#include "sha256.hpp"
#include "utf8_text.hpp"
#include "decimal_quantity.hpp"
#include <algorithm>
#include <charconv>
#include <cmath>
#include <limits>
#include <stdexcept>
#include <string_view>
#include <tuple>

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
std::string_view csv_without_signature(std::string_view input) {
    constexpr std::string_view signature="\xef\xbb\xbf";
    if(input.substr(0,signature.size())==signature) input.remove_prefix(signature.size());
    return input;
}
// Field spans retain the original encoding, including embedded quoted newlines.
// One bounded vector is reused for all rows; no per-cell feature copies occur.
template<class F> void records(std::string_view csv,F&& consume,const std::atomic<bool>* cancelled=nullptr) {
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
    if(quoted) throw std::invalid_argument("CSV contains an unterminated quoted field");
    if(csv.empty()) throw std::invalid_argument("CSV must contain at least one record");
    if(row!=csv.size()) {
        add(csv.size());
        consume(row,csv.size(),fields);
    }
}
std::string decode(std::string_view source,const std::pair<std::size_t,std::size_t>& span,std::size_t maximum) {
    std::string out;auto begin=span.first,end=begin+span.second;const bool quoted=begin<end && source[begin]=='"';if(quoted) {++begin;--end;}
    for(auto i=begin;i<end;++i) {if(out.size()>=maximum) throw std::length_error("TMA CSV selector exceeds limit");const auto c=source[i];out+=c;if(quoted && c=='"') ++i;}return out;
}
std::uint64_t number(const std::string& s) {std::uint64_t n=0;const auto r=std::from_chars(s.data(),s.data()+s.size(),n);if(s.empty() || r.ec!=std::errc{} || r.ptr!=s.data()+s.size() || std::to_string(n)!=s) throw std::invalid_argument("invalid TMA CSV object selector");return n;}
}
namespace {
std::string metadata_csv(const TmaGrid& grid,std::size_t maximum,const std::atomic<bool>* cancelled) {
    Csv csv(maximum);csv.append("table_schema,document_revision,grid_name,row_index,column_index,row_label,column_label,slot_state,object_id,object_revision,object_name,case_id,missing,center_x_px,center_y_px,width_px,height_px,area_px2\r\n");
    const auto& info=*grid.info();for(std::size_t row=0;row<info.rows();++row) for(std::size_t column=0;column<info.columns();++column) {
        if(cancelled && cancelled->load(std::memory_order_relaxed)) throw std::runtime_error("TMA cohort manifest cancelled");
        auto text=quote("qpx.tma-metadata/1")+','+std::to_string(grid.document_revision())+','+quote(info.name())+','+std::to_string(row)+','+std::to_string(column)+','+quote(info.row_labels()[row])+','+quote(info.column_labels()[column])+',';
        const auto a=grid.core(row,column);if(!a) text+="\"vacant\",,,,,,,,,,";
        else {const auto& t=*a->tma_core;const auto b=a->bounds();text+=quote(t.missing()?"missing":"present")+','+std::to_string(a->id)+','+std::to_string(a->revision)+','+quote(a->name)+','+quote(t.case_id())+','+(t.missing()?"1":"0")+','+real(b.x+b.width/2)+','+real(b.y+b.height/2)+','+real(b.width)+','+real(b.height)+','+real(a->geometry.area());}
        text+="\r\n";csv.append(text);
    }return std::move(csv.value);
}
}
std::string tma_metadata_csv(const TmaGrid& grid,std::size_t maximum) {return metadata_csv(grid,maximum,nullptr);}
std::string tma_context_csv(const TmaGrid& grid,const std::string& input,std::size_t maximum,bool require_revision) {
    limit(maximum);if(input.size()>maximum) throw std::length_error("TMA input table exceeds byte limit");Csv csv(maximum);
    const auto table=csv_without_signature(input);
    csv.append(std::string_view(input).substr(0,input.size()-table.size()));
    std::map<std::uint64_t,const Annotation*> cores;for(const auto& core:grid.cores()) cores.emplace(core.id,&core);
    std::size_t id=std::numeric_limits<std::size_t>::max(),revision=id,columns=0;bool first=true;
    records(table,[&](std::size_t begin,std::size_t end,const auto& fields) {
        if(first) {columns=fields.size();std::set<std::string> names;for(std::size_t i=0;i<fields.size();++i) {const auto name=decode(table,fields[i],128);if(!names.insert(name).second || name.rfind("tma_",0)==0) throw std::invalid_argument("duplicate or prejoined TMA CSV column");if(name=="source_object_id" || (name=="object_id" && id==std::numeric_limits<std::size_t>::max())) id=i;if(name=="source_object_revision") revision=i;}
            if(id==std::numeric_limits<std::size_t>::max() || (require_revision && revision==std::numeric_limits<std::size_t>::max())) throw std::invalid_argument("TMA input table lacks core selectors");csv.append(context_header);csv.append(table.substr(begin,end-begin));first=false;return;}
        if(fields.size()!=columns) throw std::invalid_argument("TMA input row has a different column count");const auto core_id=number(decode(table,fields[id],32));const auto found=cores.find(core_id);if(found==cores.end()) throw std::invalid_argument("table references an object outside the TMA snapshot");
        const auto& core=*found->second;if(require_revision && number(decode(table,fields[revision],32))!=core.revision) throw std::invalid_argument("TMA metadata does not match the recorded core revision");
        csv.append(context(grid,core));csv.append(table.substr(begin,end-begin));
    });return std::move(csv.value);
}

namespace {
using Captures=std::vector<std::pair<std::uint64_t,const TmaGrid*>>;
void checkpoint(const std::atomic<bool>* cancelled) {
    if(cancelled && cancelled->load(std::memory_order_relaxed)) throw std::runtime_error("TMA cohort cancelled");
}
Captures captures(const TmaCohort& cohort,std::size_t maximum,const std::atomic<bool>* cancelled) {
    detail::validate_tma_cohort(cohort,maximum,cancelled);
    Captures result;result.reserve(cohort.size());
    for(const auto& entry:cohort) result.emplace_back(entry.first,entry.second.get());
    return result;
}
void hash_update(Sha256& hash,std::string_view text,const std::atomic<bool>* cancelled) {
    for(std::size_t at=0;at<text.size();) {checkpoint(cancelled);const auto n=std::min<std::size_t>(65536,text.size()-at);hash.update(text.data()+at,n);at+=n;}
}
std::string hash_text(std::string_view text,const std::atomic<bool>* cancelled) {
    Sha256 hash;hash_update(hash,text,cancelled);
    return hash.finish();
}
std::string cohort_manifest(const Captures& sources,std::size_t maximum,const std::atomic<bool>* cancelled) {
    Csv csv(maximum);csv.append("table_schema,image_id,metadata_document_revision,grid_name,grid_slots,grid_vacant_slots,cores,metadata_csv_sha256\r\n");
    for(const auto& entry:sources) {
        checkpoint(cancelled);const auto& grid=*entry.second;const auto slots=grid.info()->rows()*grid.info()->columns();
        const auto digest=hash_text(metadata_csv(grid,maximum,cancelled),cancelled);
        csv.append(quote("qpx.tma-cohort-sources/1")+','+std::to_string(entry.first)+','+std::to_string(grid.document_revision())+','+
            quote(grid.info()->name())+','+std::to_string(slots)+','+std::to_string(slots-grid.cores().size())+','+
            std::to_string(grid.cores().size())+','+quote(digest)+"\r\n");
    }
    return std::move(csv.value);
}
std::string case_summary(const Captures& sources,bool cohort,const std::vector<std::string_view>& tables,const std::string& value_column,
                               const std::string& weight_column,std::size_t maximum,bool allow_unversioned,
                               const std::atomic<bool>* cancelled,detail::TmaCaseInputScope scope) {
    limit(maximum);
    std::size_t remaining=maximum;
    for(const auto table:tables) {
        if(table.empty()) throw std::invalid_argument("TMA measurement CSV is empty");
        if(table.size()>remaining) throw std::length_error("TMA input tables exceed byte limit");
        remaining-=table.size();
    }
    detail::bounded_utf8(value_column,128);detail::bounded_utf8(weight_column,128);
    if(tables.empty()!=value_column.empty() || (tables.empty() && !weight_column.empty()))
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
        std::uint64_t cores=0,present=0,missing=0,measured=0,rows=0,ignored=0,grids=0,last_image=0;
        Sum values,weights,weighted;
        double minimum=0,maximum=0;
    };
    using Key=std::tuple<std::string,std::uint64_t,std::uint64_t>;
    std::map<Key,Stats> groups;
    struct Core {const Annotation* annotation;Stats* stats;bool measured=false;};
    std::map<std::pair<std::uint64_t,std::uint64_t>,Core> cores;
    std::size_t total_slots=0,total_vacancies=0;
    for(const auto& source:sources) {
      const auto& grid=*source.second;const auto slots=grid.info()->rows()*grid.info()->columns();
      total_slots+=slots;total_vacancies+=slots-grid.cores().size();
      for(const auto& core:grid.cores()) {
        checkpoint();const auto& info=*core.tma_core;
        auto& stats=groups[{info.case_id(),info.case_id().empty()?source.first:0,info.case_id().empty()?core.id:0}];
        if(!stats.grids || stats.last_image!=source.first) {++stats.grids;stats.last_image=source.first;}
        ++stats.cores;if(info.missing()) ++stats.missing;else ++stats.present;
        cores.emplace(std::make_pair(source.first,core.id),Core{&core,&stats,false});
      }
    }
    const auto absent=std::numeric_limits<std::size_t>::max();
    std::size_t columns=0,id=absent,revision=absent,value=absent,weight=absent,image=absent,batch_image=absent;
    std::size_t revision_object=absent;
    std::size_t project=absent,batch_project=absent;
    auto selected_image=scope.image_id;
    bool versioned=false;
    auto numeric=[](std::string_view table,const auto& span) {
        const auto text=decode(table,span,128);const auto result=detail::finite_decimal_number(text);
        if(!result)
            throw std::invalid_argument("TMA case measurement must be a finite decimal number");
        return *result;
    };
    std::string_view header,header_ending;
    std::map<std::string,std::size_t> header_columns;
    std::vector<std::size_t> column_order;
    auto consume=[&](std::string_view table,std::size_t begin,std::size_t end,const auto& fields,std::size_t& hash_start) {
        checkpoint();
        if(!begin) {
            const auto field_end=fields.back().first+fields.back().second;
            const auto current=table.substr(0,field_end);
            const auto ending=table.substr(field_end,end-field_end);
            // Header field bytes establish the schema. The first available
            // record terminator is used only when an EOF row needs a separator.
            if(header_ending.empty()) header_ending=ending;
            if(!header.empty()) {
                if(current!=header) {
                    if(!scope.match_columns_by_name || fields.size()!=columns)
                        throw std::invalid_argument("TMA measurement CSV headers differ");
                    column_order.assign(columns,absent);
                    for(std::size_t i=0;i<columns;++i) {
                        const auto name=decode(table,fields[i],128);
                        const auto at=header_columns.find(name);
                        if(at==header_columns.end()) throw std::invalid_argument("TMA measurement CSV column names differ");
                        if(column_order[at->second]!=absent) throw std::invalid_argument("duplicate TMA case column");
                        column_order[at->second]=i;
                    }
                }
                hash_start=end;
                return;
            }
            header=current;
            columns=fields.size();
            for(std::size_t i=0;i<columns;++i) {const auto name=decode(table,fields[i],128);if(name.empty() || !header_columns.emplace(name,i).second) throw std::invalid_argument("empty or duplicate TMA case column");}
            auto find=[&](const std::string& name) {const auto at=header_columns.find(name);return at==header_columns.end()?absent:at->second;};
            image=find("source_image_id");batch_image=find("batch_image_id");
            if(image==absent) image=batch_image;
            if(image==absent) image=find("image_id");
            if(cohort && image==absent) throw std::invalid_argument("TMA cohort input lacks image selectors");
            if(!scope.project_identity.empty()) {
                project=find("project_identity");batch_project=find("batch_project_identity");
            }
            const auto legacy_id=find("object_id");
            id=find("source_object_id");revision=find("source_object_revision");
            if(id==absent) id=legacy_id;
            if(revision==absent) {
                revision=find("object_revision");
                if(revision!=absent && legacy_id!=id) revision_object=legacy_id;
            }
            value=find(value_column);if(!weight_column.empty()) weight=find(weight_column);
            if(id==absent || value==absent || (!weight_column.empty() && weight==absent) || (revision==absent && !allow_unversioned))
                throw std::invalid_argument("TMA case input lacks core revision or measurement columns");
            versioned=revision!=absent;return;
        }
        if(fields.size()!=columns) throw std::invalid_argument("TMA case input row has a different column count");
        const auto field=[&](std::size_t column) -> const auto& {
            return fields[column_order.empty()?column:column_order[column]];
        };
        for(const auto column:{project,batch_project})
            if(column!=absent && std::string_view(decode(table,field(column),128))!=scope.project_identity)
                throw std::invalid_argument("TMA case input does not match the selected project identity");
        std::uint64_t image_id=0;
        if(image!=absent) {
            image_id=number(decode(table,field(image),32));
            if(!image_id || (batch_image!=absent && batch_image!=image && number(decode(table,field(batch_image),32))!=image_id))
                throw std::invalid_argument("TMA case input has invalid or conflicting image selectors");
            if(!cohort) {
                if(!selected_image) selected_image=image_id;
                if(image_id!=selected_image) throw std::invalid_argument("TMA case input does not match the selected image");
            }
        }
        const auto identity=number(decode(table,field(id),32));
        if(revision_object!=absent && number(decode(table,field(revision_object),32))!=identity)
            throw std::invalid_argument("TMA case input has a legacy revision for a different object");
        const auto at=cores.find({cohort?image_id:0,identity});
        if(at==cores.end()) throw std::invalid_argument("case table references an object outside the TMA snapshot");
        auto& core=at->second;
        if(versioned && number(decode(table,field(revision),32))!=core.annotation->revision)
            throw std::invalid_argument("TMA case input does not match the recorded core revision");
        const auto v=numeric(table,field(value)),w=weight==absent?1.:numeric(table,field(weight));
        if(w<0) throw std::invalid_argument("TMA case weight must be nonnegative");
        auto& stats=*core.stats;
        if(core.annotation->tma_core->missing()) {++stats.ignored;return;}
        const auto product=v*w;
        if(!std::isfinite(product)) throw std::overflow_error("TMA weighted value exceeds numeric range");
        if(v!=0 && w!=0 && product==0) throw std::underflow_error("TMA weighted value is below numeric range");
        stats.values.add(v);stats.weights.add(w);stats.weighted.add(product);
        if(!stats.rows) stats.minimum=stats.maximum=v;else {stats.minimum=std::min(stats.minimum,v);stats.maximum=std::max(stats.maximum,v);}
        ++stats.rows;if(!core.measured) {++stats.measured;core.measured=true;}
    };
    std::string digest;
    if(!tables.empty()) {
        Sha256 input_hash;
        bool needs_separator=false;
        for(const auto original:tables) {
            const auto table=csv_without_signature(original);
            column_order.clear();
            std::size_t hash_start=0;
            records(table,[&](std::size_t begin,std::size_t end,const auto& fields) {
                consume(table,begin,end,fields,hash_start);
                if(begin && !column_order.empty()) {
                    if(needs_separator) hash_update(input_hash,header_ending,cancelled);
                    for(std::size_t i=0;i<columns;++i) {
                        if(i) input_hash.update(",",1);
                        const auto [start,size]=fields[column_order[i]];
                        hash_update(input_hash,table.substr(start,size),cancelled);
                    }
                    const auto row_end=fields.back().first+fields.back().second;
                    hash_update(input_hash,table.substr(row_end,end-row_end),cancelled);
                    needs_separator=row_end==end;
                }
            },cancelled);
            if(!column_order.empty()) continue;
            // Keep the first file's exact bytes, including its UTF-8 signature.
            // Later file headers (and any signatures) are omitted from pooling.
            const auto segment=hash_start?table.substr(hash_start):original;
            if(segment.empty()) continue;
            // Keep the exact last record when it is also the last hashed row.
            // Insert a delimiter only to separate it from a later file's rows.
            if(needs_separator) hash_update(input_hash,header_ending,cancelled);
            hash_update(input_hash,segment,cancelled);
            needs_separator=segment.back()!='\n';
        }
        digest=input_hash.finish();
    }
    Csv csv(maximum);
    if(cohort) csv.append("table_schema,cohort_grids,cohort_slots,cohort_vacant_slots,source_manifest_sha256,case_id,unassigned_image_id,unassigned_core_id,grids,cores,present_cores,missing_cores,measured_cores,measurement_rows,ignored_missing_rows,value_column,weight_column,input_table_sha256,measurement_revision_status,value_sum,weight_sum,weighted_sum,mean,weighted_mean,minimum,maximum\r\n");
    else csv.append("table_schema,metadata_document_revision,grid_name,grid_slots,grid_vacant_slots,case_id,unassigned_core_id,cores,present_cores,missing_cores,measured_cores,measurement_rows,ignored_missing_rows,value_column,weight_column,input_table_sha256,measurement_revision_status,value_sum,weight_sum,weighted_sum,mean,weighted_mean,minimum,maximum\r\n");
    const auto& grid=*sources.front().second;
    const auto slots=grid.info()->rows()*grid.info()->columns();
    const auto manifest_digest=cohort?hash_text(cohort_manifest(sources,maximum,cancelled),cancelled):"";
    for(const auto& entry:groups) {
        checkpoint();const auto& key=entry.first;const auto& st=entry.second;
        const auto& case_id=std::get<0>(key);const auto unassigned_image=std::get<1>(key),unassigned_core=std::get<2>(key);
        std::string line;
        if(cohort) line=quote("qpx.tma-cohort-case-summary/1")+','+std::to_string(sources.size())+','+std::to_string(total_slots)+','+
            std::to_string(total_vacancies)+','+quote(manifest_digest)+','+quote(case_id)+','+(unassigned_image?std::to_string(unassigned_image):"")+','+
            (unassigned_core?std::to_string(unassigned_core):"")+','+std::to_string(st.grids)+',';
        else line=quote("qpx.tma-case-summary/1")+','+std::to_string(grid.document_revision())+','+quote(grid.info()->name())+','+
            std::to_string(slots)+','+std::to_string(slots-grid.cores().size())+','+quote(case_id)+','+(unassigned_core?std::to_string(unassigned_core):"")+',';
        line+=std::to_string(st.cores)+','+std::to_string(st.present)+','+std::to_string(st.missing)+','+std::to_string(st.measured)+','+
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
std::string tma_case_summary_csv(const TmaGrid& grid,const std::string& table,const std::string& value,
    const std::string& weight,std::size_t maximum,bool unversioned,const std::atomic<bool>* cancelled) {
    return detail::tma_scoped_case_summary_csv(grid,table.empty()?std::vector<std::string_view>{}:std::vector<std::string_view>{table},value,weight,maximum,unversioned,cancelled,{});
}
std::string detail::tma_scoped_case_summary_csv(const TmaGrid& grid,const std::vector<std::string_view>& tables,const std::string& value,
    const std::string& weight,std::size_t maximum,bool unversioned,const std::atomic<bool>* cancelled,TmaCaseInputScope scope) {
    return case_summary({{0,&grid}},false,tables,value,weight,maximum,unversioned,cancelled,scope);
}
void detail::validate_tma_cohort(const TmaCohort& cohort,std::size_t maximum,const std::atomic<bool>* cancelled) {
    limit(maximum);checkpoint(cancelled);
    if(cohort.empty() || cohort.size()>4096) throw std::invalid_argument("TMA cohort requires 1..4096 grids");
    std::size_t bytes=0;
    for(const auto& entry:cohort) {
        checkpoint(cancelled);
        if(!entry.first || !entry.second) throw std::invalid_argument("TMA cohort requires positive image IDs and nonnull grids");
        if(entry.second->payload_bytes()>maximum-bytes) throw std::length_error("TMA cohort captures exceed byte limit");
        bytes+=entry.second->payload_bytes();
    }
}
std::string tma_cohort_manifest_csv(const TmaCohort& cohort,std::size_t maximum,const std::atomic<bool>* cancelled) {
    return cohort_manifest(captures(cohort,maximum,cancelled),maximum,cancelled);
}
std::string tma_cohort_case_summary_csv(const TmaCohort& cohort,const std::string& table,const std::string& value,
    const std::string& weight,std::size_t maximum,bool unversioned,const std::atomic<bool>* cancelled) {
    return detail::tma_scoped_case_summary_csv(cohort,table.empty()?std::vector<std::string_view>{}:std::vector<std::string_view>{table},value,weight,maximum,unversioned,cancelled,{});
}
std::string detail::tma_scoped_case_summary_csv(const TmaCohort& cohort,const std::vector<std::string_view>& tables,const std::string& value,
    const std::string& weight,std::size_t maximum,bool unversioned,const std::atomic<bool>* cancelled,TmaCaseInputScope scope) {
    return case_summary(captures(cohort,maximum,cancelled),true,tables,value,weight,maximum,unversioned,cancelled,scope);
}
}
