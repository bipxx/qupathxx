// SPDX-License-Identifier: AGPL-3.0-only
#include "format_plugin_profile.hpp"
#include "../format-common/strict_tsv.hpp"
#include <array>

namespace {
using namespace qpx_tsv;
struct Point {Span id,classification;double x,y;std::int32_t c,z,t;};
struct Handle {std::unique_ptr<char[]> bytes;std::unique_ptr<Point[]> points;std::uint64_t count;};
std::uint64_t scan(const char* bytes,std::uint64_t size,qpx_reader_cancel_v1 cancel,Handle* output=nullptr) {
    Cursor cursor{bytes,size,0,cancel};
    constexpr std::array<std::string_view,7> header{"id","x","y","c","z","t","class"};
    for(std::size_t i=0;i<header.size();++i) {
        if(!cursor.has())fail(QPX_READER_UNSUPPORTED,"Point TSV requires the exact id,x,y,c,z,t,class header");
        const auto token=cursor.next();
        if(view(bytes,token.span)!=header[i] || (i+1<header.size()?token.end!=End::tab:token.end==End::tab))
            fail(QPX_READER_UNSUPPORTED,"Point TSV requires the exact id,x,y,c,z,t,class header");
        valid(cursor.position<=header_limit,"Point TSV header exceeds 64 KiB");
    }
    std::uint64_t count=0;
    while(cursor.has()) {
        std::array<Span,7> fields{};
        for(std::size_t i=0;i<fields.size();++i) {
            const auto token=cursor.next();fields[i]=token.span;
            valid(i+1<fields.size()?token.end==End::tab:token.end!=End::tab,"Point TSV row must have exactly seven columns");
        }
        label(bytes,fields[0],cancel);label(bytes,fields[6],cancel,true);
        const Point point{fields[0],fields[6],coordinate(view(bytes,fields[1]),cancel),coordinate(view(bytes,fields[2]),cancel),
            plane(view(bytes,fields[3]),cancel),plane(view(bytes,fields[4]),cancel),plane(view(bytes,fields[5]),cancel)};
        valid(point.z>=0 && point.t>=0,"Point TSV Z and T planes must be explicit nonnegative indices");
        if(output)output->points[count]=point;++count;
    }
    valid(count>0,"Point TSV requires at least one point");checkpoint(cancel);return count;
}
const Handle& handle(void* value) {require(value!=nullptr,"Point TSV handle is null");return *static_cast<Handle*>(value);}
qpx_reader_status_v1 QPX_FORMAT_CALL describe(qpx_format_info_v1* output,qpx_reader_text_v1* error) noexcept {
    return boundary(error,[&]() ->qpx_reader_status_v1 {
        bool complete=false;descriptor(output,QPX_FORMAT_ANNOTATION,"org.qupathxx.point-tsv","point-tsv","Pixel point TSV",
            "pixel-point-annotation-tsv/1",QPX_FORMAT_PLUGIN_PROFILE_ID,complete);
        return complete?QPX_READER_OK:failure(error,QPX_READER_BUFFER_TOO_SMALL,"Point TSV descriptor text buffer is too small");
    });
}
qpx_reader_status_v1 QPX_FORMAT_CALL open(const qpx_reader_open_v1* request,void** output,qpx_reader_text_v1* error) noexcept {
    return boundary(error,[&]() ->qpx_reader_status_v1 {
        require(output!=nullptr,"Point TSV output handle is null");*output=nullptr;
        auto input=source(request,sizeof(Handle));const auto count=scan(input.bytes.get(),input.size,request->cancel);
        auto result=std::make_unique<Handle>();result->count=count;result->points=input.budget.array<Point>(count);
        auto index=input.budget.array<std::uint64_t>(count);result->bytes=std::move(input.bytes);
        scan(result->bytes.get(),input.size,request->cancel,result.get());
        unique_ids(result->bytes.get(),count,index.get(),[&](std::uint64_t i){return result->points[i].id;},request->cancel);
        checkpoint(request->cancel);*output=result.release();return QPX_READER_OK;
    });
}
void QPX_FORMAT_CALL destroy(void* value) noexcept {try{delete static_cast<Handle*>(value);}catch(...) {}}
qpx_reader_status_v1 QPX_FORMAT_CALL metadata(void* value,qpx_annotation_metadata_v1* output,qpx_reader_cancel_v1 cancel,qpx_reader_text_v1* error) noexcept {
    return boundary(error,[&]() ->qpx_reader_status_v1 {
        const auto& source=handle(value);require(output && output->struct_size>=sizeof(*output),"Point TSV metadata structure is too small");checkpoint(cancel);
        output->coordinate_frame=QPX_FORMAT_IMAGE_PIXELS;output->points=source.count;output->reserved[0]=output->reserved[1]=0;return QPX_READER_OK;
    });
}
qpx_reader_status_v1 QPX_FORMAT_CALL points(void* value,const qpx_annotation_points_request_v1* request,qpx_annotation_points_buffer_v1* output,qpx_reader_cancel_v1 cancel,qpx_reader_text_v1* error) noexcept {
    return boundary(error,[&]() ->qpx_reader_status_v1 {
        const auto& source=handle(value);
        require(output && output->struct_size>=sizeof(*output),"Point TSV output structure is too small");output->written=0;output->text.size=0;
        require(request && request->struct_size>=sizeof(*request),"Point TSV request structure is too small");
        require(!request->reserved && !output->reserved,"Point TSV reserved fields must be zero");
        require(!output->capacity || output->points,"Point TSV points pointer is null");
        require(!output->text.capacity || output->text.data,"Point TSV text pointer is null");
        const auto count=page(request->first,request->count,source.count);std::uint64_t bytes=0;
        for(std::uint64_t i=0;i<count;++i) {checkpoint(cancel);const auto& point=source.points[request->first+i];bytes+=point.id.size+point.classification.size;}
        if(output->capacity<count || output->text.capacity<bytes) {
            output->written=count;output->text.size=bytes;
            return failure(error,QPX_READER_BUFFER_TOO_SMALL,"Point TSV output buffers are too small");
        }
        std::uint64_t offset=0;
        for(std::uint64_t i=0;i<count;++i) {
            checkpoint(cancel);const auto& point=source.points[request->first+i];
            output->points[i]={point.x,point.y,point.c,point.z,point.t,0,offset,point.id.size,offset+point.id.size,point.classification.size};
            std::memcpy(output->text.data+offset,source.bytes.get()+point.id.offset,static_cast<std::size_t>(point.id.size));offset+=point.id.size;
            checkpoint(cancel);
            if(point.classification.size)std::memcpy(output->text.data+offset,source.bytes.get()+point.classification.offset,static_cast<std::size_t>(point.classification.size));offset+=point.classification.size;
        }
        checkpoint(cancel);output->written=count;output->text.size=bytes;return QPX_READER_OK;
    });
}
} // namespace

extern "C" QPX_FORMAT_EXPORT qpx_reader_status_v1 QPX_FORMAT_CALL qpx_annotation_format_entry_v1(
    std::uint32_t requested,qpx_annotation_format_api_v1* output,qpx_reader_text_v1* error) {
    return qpx_tsv::boundary(error,[&]() ->qpx_reader_status_v1 {
        qpx_tsv::require(requested==QPX_FORMAT_ABI_V1,"Point TSV does not support the requested ABI");
        qpx_tsv::require(output && output->struct_size>=sizeof(*output),"Point TSV API structure is too small");
        *output={sizeof(*output),QPX_FORMAT_ABI_V1,describe,open,destroy,metadata,points};return QPX_READER_OK;
    });
}
