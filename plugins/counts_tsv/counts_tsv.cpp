// SPDX-License-Identifier: AGPL-3.0-only
#include "format_plugin_profile.hpp"
#include "../format-common/strict_tsv.hpp"

namespace {
using namespace qpx_tsv;
struct Handle {
    std::unique_ptr<char[]> bytes;
    std::unique_ptr<Span[]> features,observations;
    std::unique_ptr<std::uint64_t[]> values;
    std::uint64_t feature_count,observation_count,value_count;
};
struct Shape {std::uint64_t features,observations;};
Shape scan(const char* bytes,std::uint64_t size,qpx_reader_cancel_v1 cancel,Handle* output=nullptr) {
    Cursor cursor{bytes,size,0,cancel};
    if(!cursor.has())fail(QPX_READER_UNSUPPORTED,"Counts TSV requires observation_id followed by feature IDs");
    auto token=cursor.next();
    if(view(bytes,token.span)!="observation_id")fail(QPX_READER_UNSUPPORTED,"Counts TSV requires the exact observation_id header");
    Shape shape{};
    while(token.end==End::tab) {
        token=cursor.next();label(bytes,token.span,cancel);
        valid(cursor.position<=header_limit,"Counts TSV header exceeds 64 KiB");
        if(output)output->features[shape.features]=token.span;
        ++shape.features;
    }
    valid(shape.features>0,"Counts TSV header requires at least one feature");
    valid(cursor.position<=header_limit,"Counts TSV header exceeds 64 KiB");
    while(cursor.has()) {
        token=cursor.next();label(bytes,token.span,cancel);
        if(output)output->observations[shape.observations]=token.span;
        valid(token.end==End::tab,"Counts TSV row has too few columns");
        for(std::uint64_t feature=0;feature<shape.features;++feature) {
            token=cursor.next();const auto value=integer(view(bytes,token.span),cancel);
            if(output)output->values[shape.observations*shape.features+feature]=value;
            if(feature+1<shape.features)valid(token.end==End::tab,"Counts TSV row has too few columns");
            else valid(token.end!=End::tab,"Counts TSV row has too many columns");
        }
        ++shape.observations;
    }
    valid(shape.observations>0,"Counts TSV requires at least one observation");checkpoint(cancel);return shape;
}
const Handle& handle(void* value) {require(value!=nullptr,"Counts TSV handle is null");return *static_cast<Handle*>(value);}
qpx_reader_status_v1 QPX_FORMAT_CALL describe(qpx_format_info_v1* output,qpx_reader_text_v1* error) noexcept {
    return boundary(error,[&]() ->qpx_reader_status_v1 {
        bool complete=false;descriptor(output,QPX_FORMAT_MATRIX,"org.qupathxx.counts-tsv","counts-tsv","Dense counts TSV",
            "dense-observation-feature-uint64-tsv/1",QPX_FORMAT_PLUGIN_PROFILE_ID,complete);
        return complete?QPX_READER_OK:failure(error,QPX_READER_BUFFER_TOO_SMALL,"Counts TSV descriptor text buffer is too small");
    });
}
qpx_reader_status_v1 QPX_FORMAT_CALL open(const qpx_reader_open_v1* request,void** output,qpx_reader_text_v1* error) noexcept {
    return boundary(error,[&]() ->qpx_reader_status_v1 {
        require(output!=nullptr,"Counts TSV output handle is null");*output=nullptr;
        auto input=source(request,sizeof(Handle));const auto shape=scan(input.bytes.get(),input.size,request->cancel);
        require(shape.features<=UINT64_MAX/shape.observations,"Counts TSV matrix size overflows");
        auto result=std::make_unique<Handle>();result->feature_count=shape.features;result->observation_count=shape.observations;
        result->value_count=shape.features*shape.observations;
        result->features=input.budget.array<Span>(shape.features);result->observations=input.budget.array<Span>(shape.observations);
        result->values=input.budget.array<std::uint64_t>(result->value_count);
        auto index=input.budget.array<std::uint64_t>(std::max(shape.features,shape.observations));
        result->bytes=std::move(input.bytes);scan(result->bytes.get(),input.size,request->cancel,result.get());
        unique_ids(result->bytes.get(),shape.features,index.get(),[&](std::uint64_t i){return result->features[i];},request->cancel);
        unique_ids(result->bytes.get(),shape.observations,index.get(),[&](std::uint64_t i){return result->observations[i];},request->cancel);
        checkpoint(request->cancel);*output=result.release();return QPX_READER_OK;
    });
}
void QPX_FORMAT_CALL destroy(void* value) noexcept {try{delete static_cast<Handle*>(value);}catch(...) {}}
qpx_reader_status_v1 QPX_FORMAT_CALL metadata(void* value,qpx_matrix_metadata_v1* output,qpx_reader_cancel_v1 cancel,qpx_reader_text_v1* error) noexcept {
    return boundary(error,[&]() ->qpx_reader_status_v1 {
        const auto& source=handle(value);require(output && output->struct_size>=sizeof(*output),"Counts TSV metadata structure is too small");checkpoint(cancel);
        output->value_type=QPX_FORMAT_COUNTS_UINT64;output->features=source.feature_count;output->observations=source.observation_count;
        output->reserved[0]=output->reserved[1]=0;return QPX_READER_OK;
    });
}
qpx_reader_status_v1 QPX_FORMAT_CALL axis(void* value,const qpx_matrix_axis_request_v1* request,qpx_matrix_axis_buffer_v1* output,qpx_reader_cancel_v1 cancel,qpx_reader_text_v1* error) noexcept {
    return boundary(error,[&]() ->qpx_reader_status_v1 {
        const auto& source=handle(value);
        require(output && output->struct_size>=sizeof(*output),"Counts TSV axis buffer structure is too small");output->written=0;output->text.size=0;
        require(request && request->struct_size>=sizeof(*request),"Counts TSV axis request structure is too small");
        require(!output->reserved && (request->axis==QPX_FORMAT_FEATURE_AXIS || request->axis==QPX_FORMAT_OBSERVATION_AXIS),"Counts TSV axis or reserved field is invalid");
        require(!output->capacity || output->rows,"Counts TSV axis rows pointer is null");
        require(!output->text.capacity || output->text.data,"Counts TSV axis text pointer is null");
        const auto total=request->axis==QPX_FORMAT_FEATURE_AXIS?source.feature_count:source.observation_count;
        const auto* spans=request->axis==QPX_FORMAT_FEATURE_AXIS?source.features.get():source.observations.get();
        const auto count=page(request->first,request->count,total);std::uint64_t bytes=0;
        for(std::uint64_t i=0;i<count;++i){checkpoint(cancel);bytes+=spans[request->first+i].size;}
        if(output->capacity<count || output->text.capacity<bytes) {
            output->written=count;output->text.size=bytes;
            return failure(error,QPX_READER_BUFFER_TOO_SMALL,"Counts TSV axis buffers are too small");
        }
        std::uint64_t offset=0;
        for(std::uint64_t i=0;i<count;++i) {
            checkpoint(cancel);const auto index=request->first+i;const auto span=spans[index];
            output->rows[i]={index,offset,span.size,offset+span.size,0};
            if(span.size)std::memcpy(output->text.data+offset,source.bytes.get()+span.offset,static_cast<std::size_t>(span.size));offset+=span.size;
        }
        checkpoint(cancel);output->written=count;output->text.size=bytes;return QPX_READER_OK;
    });
}
qpx_reader_status_v1 QPX_FORMAT_CALL values(void* value,const qpx_matrix_values_request_v1* request,qpx_matrix_values_buffer_v1* output,qpx_reader_cancel_v1 cancel,qpx_reader_text_v1* error) noexcept {
    return boundary(error,[&]() ->qpx_reader_status_v1 {
        const auto& source=handle(value);
        require(output && output->struct_size>=sizeof(*output),"Counts TSV values buffer structure is too small");output->written=0;
        require(request && request->struct_size>=sizeof(*request),"Counts TSV values request structure is too small");
        require(!request->reserved && !output->reserved,"Counts TSV values reserved fields must be zero");
        require(!output->capacity || output->values,"Counts TSV values pointer is null");
        const auto count=page(request->first,request->count,source.value_count);checkpoint(cancel);
        if(output->capacity<count) {output->written=count;return failure(error,QPX_READER_BUFFER_TOO_SMALL,"Counts TSV values buffer is too small");}
        for(std::uint64_t i=0;i<count;) {
            checkpoint(cancel);const auto chunk=std::min<std::uint64_t>(512,count-i);
            std::memcpy(output->values+i,source.values.get()+request->first+i,static_cast<std::size_t>(chunk*sizeof(std::uint64_t)));i+=chunk;
        }
        checkpoint(cancel);output->written=count;return QPX_READER_OK;
    });
}
} // namespace

extern "C" QPX_FORMAT_EXPORT qpx_reader_status_v1 QPX_FORMAT_CALL qpx_matrix_format_entry_v1(
    std::uint32_t requested,qpx_matrix_format_api_v1* output,qpx_reader_text_v1* error) {
    return qpx_tsv::boundary(error,[&]() ->qpx_reader_status_v1 {
        qpx_tsv::require(requested==QPX_FORMAT_ABI_V1,"Counts TSV does not support the requested ABI");
        qpx_tsv::require(output && output->struct_size>=sizeof(*output),"Counts TSV API structure is too small");
        *output={sizeof(*output),QPX_FORMAT_ABI_V1,describe,open,destroy,metadata,axis,values};return QPX_READER_OK;
    });
}
