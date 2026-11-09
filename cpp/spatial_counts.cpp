#include "qupathxx/spatial_counts.hpp"
#include "qupathxx/spatial_import.hpp"
#include "qupathxx/object_plane.hpp"
#include "qupathxx/workflow.hpp"
#include "sha256.hpp"
#include "spatial_count_text.hpp"
#include "utf8_text.hpp"
#include <algorithm>
#include <charconv>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <set>
#include <string_view>
#include <unordered_set>

namespace qpx {
namespace {
constexpr std::uint64_t exact_integer=9007199254740992ULL;
struct Budget {
    std::size_t used=0,limit;
    void charge(std::size_t bytes) {workflow_capture_charge(used,bytes,limit);}
};
std::string identifier(std::string_view text) {return detail::spatial_identifier(text);}
std::uint64_t integer(std::string_view text) {return detail::spatial_unsigned(text);}
double coordinate(std::string_view text) {
    double value=0;
    const auto p=std::from_chars(text.data(),text.data()+text.size(),value);
    if(text.empty() || p.ec!=std::errc{} || p.ptr!=text.data()+text.size() || !std::isfinite(value) || value<0)
        throw std::invalid_argument("Expected finite nonnegative source coordinate");
    return value;
}
struct Lines {
    std::string_view remaining;
    bool next(std::string_view& line) {
        if(remaining.empty())return false;
        const auto at=remaining.find('\n');line=remaining.substr(0,at);
        remaining=at==std::string_view::npos?std::string_view{}:remaining.substr(at+1);
        if(!line.empty() && line.back()=='\r')line.remove_suffix(1);
        if(line.size()>4096 || line.find('\0')!=std::string_view::npos)
            throw std::invalid_argument("Spatial-count line exceeds limits or contains NUL");
        return true;
    }
    bool matrix_line(std::string_view& line) {
        while(next(line)) {
            const auto at=line.find_first_not_of(" \t");
            if(at!=std::string_view::npos && line[at]!='%')return true;
        }
        return false;
    }
};
std::vector<std::string_view> words(std::string_view line,bool tabs=false) {
    std::vector<std::string_view> values;
    while(true) {
        if(!tabs) {
            const auto start=line.find_first_not_of(" \t");
            if(start==std::string_view::npos)break;
            line.remove_prefix(start);
        }
        const auto end=tabs?line.find('\t'):line.find_first_of(" \t");
        values.push_back(line.substr(0,end));
        if(values.size()>3)throw std::invalid_argument("Extra spatial-count fields");
        if(end==std::string_view::npos)break;
        line.remove_prefix(end+1);
    }
    return values;
}
std::string snapshot(const std::string& path,const SpatialCountsOptions& options,
    Budget& budget,std::string& hash,const std::atomic<bool>* cancelled) {
    workflow_path_argument(path);workflow_checkpoint(cancelled);
    const auto file=std::filesystem::u8path(path);
    if(!std::filesystem::is_regular_file(file))throw std::invalid_argument("Spatial-count input must be a regular file");
    const auto size=std::filesystem::file_size(file);
    if(size>options.file_bytes)throw WorkflowCaptureLimit("Spatial-count file byte limit exceeded");
    budget.charge(static_cast<std::size_t>(size));
    std::ifstream input(file,std::ios::binary);
    if(!input)throw std::runtime_error("Cannot open spatial-count input");
    std::string text(static_cast<std::size_t>(size),'\0');Sha256 digest;
    for(std::size_t at=0;at<text.size();) {
        workflow_checkpoint(cancelled);const auto n=std::min<std::size_t>(65536,text.size()-at);
        input.read(text.data()+at,static_cast<std::streamsize>(n));
        if(!input)throw WorkflowInputChanged("Spatial-count input truncated during capture");
        digest.update(text.data()+at,n);at+=n;
    }
    if(input.peek()!=std::char_traits<char>::eof())throw WorkflowInputChanged("Spatial-count input grew during capture");
    hash=digest.finish();return text;
}
std::vector<std::string> feature_table(const std::string& text,std::uint32_t maximum,Budget& budget,const std::atomic<bool>* cancelled) {
    Lines lines{text};std::string_view line;
    if(!lines.next(line) || line!="feature_id")throw std::invalid_argument("Expected feature_id TSV header");
    std::vector<std::string> result;std::unordered_set<std::string> unique;
    while(lines.next(line)) {
        workflow_checkpoint(cancelled);if(result.size()==maximum)throw WorkflowCaptureLimit("Feature catalogue exceeds limit");
        budget.charge(512+line.size());auto id=identifier(line);
        if(!unique.insert(id).second)throw std::invalid_argument("Duplicate feature ID");result.push_back(std::move(id));
    }
    if(result.empty())throw std::invalid_argument("Empty feature table");return result;
}
}
SpatialFeatureCatalogue read_spatial_feature_catalogue(const std::string& path,std::size_t file,std::size_t work,std::uint32_t maximum,const std::atomic<bool>* cancelled) {
    workflow_checkpoint(cancelled);
    if(!file || file>1024ULL*1024*1024 || !work || work>1024ULL*1024*1024 || !maximum || maximum>1000000)
        throw std::invalid_argument("Invalid feature catalogue limits");
    SpatialCountsOptions options;options.file_bytes=file;Budget budget{0,work};SpatialFeatureCatalogue result;
    const auto text=snapshot(path,options,budget,result.source_sha256,cancelled);result.features=feature_table(text,maximum,budget,cancelled);return result;
}
void SpatialCountsOptions::validate() const {
    if(features.empty() || features.size()>64)throw std::invalid_argument("Select between1 and64 feature IDs");
    std::set<std::string> unique;
    for(const auto& feature:features)if(!unique.insert(identifier(feature)).second)
        throw std::invalid_argument("Duplicate selected feature ID");
    unique.clear();for(const auto& column:position_columns)if(!unique.insert(identifier(column)).second)
        throw std::invalid_argument("Coordinate column mappings must be distinct");
    if(!file_bytes || file_bytes>1024ULL*1024*1024 || !work_bytes || work_bytes>1024ULL*1024*1024 ||
       !max_features || max_features>1000000 || !max_observations || max_observations>100000 ||
       !max_entries || max_entries>5000000)
        throw std::invalid_argument("Invalid spatial-count resource limits");
    ObjectPlane(-1,z,t).validate();
}
SpatialCountsProjection read_spatial_counts(const std::string& matrix_path,
    const std::string& feature_path,const std::string& position_path,
    const SpatialCountsOptions& options,const std::atomic<bool>* cancelled) {
    options.validate();workflow_checkpoint(cancelled);Budget budget{0,options.work_bytes};
    SpatialCountsProjection result;result.features=options.features;
    const auto matrix=snapshot(matrix_path,options,budget,result.source_sha256[0],cancelled);
    Lines entries{matrix};std::string_view line;
    if(!entries.next(line) || line!="%%MatrixMarket matrix coordinate integer general")
        throw std::invalid_argument("Only Matrix Market coordinate integer general is supported");
    if(!entries.matrix_line(line))throw std::invalid_argument("Missing matrix dimensions");
    const auto dimensions=words(line);
    if(dimensions.size()!=3)throw std::invalid_argument("Expected matrix rows, columns and entry count");
    const auto rows=integer(dimensions[0]),columns=integer(dimensions[1]),nnz=integer(dimensions[2]);
    if(!rows || rows>options.max_features || !columns || columns>options.max_observations || nnz>options.max_entries)
        throw WorkflowCaptureLimit("Spatial-count matrix dimensions/entries exceed limits");
    result.matrix_features=static_cast<std::uint32_t>(rows);result.matrix_entries=nnz;
    budget.charge(static_cast<std::size_t>(rows)*sizeof(std::int32_t));
    std::vector<std::int32_t> selected(static_cast<std::size_t>(rows),-1);
    const auto feature_text=snapshot(feature_path,options,budget,result.source_sha256[1],cancelled);
    const auto all_features=feature_table(feature_text,options.max_features,budget,cancelled);
    if(all_features.size()!=rows)throw std::invalid_argument("Feature count differs from matrix rows");
    std::size_t found=0;
    for(std::size_t row=0;row<rows;++row) {
        workflow_checkpoint(cancelled);const auto match=std::find(options.features.begin(),options.features.end(),all_features[row]);
        if(match!=options.features.end()) {selected[row]=static_cast<std::int32_t>(match-options.features.begin());++found;}
    }
    if(found!=options.features.size())throw std::invalid_argument("Selected feature ID absent");
    std::unordered_set<std::string> ids;
    const auto position_text=snapshot(position_path,options,budget,result.source_sha256[2],cancelled);
    Lines positions{position_text};
    if(!positions.next(line))throw std::invalid_argument("Missing position TSV header");
    const auto headers=words(line,true);if(headers.size()!=3)throw std::invalid_argument("Position TSV requires three explicitly mapped columns");
    std::array<std::size_t,3> mapping{};
    for(std::size_t i=0;i<3;++i) {
        const auto at=std::find(headers.begin(),headers.end(),options.position_columns[i]);
        if(at==headers.end() || std::count(headers.begin(),headers.end(),options.position_columns[i])!=1)throw std::invalid_argument("Position column missing or ambiguous");
        mapping[i]=static_cast<std::size_t>(at-headers.begin());
    }
    budget.charge(static_cast<std::size_t>(columns)*sizeof(SpatialObservation));
    result.observations.reserve(static_cast<std::size_t>(columns));
    for(std::size_t column=0;column<columns;++column) {
        workflow_checkpoint(cancelled);
        if(!positions.next(line))throw std::invalid_argument("Truncated position table");
        const auto fields=words(line,true);
        if(fields.size()!=3)throw std::invalid_argument("Expected observation ID and two pixel coordinates");
        budget.charge(512+fields[mapping[0]].size());auto id=identifier(fields[mapping[0]]);
        if(!ids.insert(id).second)throw std::invalid_argument("Duplicate observation ID");
        result.observations.push_back({std::move(id),{coordinate(fields[mapping[1]]),coordinate(fields[mapping[2]])}});
    }
    if(positions.next(line))throw std::invalid_argument("Extra position records");
    const auto count=static_cast<std::size_t>(columns)*result.features.size();
    budget.charge(count*sizeof(std::uint64_t));result.counts.resize(count,0);
    // Includes conservative node/bucket storage for duplicate detection; never
    // allocate a full feature-by-observation matrix.
    budget.charge(static_cast<std::size_t>(nnz)*128);
    std::unordered_set<std::uint64_t> seen;
    for(std::uint64_t n=0;n<nnz;++n) {
        workflow_checkpoint(cancelled);
        if(!entries.matrix_line(line))throw std::invalid_argument("Truncated matrix entries");
        const auto fields=words(line);
        if(fields.size()!=3)throw std::invalid_argument("Expected matrix row, column and count");
        const auto row=integer(fields[0]),column=integer(fields[1]),value=integer(fields[2]);
        if(!row || row>rows || !column || column>columns)throw std::invalid_argument("Matrix entry index outside dimensions");
        if(!seen.insert((row-1)*columns+column-1).second)throw std::invalid_argument("Duplicate sparse matrix coordinate");
        const auto feature=selected[static_cast<std::size_t>(row-1)];
        if(feature>=0) {
            if(value>exact_integer)throw std::invalid_argument("Selected count exceeds exact project integer representation");
            result.counts[static_cast<std::size_t>(column-1)*result.features.size()+feature]=value;
        }
    }
    if(entries.matrix_line(line))throw std::invalid_argument("Extra matrix entries");
    workflow_checkpoint(cancelled);return result;
}
std::string import_spatial_counts(const std::string& input_project,const std::string& output_project,
    std::uint64_t image_id,const std::string& matrix,const std::string& features,const std::string& positions,
    const SpatialCountsOptions& options,const std::atomic<bool>* cancelled) {
    SpatialImportRequest request;request.input_project=input_project;request.output_project=output_project;request.image_id=image_id;
    request.matrix=matrix;request.features=features;request.positions=positions;request.options=options;
    return publish_spatial_import(*preview_spatial_import(request,cancelled),cancelled);
}
}
