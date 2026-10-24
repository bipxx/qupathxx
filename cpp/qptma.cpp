#include "qupathxx/qptma.hpp"
#include "tma_internal.hpp"
#include "decimal_quantity.hpp"
#include "utf8_text.hpp"
#include "atomic_export.hpp"
#include "reader_file_guard.hpp"
#include <array>
#include <charconv>
#include <cmath>
#include <fstream>
#include <iomanip>
#include <limits>
#include <locale>
#include <sstream>
#include <string_view>

namespace qpx {
namespace {
void quota(std::size_t limit) {if(!limit || limit>64*1024*1024) throw std::invalid_argument("qptma byte limit must be 1..64 MiB");}
void field(const std::string& text,std::size_t limit) {
    detail::bounded_utf8(text,limit);
    if(text.find_first_of("\t\r\n")!=std::string::npos) throw std::invalid_argument("qptma text field contains a delimiter or line break");
}
struct Lines {
    const std::string& text;std::size_t at=0;
    std::string_view next() {
        if(at>=text.size()) throw std::invalid_argument("truncated qptma grid");
        const auto begin=at,end=text.find('\n',at);at=end==std::string::npos?text.size():end+1;
        auto view=std::string_view(text).substr(begin,(end==std::string::npos?text.size():end)-begin);
        if(!view.empty() && view.back()=='\r')view.remove_suffix(1);
        return view;
    }
};
std::size_t dimension(std::string_view text,std::string_view prefix) {
    if(text.substr(0,prefix.size())!=prefix) throw std::invalid_argument("invalid qptma dimension header");
    text.remove_prefix(prefix.size());std::size_t value=0;
    const auto result=std::from_chars(text.data(),text.data()+text.size(),value);
    if(result.ec!=std::errc{} || result.ptr!=text.data()+text.size() || !value || value>512) throw std::invalid_argument("invalid qptma dimension");
    return value;
}
std::vector<std::string> fields(std::string_view line) {
    // QuPath's qptma TSV reader preserves quotes literally; this is not CSV.
    std::vector<std::string> result;std::size_t at=0;
    while(true) {
        if(result.size()==7)throw std::invalid_argument("too many qptma fields");
        const auto end=line.find('\t',at);
        result.emplace_back(line.substr(at,(end==std::string_view::npos?line.size():end)-at));
        if(end==std::string_view::npos)break;at=end+1;
    }
    if(result.size()!=7)throw std::invalid_argument("qptma row must have seven fields");return result;
}
double number(const std::string& value) {
    const auto result=detail::finite_decimal_number(value);
    if(!result) throw std::invalid_argument("invalid finite qptma coordinate");return *result;
}
std::string decimal(double value) {
    std::ostringstream out;out.imbue(std::locale::classic());out<<std::setprecision(std::numeric_limits<double>::max_digits10)<<value;return out.str();
}
}
void QptmaCore::validate() const {
    field(name,4096);field(case_id,256);const auto& b=bounds;
    if(!std::isfinite(b.x) || !std::isfinite(b.y) || !std::isfinite(b.width) || !std::isfinite(b.height) ||
       b.width<=0 || b.height<=0 || b.width>1e12 || b.height>1e12 || std::abs(b.x)+b.width>1e15 || std::abs(b.y)+b.height>1e15)
        throw std::invalid_argument("invalid qptma core bounds");
}
void QptmaData::validate() const {
    if(!rows || !columns || rows>512 || columns>512 || rows>10000/columns || cores.size()!=rows*columns)
        throw std::invalid_argument("qptma requires a complete 1..10000 core grid with at most 512 rows/columns");
    field(source_hint,32768);field(image_name,32768);for(const auto& core:cores)core.validate();
}
QptmaData parse_qptma(const std::string& text,std::size_t limit,const std::atomic<bool>* cancel) {
    detail::export_checkpoint(cancel);quota(limit);detail::bounded_utf8(text,limit);Lines lines{text};
    if(text.compare(0,3,"\xef\xbb\xbf")==0)lines.at=3;
    QptmaData out;out.source_hint=std::string(lines.next());out.image_name=std::string(lines.next());
    field(out.source_hint,32768);field(out.image_name,32768);
    if(!lines.next().empty()) throw std::invalid_argument("qptma prelude separator missing");
    out.columns=dimension(lines.next(),"TMA grid width: ");out.rows=dimension(lines.next(),"TMA grid height: ");
    if(out.rows>10000/out.columns) throw std::length_error("qptma grid exceeds core limit");
    if(lines.next()!="Core name\tX\tY\tWidth\tHeight\tPresent\tCase ID") throw std::invalid_argument("invalid qptma column header");
    out.cores.reserve(out.rows*out.columns);
    for(std::size_t i=0;i<out.rows*out.columns;++i) {
        detail::export_checkpoint(cancel);auto row=fields(lines.next());
        if(row[5]!="true" && row[5]!="false") throw std::invalid_argument("invalid qptma presence value");
        QptmaCore core;core.name=std::move(row[0]);core.case_id=std::move(row[6]);core.missing=row[5]=="false";
        core.bounds={number(row[1]),number(row[2]),number(row[3]),number(row[4])};out.cores.push_back(std::move(core));
    }
    if(lines.at!=text.size()) throw std::invalid_argument("extra qptma rows or trailing data");
    out.validate();detail::export_checkpoint(cancel);return out;
}
std::string qptma_text(const QptmaData& data,std::size_t limit,const std::atomic<bool>* cancel) {
    detail::export_checkpoint(cancel);quota(limit);data.validate();std::string out;
    auto append=[&](const std::string& text) {if(text.size()>limit-out.size())throw std::length_error("qptma exceeds byte limit");out+=text;};
    append(data.source_hint+"\n"+data.image_name+"\n\nTMA grid width: "+std::to_string(data.columns)+"\nTMA grid height: "+std::to_string(data.rows)+"\nCore name\tX\tY\tWidth\tHeight\tPresent\tCase ID\n");
    for(const auto& core:data.cores) {
        detail::export_checkpoint(cancel);const auto& b=core.bounds;
        append(core.name+"\t"+decimal(b.x)+"\t"+decimal(b.y)+"\t"+decimal(b.width)+"\t"+decimal(b.height)+"\t"+(core.missing?"false":"true")+"\t"+core.case_id+"\n");
    }
    return out;
}
std::vector<ObjectEdit> qptma_grid_edits(const QptmaData& data,std::string name,const std::vector<std::string>& rows,const std::vector<std::string>& columns) {
    data.validate();TmaGridOptions options;options.name=std::move(name);options.rows=data.rows;options.columns=data.columns;
    options.row_labels=rows;options.column_labels=columns;const auto info=TmaCodec::grid(options);
    std::vector<ObjectEdit> edits;edits.reserve(data.cores.size());
    for(std::size_t i=0;i<data.cores.size();++i) {
        const auto& core=data.cores[i];auto edit=ObjectEdit::create_geometry(Roi::ellipse(core.bounds),core.name);
        edit.value.tma_core=TmaCodec::core(info,i/data.columns,i%data.columns,core.missing,core.case_id);edits.push_back(std::move(edit));
    }
    return edits;
}
std::vector<std::uint64_t> import_qptma(Document& doc,const QptmaData& data,std::string name,const std::vector<std::string>& rows,const std::vector<std::string>& columns) {
    if(doc.tma_core_count())throw std::invalid_argument("document already has a TMA grid; clear it explicitly before import");
    return doc.execute(qptma_grid_edits(data,std::move(name),rows,columns),"Import QuPath TMA grid");
}
QptmaExport qptma_from_grid(const TmaGrid& grid,std::string hint,std::string name) {
    field(hint,32768);field(name,32768);QptmaExport out;out.data.source_hint=std::move(hint);out.data.image_name=std::move(name);
    out.data.rows=grid.info()->rows();out.data.columns=grid.info()->columns();const auto count=out.data.rows*out.data.columns;
    std::vector<const Annotation*> slots(count,nullptr);bool classes=false,parents=false;
    for(const auto& a:grid.cores()) {
        if(!a.tma_core || a.tma_core->row()>=out.data.rows || a.tma_core->column()>=out.data.columns)throw std::invalid_argument("invalid TMA snapshot for qptma export");
        const auto i=a.tma_core->row()*out.data.columns+a.tma_core->column();if(slots[i])throw std::invalid_argument("duplicate TMA snapshot slot");
        slots[i]=&a;classes=classes || !a.class_name.empty();parents=parents || a.parent_id!=0;
    }
    out.data.cores.reserve(count);
    for(const auto a:slots) {
        if(!a)throw std::invalid_argument("qptma cannot represent a vacant TMA slot; restore a missing core before export");
        out.data.cores.push_back({a->name,a->tma_core->case_id(),a->bounds(),a->tma_core->missing()});
    }
    out.omitted_metadata={"native_object_ids","native_grid_name","axis_labels"};
    if(classes)out.omitted_metadata.push_back("class_assignments");if(parents)out.omitted_metadata.push_back("parent_links");
    out.data.validate();return out;
}
QptmaData load_qptma(const std::string& path,std::size_t limit,const std::atomic<bool>* cancel) {
    detail::export_checkpoint(cancel);quota(limit);detail::bounded_utf8(path,32768);
    detail::ReaderFileGuard observed(detail::export_path(path));if(observed.version()->bytes>limit)throw std::length_error("qptma input exceeds byte limit");
    std::ifstream stream(std::filesystem::u8path(path),std::ios::binary);if(!stream)throw std::runtime_error("cannot open qptma input");
    std::string text;text.reserve(static_cast<std::size_t>(observed.version()->bytes));std::array<char,65536> chunk;
    while(stream) {detail::export_checkpoint(cancel);stream.read(chunk.data(),chunk.size());const auto n=static_cast<std::size_t>(stream.gcount());
        if(n>limit-text.size())throw std::length_error("qptma input exceeds byte limit");text.append(chunk.data(),n);}
    if(!stream.eof() || stream.bad())throw std::runtime_error("cannot read qptma input");observed.verify();
    return parse_qptma(text,limit,cancel);
}
std::size_t save_qptma(const QptmaData& data,const std::string& path,std::size_t limit,const std::atomic<bool>* cancel) {
    detail::export_checkpoint(cancel);const auto text=qptma_text(data,limit,cancel);detail::bounded_utf8(path,32768);
    detail::AtomicExportFile output(path,limit,cancel);output.write(text.data(),text.size());output.commit(false);return text.size();
}
}
