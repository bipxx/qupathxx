#include "webp_memory_backend.h"
#include <string.h>
#include <webp/decode.h>

typedef struct Context Context;
typedef union Block Block;
union Block {
    struct {size_t charged;Block* previous;Block* next;Context* owner;} info;
    long double alignment;
    unsigned char padding[32];
};
struct Context {
    uint64_t limit,used;
    Block* blocks;
    qpx_reader_cancel_v1 cancel;
    qpx_reader_status_v1 status;
};
#if defined(_MSC_VER)
static __declspec(thread) Context* active_context;
#else
static _Thread_local Context* active_context;
#endif

static int stopped(Context* context) {
    if(context->cancel.is_cancelled && context->cancel.is_cancelled(context->cancel.context))
        context->status=QPX_READER_CANCELLED;
    return context->status!=QPX_READER_OK;
}
void* qpx_webp_malloc(size_t bytes) {
    const size_t bookkeeping=32;
    Context* context=active_context;
    Block* block;
    size_t charged;
    if(!context || stopped(context))return NULL;
    if(bytes>SIZE_MAX-sizeof(Block)-bookkeeping) {
        context->status=QPX_READER_INVALID_REQUEST;return NULL;
    }
    /* Charge the exact codec payload/header plus a conservative CRT-block
     * bookkeeping allowance. This is not a claim to bound process RSS. */
    charged=bytes+sizeof(Block)+bookkeeping;
    if(context->used>context->limit || charged>context->limit-context->used) {
        context->status=QPX_READER_INVALID_REQUEST;return NULL;
    }
    block=(Block*)malloc(charged-bookkeeping);
    if(!block) {context->status=QPX_READER_INTERNAL_ERROR;return NULL;}
    block->info.charged=charged;block->info.owner=context;
    block->info.previous=NULL;block->info.next=context->blocks;
    if(context->blocks)context->blocks->info.previous=block;
    context->blocks=block;context->used+=charged;
    return block+1;
}
void* qpx_webp_calloc(size_t count,size_t bytes) {
    void* pointer;
    if(count && bytes>SIZE_MAX/count) {
        if(active_context)active_context->status=QPX_READER_INVALID_REQUEST;
        return NULL;
    }
    bytes*=count;pointer=qpx_webp_malloc(bytes);
    if(pointer)memset(pointer,0,bytes);
    return pointer;
}
void qpx_webp_free(void* pointer) {
    Block* block;
    Context* context;
    if(!pointer)return;
    block=(Block*)pointer-1;context=block->info.owner;
    if(block->info.previous)block->info.previous->info.next=block->info.next;
    else context->blocks=block->info.next;
    if(block->info.next)block->info.next->info.previous=block->info.previous;
    context->used-=block->info.charged;free(block);
}
qpx_reader_status_v1 qpx_webp_decode_rgba(const uint8_t* data,size_t size,
    uint8_t* rgba,size_t capacity,int width,int height,uint64_t codec_work_bytes,
    qpx_reader_cancel_v1 cancel) {
    Context context;
    WebPDecBuffer output;
    WebPIDecoder* decoder=NULL;
    VP8StatusCode status=VP8_STATUS_SUSPENDED;
    size_t available=0;
    int initialized=0;
    const uint64_t overhead=sizeof(Context)+sizeof(WebPDecBuffer);
    if(active_context)return QPX_READER_INTERNAL_ERROR;
    if(!data || !size || !rgba || width<=0 || width>16384 || height<=0 || height>16384 ||
        (uint64_t)width*height*4>capacity)return QPX_READER_INVALID_REQUEST;
    if(codec_work_bytes<=overhead)return QPX_READER_INVALID_REQUEST;
    memset(&context,0,sizeof(context));
    context.limit=codec_work_bytes-overhead;context.cancel=cancel;
    active_context=&context;
    if(stopped(&context))goto End;
    if(!WebPInitDecBuffer(&output)) {context.status=QPX_READER_INTERNAL_ERROR;goto End;}
    initialized=1;
    output.colorspace=MODE_RGBA;output.width=width;output.height=height;
    output.is_external_memory=1;output.u.RGBA.rgba=rgba;
    output.u.RGBA.stride=width*4;output.u.RGBA.size=capacity;
    decoder=WebPINewDecoder(&output);
    if(!decoder) {
        if(context.status==QPX_READER_OK)context.status=QPX_READER_INTERNAL_ERROR;
        goto End;
    }
    /* Stable mapped input avoids an unneeded compressed-input copy. Poll at
     * every public call and allocation. A single codec call can still decode
     * an entire compact image; this is not a fixed-latency/row-cancel promise. */
    while(available<size && status==VP8_STATUS_SUSPENDED && !stopped(&context)) {
        const size_t remaining=size-available;
        available+=remaining<4096?remaining:4096;
        status=WebPIUpdate(decoder,data,available);
        if(stopped(&context))break;
    }
    if(context.status==QPX_READER_OK) {
        if(status==VP8_STATUS_OK) {
            int last_y=0,decoded_width=0,decoded_height=0,stride=0;
            uint8_t* decoded=WebPIDecGetRGB(decoder,&last_y,&decoded_width,&decoded_height,&stride);
            if(decoded!=rgba || decoded_width!=width || decoded_height!=height ||
                last_y!=height || stride!=width*4)context.status=QPX_READER_INTERNAL_ERROR;
        } else if(status==VP8_STATUS_OUT_OF_MEMORY)
            /* Real CRT failures were recorded by our allocator. Unflagged
             * allocation refusal is an upstream arithmetic/size guard. */
            context.status=QPX_READER_INVALID_REQUEST;
        else if(status==VP8_STATUS_INVALID_PARAM)context.status=QPX_READER_INTERNAL_ERROR;
        else context.status=QPX_READER_IO_ERROR;
    }
End:
    WebPIDelete(decoder);
    if(initialized)WebPFreeDecBuffer(&output);
    if(context.blocks && context.status==QPX_READER_OK)context.status=QPX_READER_INTERNAL_ERROR;
    while(context.blocks)qpx_webp_free(context.blocks+1);
    context.cancel.is_cancelled=NULL;context.cancel.context=NULL;
    active_context=NULL;
    return context.status;
}
