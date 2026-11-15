#include "reader_plugin_profile.hpp"
#include <qupathxx/reader_plugin.h>
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <limits>
#include <memory>
#include <new>
#include <stdexcept>
#include <string>
#include <string_view>

namespace {
constexpr std::uint64_t work_limit=UINT64_C(256)*1024*1024;
constexpr std::size_t header_limit=65536;
struct Failure {qpx_reader_status_v1 status;const char* message;};
[[noreturn]] void fail(qpx_reader_status_v1 status,const char* message) {throw Failure{status,message};}
void require(bool condition,const char* message) {if(!condition)fail(QPX_READER_INVALID_REQUEST,message);}
void valid(bool condition,const char* message) {if(!condition)fail(QPX_READER_IO_ERROR,message);}
void checkpoint(qpx_reader_cancel_v1 cancel) {
    if(cancel.is_cancelled && cancel.is_cancelled(cancel.context))fail(QPX_READER_CANCELLED,"PNM operation cancelled");
}
bool text(qpx_reader_text_v1& output,const char* value) noexcept {
    const auto length=std::strlen(value);output.size=length;
    if(output.capacity<length || (length && !output.data))return false;
    if(length)std::memcpy(output.data,value,length);
    return true;
}
qpx_reader_status_v1 failure(qpx_reader_text_v1* error,qpx_reader_status_v1 status,const char* message) noexcept {
    if(error && !text(*error,message))return QPX_READER_BUFFER_TOO_SMALL;
    return status;
}
template<class Function>qpx_reader_status_v1 boundary(qpx_reader_text_v1* error,Function&& function) noexcept {
    if(error)error->size=0;
    try{return function();}
    catch(const Failure& e){return failure(error,e.status,e.message);}
    catch(const std::invalid_argument& e){return failure(error,QPX_READER_INVALID_REQUEST,e.what());}
    catch(const std::out_of_range& e){return failure(error,QPX_READER_INVALID_REQUEST,e.what());}
    catch(const std::bad_alloc&){return failure(error,QPX_READER_INTERNAL_ERROR,"PNM plugin allocation failed");}
    catch(const std::filesystem::filesystem_error& e){return failure(error,QPX_READER_IO_ERROR,e.what());}
    catch(const std::runtime_error& e){return failure(error,QPX_READER_IO_ERROR,e.what());}
    catch(const std::exception& e){return failure(error,QPX_READER_INTERNAL_ERROR,e.what());}
    catch(...){return failure(error,QPX_READER_INTERNAL_ERROR,"PNM plugin internal failure");}
}
std::uint64_t multiply(std::uint64_t a,std::uint64_t b) {
    require(!a || b<=std::numeric_limits<std::uint64_t>::max()/a,"PNM decoded size overflow");
    return a*b;
}
struct Budget {
    std::uint64_t remaining;
    std::unique_ptr<std::uint8_t[]> array(std::uint64_t bytes) {
        // Payload sizes are exact new[] counts; reserve a fixed bookkeeping
        // allowance per owned array in addition to the wrapper/context budget.
        constexpr std::uint64_t bookkeeping=32;
        require(bytes<=remaining && bookkeeping<=remaining-bytes,"PNM decoded data exceeds reader work budget");
        remaining-=bytes+bookkeeping;
        return std::unique_ptr<std::uint8_t[]>(new std::uint8_t[static_cast<std::size_t>(bytes)]);
    }
};
enum class Transfer {Bt709,Srgb,Linear};
Transfer transfer(qpx_reader_string_v1 options) {
    require(options.size<=64 && (!options.size || options.data),"PNM options are invalid");
    const std::string_view value(options.size?options.data:"",static_cast<std::size_t>(options.size));
    if(value.empty() || value=="{\"transfer\":\"bt709\"}")return Transfer::Bt709;
    if(value=="{\"transfer\":\"srgb\"}")return Transfer::Srgb;
    if(value=="{\"transfer\":\"linear\"}")return Transfer::Linear;
    fail(QPX_READER_INVALID_REQUEST,"PNM options must use the canonical bt709, srgb or linear transfer object");
}
bool space(unsigned char value) {return value==32 || (value>=9 && value<=13);}
bool digit(unsigned char value) {return value>='0' && value<='9';}
std::string_view trim(std::string_view value) {
    while(!value.empty() && space(static_cast<unsigned char>(value.front())))value.remove_prefix(1);
    while(!value.empty() && space(static_cast<unsigned char>(value.back())))value.remove_suffix(1);
    return value;
}
struct Cursor {
    const std::uint8_t* bytes;
    std::size_t size,position=0;
    qpx_reader_cancel_v1 cancel;
    bool parsing_header=true;
    void tick() {
        if(parsing_header)require(position<header_limit,"PNM header exceeds 64 KiB limit");
        if((position&4095)==0)checkpoint(cancel);
    }
    void skip() {
        while(position<size) {
            tick();
            if(space(bytes[position])){++position;continue;}
            if(bytes[position]!='#')break;
            while(position<size && bytes[position]!='\r' && bytes[position]!='\n'){tick();++position;}
        }
    }
    std::uint64_t number(std::uint64_t limit,qpx_reader_status_v1 overflow_status,const char* message) {
        skip();
        valid(position<size && digit(bytes[position]),"PNM requires an unsigned decimal token");
        std::uint64_t value=0;
        while(position<size && digit(bytes[position])) {
            tick();const unsigned next=bytes[position]-'0';
            if(value>limit/10 || (value==limit/10 && next>limit%10))fail(overflow_status,message);
            value=value*10+next;++position;
        }
        valid(position==size || space(bytes[position]) || bytes[position]=='#',"PNM decimal token has an invalid suffix");
        return value;
    }
    void final_delimiter() {
        // Comments end BEFORE CR/LF. That one CR or LF is the delimiter. Never
        // consume a second whitespace byte, '#' sample, or guessed CRLF pair.
        if(position<size && bytes[position]=='#') {
            while(position<size && bytes[position]!='\r' && bytes[position]!='\n'){tick();++position;}
        }
        valid(position<size && space(bytes[position]),"PNM raster delimiter is missing");
        tick();++position;parsing_header=false;
    }
    std::string_view line() {
        const auto start=position;
        while(position<size && bytes[position]!='\r' && bytes[position]!='\n') {
            require(position<header_limit,"PAM header exceeds 64 KiB limit");tick();++position;
        }
        valid(position<size,"PAM header line is truncated");
        require(position<header_limit,"PAM header exceeds 64 KiB limit");
        const auto end=position;
        if(bytes[position++]=='\r') {
            require(position<header_limit,"PAM header exceeds 64 KiB limit");
            valid(position<size && bytes[position]=='\n',"PAM header requires LF or CRLF lines");++position;
        }
        return {reinterpret_cast<const char*>(bytes+start),end-start};
    }
};
std::uint64_t line_number(std::string_view value,qpx_reader_status_v1 overflow_status) {
    value=trim(value);valid(!value.empty(),"PAM numeric field is empty");
    std::uint64_t result=0;
    for(const unsigned char character:value) {
        valid(digit(character),"PAM numeric field is not an unsigned decimal integer");
        const unsigned next=character-'0';
        if(result>std::numeric_limits<std::uint64_t>::max()/10 ||
            (result==std::numeric_limits<std::uint64_t>::max()/10 && next>std::numeric_limits<std::uint64_t>::max()%10))
            fail(overflow_status,"PAM numeric field overflows its unsigned representation");
        result=result*10+next;
    }
    return result;
}
struct Header {
    std::uint64_t width=0,height=0,channels=0,maxval=0;
    unsigned kind=0;
    bool blackness=false,alpha=false;
};
Header pam(Cursor& cursor) {
    valid(cursor.position==0,"PAM parser did not start at the magic number");
    if(trim(cursor.line())!="P7")fail(QPX_READER_UNSUPPORTED,"P7 thumbnail or other non-PAM interpretation is unsupported");
    Header header;header.kind=7;
    unsigned seen=0;
    std::array<char,64> tuple{};
    std::size_t tuple_size=0;
    bool tuple_seen=false,tuple_overflow=false;
    for(;;) {
        auto line=trim(cursor.line());
        if(line.empty() || line.front()=='#')continue;
        const auto split=line.find_first_of(" \t\r\n\v\f");
        const auto key=line.substr(0,split);
        const auto value=split==std::string_view::npos?std::string_view{}:trim(line.substr(split));
        if(key=="ENDHDR") {valid(value.empty(),"PAM ENDHDR has an unexpected value");break;}
        if(key=="TUPLTYPE") {
            valid(!value.empty(),"PAM TUPLTYPE is empty");
            auto append=[&](char character) {
                if(tuple_size<tuple.size())tuple[tuple_size++]=character;
                else tuple_overflow=true;
            };
            if(tuple_seen)append(' ');
            tuple_seen=true;
            for(const char character:value)append(character);
            continue;
        }
        unsigned field=0;
        std::uint64_t* destination=nullptr;
        qpx_reader_status_v1 overflow_status=QPX_READER_IO_ERROR;
        if(key=="WIDTH"){field=1;destination=&header.width;overflow_status=QPX_READER_INVALID_REQUEST;}
        else if(key=="HEIGHT"){field=2;destination=&header.height;overflow_status=QPX_READER_INVALID_REQUEST;}
        else if(key=="DEPTH"){field=4;destination=&header.channels;}
        else if(key=="MAXVAL"){field=8;destination=&header.maxval;}
        else fail(QPX_READER_UNSUPPORTED,"PAM header interpretation is unsupported");
        valid(!(seen&field),"PAM mandatory field is duplicated");seen|=field;
        *destination=line_number(value,overflow_status);
    }
    valid(seen==15 && header.width && header.height && header.channels && header.maxval && header.maxval<=65535,
        "PAM mandatory fields are missing or outside their valid range");
    if(!tuple_seen || tuple_overflow)fail(QPX_READER_UNSUPPORTED,"PAM tuple meaning is missing or unsupported");
    const std::string_view name(tuple.data(),tuple_size);
    std::uint64_t channels=0;
    bool black_and_white=false;
    if(name=="BLACKANDWHITE"){channels=1;black_and_white=true;}
    else if(name=="BLACKANDWHITE_ALPHA"){channels=2;black_and_white=true;header.alpha=true;}
    else if(name=="GRAYSCALE")channels=1;
    else if(name=="GRAYSCALE_ALPHA"){channels=2;header.alpha=true;}
    else if(name=="RGB")channels=3;
    else if(name=="RGB_ALPHA"){channels=4;header.alpha=true;}
    else fail(QPX_READER_UNSUPPORTED,"PAM tuple meaning is unsupported");
    valid(header.channels==channels && (!black_and_white || header.maxval==1),"PAM tuple depth or maxval is invalid");
    return header;
}
Header header(Cursor& cursor) {
    if(cursor.size<2 || cursor.bytes[0]!='P' || cursor.bytes[1]<'1' || cursor.bytes[1]>'7')
        fail(QPX_READER_UNSUPPORTED,"PNM signature is not supported");
    if(cursor.bytes[1]=='7') {
        auto result=pam(cursor);cursor.parsing_header=false;return result;
    }
    Header result;result.kind=cursor.bytes[1]-'0';
    cursor.position=2;
    valid(cursor.position<cursor.size && (space(cursor.bytes[cursor.position]) || cursor.bytes[cursor.position]=='#'),"PNM magic number delimiter is missing");
    result.width=cursor.number(std::numeric_limits<std::uint64_t>::max(),QPX_READER_INVALID_REQUEST,"PNM width overflows its unsigned representation");
    result.height=cursor.number(std::numeric_limits<std::uint64_t>::max(),QPX_READER_INVALID_REQUEST,"PNM height overflows its unsigned representation");
    result.blackness=result.kind==1 || result.kind==4;
    result.channels=result.kind==3 || result.kind==6?3:1;
    result.maxval=result.blackness?1:cursor.number(65535,QPX_READER_IO_ERROR,"PNM maxval is outside 1 through 65535");
    valid(result.width && result.height && result.maxval,"PNM dimensions and maxval must be positive");
    // The first separator after the final header number ends the header in
    // plain formats too. Subsequent plain-raster whitespace/comments are data
    // separators, so their absolute offsets do not inherit the header limit.
    cursor.final_delimiter();
    return result;
}
struct Raster {
    std::int64_t width=0,height=0;
    std::uint32_t channels=0,sample_bytes=0;
    bool blackness=false,alpha=false;
    std::unique_ptr<std::uint8_t[]> raw,rgba;
};
struct Handle {Raster image;};
std::uint8_t linear_code(std::uint16_t sample,std::uint32_t maxval) {
    // Exact rational round-half-up, shared by alpha and explicit sRGB codes.
    return static_cast<std::uint8_t>((UINT64_C(510)*sample+maxval)/(2*maxval));
}
std::uint8_t display_code(std::uint16_t sample,std::uint32_t maxval,Transfer selected) {
    if(selected==Transfer::Srgb)return linear_code(sample,maxval);
    const double encoded=static_cast<double>(sample)/maxval;
    const double light=selected==Transfer::Linear?encoded:encoded<0.081?encoded/4.5:std::pow((encoded+0.099)/1.099,1.0/0.45);
    const double value=light<=0.0031308?12.92*light:1.055*std::pow(light,1.0/2.4)-0.055;
    if(value<=0)return 0;
    if(value>=1)return 255;
    return static_cast<std::uint8_t>(std::floor(value*255+0.5));
}
Raster decode(const std::uint8_t* input,std::size_t input_size,Budget& budget,Transfer selected,qpx_reader_cancel_v1 cancel) {
    Cursor cursor{input,input_size,0,cancel};
    checkpoint(cancel);
    const auto description=header(cursor);
    require(description.width<=static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max()) &&
        description.height<=static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max()),"PNM dimension exceeds the native coordinate range");
    const auto pixels=multiply(description.width,description.height);
    const auto samples=multiply(pixels,description.channels);
    const auto sample_bytes=description.maxval<=255?1u:2u;
    const auto raw_bytes=multiply(samples,sample_bytes),rgba_bytes=multiply(pixels,4);
    const auto raster=cursor.position;
    std::uint64_t pbm_row_bytes=0;
    if(description.kind>=4) {
        const auto expected=description.kind==4?
            multiply(pbm_row_bytes=(description.width+7)/8,description.height):raw_bytes;
        valid(expected==cursor.size-cursor.position,"PNM binary raster is truncated or has trailing image data");
    } else valid(samples<=cursor.size-cursor.position,"PNM plain raster is truncated");
    Raster result;
    result.width=static_cast<std::int64_t>(description.width);result.height=static_cast<std::int64_t>(description.height);
    result.channels=static_cast<std::uint32_t>(description.channels);result.sample_bytes=sample_bytes;
    result.blackness=description.blackness;result.alpha=description.alpha;
    result.raw=budget.array(raw_bytes);result.rgba=budget.array(rgba_bytes);
    auto lut=budget.array(description.maxval+1);
    for(std::uint32_t value=0;value<=description.maxval;++value) {
        if((value&4095)==0)checkpoint(cancel);
        lut[value]=display_code(static_cast<std::uint16_t>(value),static_cast<std::uint32_t>(description.maxval),selected);
    }
    std::uint64_t binary_position=raster;
    for(std::uint64_t y=0;y<description.height;++y) {
        checkpoint(cancel);
        for(std::uint64_t x=0;x<description.width;++x) {
            const auto pixel=y*description.width+x;
            if((pixel&4095)==0)checkpoint(cancel);
            std::array<std::uint16_t,4> values{};
            for(std::uint64_t channel=0;channel<description.channels;++channel) {
                if(((pixel*description.channels+channel)&4095)==0)checkpoint(cancel);
                std::uint64_t value=0;
                if(description.kind==1) {
                    cursor.skip();valid(cursor.position<cursor.size && (input[cursor.position]=='0' || input[cursor.position]=='1'),"PNM plain bitmap sample is invalid");
                    value=input[cursor.position++]-'0';
                } else if(description.kind<=3) {
                    value=cursor.number(description.maxval,QPX_READER_IO_ERROR,"PNM sample exceeds maxval");
                } else if(description.kind==4) {
                    value=(input[raster+y*pbm_row_bytes+x/8]>>(7-x%8))&1;
                } else if(sample_bytes==1) value=input[binary_position++];
                else {value=(static_cast<std::uint16_t>(input[binary_position])<<8)|input[binary_position+1];binary_position+=2;}
                valid(value<=description.maxval,"PNM sample exceeds maxval");
                values[channel]=static_cast<std::uint16_t>(value);
                auto* destination=result.raw.get()+(pixel*description.channels+channel)*sample_bytes;
                if(sample_bytes==1)*destination=static_cast<std::uint8_t>(value);
                else std::memcpy(destination,&values[channel],2);
            }
            auto* rgba=result.rgba.get()+pixel*4;
            if(description.channels<=2) {
                const auto color=description.blackness?description.maxval-values[0]:values[0];
                rgba[0]=rgba[1]=rgba[2]=lut[color];
            } else for(unsigned channel=0;channel<3;++channel)rgba[channel]=lut[values[channel]];
            rgba[3]=description.alpha?linear_code(values[description.channels-1],static_cast<std::uint32_t>(description.maxval)):255;
        }
    }
    if(description.kind<=3) {cursor.skip();valid(cursor.position==cursor.size,"PNM plain raster has trailing image data");}
    checkpoint(cancel);
    return result;
}
const Raster& image(void* value) {require(value!=nullptr,"PNM reader handle is null");return static_cast<Handle*>(value)->image;}
std::string path_string(qpx_reader_string_v1 value) {
    require(value.data && value.size && value.size<=32768,"PNM image path is invalid");
    std::string path(value.data,static_cast<std::size_t>(value.size));
    require(path.find('\0')==std::string::npos,"PNM image path contains NUL");
    require(std::filesystem::u8path(path).is_absolute(),"PNM image path must be absolute");
    return path;
}
qpx_reader_status_v1 QPX_READER_CALL describe(qpx_reader_info_v1* output,qpx_reader_text_v1* error) noexcept {
    return boundary(error,[&]() ->qpx_reader_status_v1 {
        require(output && output->struct_size>=sizeof(*output),"PNM reader info structure is too small");
        output->abi_version=QPX_READER_ABI_V1;output->capabilities=QPX_READER_CAP_RGBA|QPX_READER_CAP_RAW;
        bool complete=text(output->provider_id,"org.qupathxx.pnm");
        complete=text(output->display_name,"PNM") && complete;
        complete=text(output->version,"1.0.0") && complete;
        complete=text(output->contract,"pnm-pam-uint-samples/1") && complete;
        return complete?QPX_READER_OK:failure(error,QPX_READER_BUFFER_TOO_SMALL,"PNM descriptor text buffer is too small");
    });
}
qpx_reader_status_v1 QPX_READER_CALL open(const qpx_reader_open_v1* request,void** output,qpx_reader_text_v1* error) noexcept {
    return boundary(error,[&]() ->qpx_reader_status_v1 {
        require(output!=nullptr,"PNM reader output handle is null");*output=nullptr;
        require(request && request->struct_size>=sizeof(*request),"PNM open structure is too small");
        require(request->reserved==0,"PNM open reserved field must be zero");
        const auto selected=transfer(request->options);
        require(request->work_bytes && request->work_bytes<=work_limit,"PNM work budget must be between 1 and 256 MiB");
        require(request->path.data && request->path.size && request->path.size<=32768,"PNM image path is invalid");
        // Reserve the path/wrapper allowance before constructing owned paths.
        const auto overhead=UINT64_C(4096)+8*request->path.size+sizeof(Handle);
        require(request->work_bytes>overhead,"PNM reader work budget is too small for opening overhead");
        checkpoint(request->cancel);
        const auto path=path_string(request->path);const auto native=std::filesystem::u8path(path);
        const auto encoded=std::filesystem::file_size(native);
        require(encoded<=request->work_bytes,"PNM encoded input exceeds reader work budget");
        // Bounded UTF-8/UTF-16 path copies, owner and unbuffered FILE state;
        // input/output/LUT array payloads plus bookkeeping are charged below.
        Budget budget{request->work_bytes-overhead};
        auto bytes=budget.array(encoded);
        std::unique_ptr<std::FILE,decltype(&std::fclose)> file(_wfopen(native.c_str(),L"rb"),std::fclose);
        if(!file)fail(QPX_READER_IO_ERROR,"PNM image cannot be opened");
        if(std::setvbuf(file.get(),nullptr,_IONBF,0))fail(QPX_READER_IO_ERROR,"PNM unbuffered input could not be configured");
        for(std::uint64_t position=0;position<encoded;) {
            checkpoint(request->cancel);
            const auto count=static_cast<std::size_t>(std::min<std::uint64_t>(65536,encoded-position));
            valid(std::fread(bytes.get()+position,1,count,file.get())==count,"PNM input is truncated or changed");position+=count;
        }
        valid(std::fgetc(file.get())==EOF && !std::ferror(file.get()),"PNM input changed during reading");
        auto result=std::make_unique<Handle>();
        result->image=decode(bytes.get(),static_cast<std::size_t>(encoded),budget,selected,request->cancel);
        *output=result.release();return QPX_READER_OK;
    });
}
void QPX_READER_CALL destroy(void* value) noexcept {try{delete static_cast<Handle*>(value);}catch(...) {}}
qpx_reader_status_v1 QPX_READER_CALL metadata(void* value,qpx_reader_metadata_v1* output,qpx_reader_cancel_v1 cancel,qpx_reader_text_v1* error) noexcept {
    return boundary(error,[&]() ->qpx_reader_status_v1 {
        const auto& source=image(value);require(output && output->struct_size>=sizeof(*output),"PNM metadata structure is too small");checkpoint(cancel);
        output->level_count=1;output->width=source.width;output->height=source.height;
        output->has_raw=1;output->channels=source.channels;output->sample_type=source.sample_bytes==2?QPX_READER_UINT16:QPX_READER_UINT8;
        output->z_planes=1;output->timepoints=1;output->series=0;output->series_count=1;
        output->calibration_flags=0;output->pixel_width_um=0;output->pixel_height_um=0;output->z_spacing_um=0;output->time_interval_seconds=0;
        bool complete=text(output->vendor,"PNM");complete=text(output->dimension_order,"XYCZT") && complete;
        return complete?QPX_READER_OK:failure(error,QPX_READER_BUFFER_TOO_SMALL,"PNM metadata text buffer is too small");
    });
}
qpx_reader_status_v1 QPX_READER_CALL level(void* value,std::uint32_t index,qpx_reader_level_v1* output,qpx_reader_text_v1* error) noexcept {
    return boundary(error,[&]() ->qpx_reader_status_v1 {
        const auto& source=image(value);require(output && output->struct_size>=sizeof(*output),"PNM level structure is too small");require(index==0,"PNM reader has only level zero");
        output->reserved=0;output->width=source.width;output->height=source.height;output->downsample=1;return QPX_READER_OK;
    });
}
qpx_reader_status_v1 QPX_READER_CALL channel_name(void* value,std::uint32_t channel,qpx_reader_text_v1* output,qpx_reader_text_v1* error) noexcept {
    return boundary(error,[&]() ->qpx_reader_status_v1 {
        const auto& source=image(value);require(output!=nullptr,"PNM channel-name output is null");require(channel<source.channels,"PNM channel index is out of range");
        const char* name="";
        if(source.blackness)name="Blackness";
        else if(source.channels<=2)name=channel==0?"Gray":"Alpha";
        else {const char* names[]={"Red","Green","Blue","Alpha"};name=names[channel];}
        return text(*output,name)?QPX_READER_OK:failure(error,QPX_READER_BUFFER_TOO_SMALL,"PNM channel-name buffer is too small");
    });
}
struct Region {std::int64_t width,height;std::uint64_t pixels;};
Region region(const qpx_reader_region_v1* request,std::uint64_t bytes_per_pixel) {
    require(request && request->struct_size>=sizeof(*request),"PNM region structure is too small");
    require(request->level==0 && !request->z && !request->t,"PNM reader has only level zero, Z=0 and T=0");
    const auto width=request->width,height=request->height;
    require(width>0 && height>0 && static_cast<std::uint64_t>(width)<=work_limit/bytes_per_pixel &&
        static_cast<std::uint64_t>(height)<=work_limit/(bytes_per_pixel*static_cast<std::uint64_t>(width)),"PNM output must have positive dimensions and fit 256 MiB");
    const auto limit=std::numeric_limits<std::int64_t>::max();
    require(request->x<=limit-width && request->y<=limit-height,"PNM region coordinate overflow");
    return {width,height,static_cast<std::uint64_t>(width)*static_cast<std::uint64_t>(height)};
}
qpx_reader_status_v1 QPX_READER_CALL read_rgba(void* value,const qpx_reader_region_v1* request,qpx_reader_pixels_v1* output,qpx_reader_text_v1* error) noexcept {
    return boundary(error,[&]() ->qpx_reader_status_v1 {
        const auto& source=image(value);require(output && output->struct_size>=sizeof(*output),"PNM pixel structure is too small");output->written=0;output->validity_written=0;
        const auto area=region(request,4);require(!request->first_channel && !request->channels,"PNM RGBA request has quantitative coordinates");
        const auto row_stride=static_cast<std::uint64_t>(area.width)*4,count=area.pixels*4;
        require(output->sample_type==QPX_READER_UINT8 && output->width==area.width && output->height==area.height && output->channels==4 && !output->reserved &&
            output->row_stride==row_stride && output->pixel_stride==4 && output->channel_stride==1,"PNM RGBA output must use packed UInt8x4 layout");
        require(output->data && output->capacity>=count && !output->validity && !output->validity_capacity,"PNM RGBA output buffer is invalid");
        std::fill_n(output->data,static_cast<std::size_t>(count),std::uint8_t(0));
        for(std::int64_t row=0;row<area.height;++row)for(std::int64_t col=0;col<area.width;++col) {
            const auto x=request->x+col,y=request->y+row;if(x<0 || y<0 || x>=source.width || y>=source.height)continue;
            std::memcpy(output->data+(static_cast<std::uint64_t>(row)*area.width+col)*4,source.rgba.get()+(static_cast<std::uint64_t>(y)*source.width+x)*4,4);
        }
        output->written=count;return QPX_READER_OK;
    });
}
qpx_reader_status_v1 QPX_READER_CALL read_raw(void* value,const qpx_reader_region_v1* request,qpx_reader_pixels_v1* output,qpx_reader_cancel_v1 cancel,qpx_reader_text_v1* error) noexcept {
    return boundary(error,[&]() ->qpx_reader_status_v1 {
        const auto& source=image(value);require(output && output->struct_size>=sizeof(*output),"PNM raw pixel structure is too small");output->written=0;output->validity_written=0;
        require(request && request->struct_size>=sizeof(*request),"PNM raw region structure is too small");
        require(request->channels>0 && request->first_channel<source.channels && request->channels<=source.channels-request->first_channel,"PNM raw channel range is invalid");
        const auto pixel_stride=static_cast<std::uint64_t>(request->channels)*source.sample_bytes;
        const auto area=region(request,pixel_stride);const auto count=area.pixels*pixel_stride,row_stride=static_cast<std::uint64_t>(area.width)*pixel_stride;
        require(area.pixels<=work_limit-count,"PNM raw output and validity exceed 256 MiB");
        const auto sample_type=source.sample_bytes==2?QPX_READER_UINT16:QPX_READER_UINT8;
        require(output->sample_type==sample_type && output->width==area.width && output->height==area.height && output->channels==request->channels && !output->reserved &&
            output->row_stride==row_stride && output->pixel_stride==pixel_stride && output->channel_stride==source.sample_bytes,"PNM raw output must use packed native-endian HWC layout");
        require(output->data && output->capacity>=count && output->validity && output->validity_capacity>=area.pixels,"PNM raw output or validity buffer is invalid");
        checkpoint(cancel);std::fill_n(output->data,static_cast<std::size_t>(count),std::uint8_t(0));std::fill_n(output->validity,static_cast<std::size_t>(area.pixels),std::uint8_t(0));
        for(std::int64_t row=0;row<area.height;++row) {
            checkpoint(cancel);
            for(std::int64_t col=0;col<area.width;++col) {
                if((col&4095)==0)checkpoint(cancel);
                const auto x=request->x+col,y=request->y+row;if(x<0 || y<0 || x>=source.width || y>=source.height)continue;
                const auto to=static_cast<std::uint64_t>(row)*area.width+col;
                const auto from=(static_cast<std::uint64_t>(y)*source.width+x)*source.channels+request->first_channel;
                std::memcpy(output->data+to*pixel_stride,source.raw.get()+from*source.sample_bytes,static_cast<std::size_t>(pixel_stride));output->validity[to]=1;
            }
        }
        checkpoint(cancel);output->written=count;output->validity_written=area.pixels;return QPX_READER_OK;
    });
}
}

extern "C" QPX_READER_EXPORT qpx_reader_status_v1 QPX_READER_CALL qpx_reader_entry_v1(
    std::uint32_t requested_abi,qpx_reader_api_v1* output,qpx_reader_text_v1* error) {
    return boundary(error,[&]() ->qpx_reader_status_v1 {
        require(requested_abi==QPX_READER_ABI_V1,"PNM reader does not support the requested ABI");
        require(output && output->struct_size>=sizeof(*output),"PNM reader API structure is too small");
        *output={sizeof(*output),QPX_READER_ABI_V1,describe,open,destroy,metadata,level,channel_name,read_rgba,read_raw};return QPX_READER_OK;
    });
}
