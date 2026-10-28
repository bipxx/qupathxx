#include "imgui.h"
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <string>
#include <vector>

int main(int argc,char** argv) {
    if(argc!=7) return 2;
    const std::string mode=argv[1];const float logical=std::stof(argv[2]),density=std::stof(argv[3]);
    if(!(logical>=.5f && logical<=6 && density>=.5f && density<=4)) return 3;
    const bool merged=mode!="default",full=mode.find("full")!=std::string::npos,capped=mode.find("capped")!=std::string::npos;
    if(mode!="default" && mode!="merge-corpus" && mode!="merge-full-bmp" && mode!="merge-corpus-capped" && mode!="merge-full-bmp-capped") return 4;
    if(merged && full && !capped && 13*logical*density>26) return 5;
    const unsigned int samples[]={0x41,0xE9,0x3B1,0x539F,0x59CB,0x96EA,0x56FE,0x50CF,
        0x65E5,0x672C,0x8A9E,0x3042,0x30A2,0xD55C,0xAD6D,0xC5B4,0x2000B,0x1F52C,0x1F469};
    const auto began=std::chrono::steady_clock::now();
    ImGui::CreateContext();auto& io=ImGui::GetIO();io.IniFilename=nullptr;io.LogFilename=nullptr;
    ImFontConfig base;base.SizePixels=13*logical;base.RasterizerDensity=density;
    auto* font=io.Fonts->AddFontDefault(&base);
    ImVector<ImWchar> corpus;const ImWchar bmp[]={0x20,0xFFFD,0};
    float merge_density=density;
    if(merged) {
        ImFontGlyphRangesBuilder builder;
        for(auto code:samples) if(code<=std::numeric_limits<ImWchar>::max()) builder.AddChar(static_cast<ImWchar>(code));
        builder.BuildRanges(&corpus);
        ImFontConfig merge;merge.MergeMode=true;merge.PixelSnapH=true;merge.SizePixels=base.SizePixels;
        merge_density=capped?std::min(density,16.f/base.SizePixels):density;
        merge.RasterizerDensity=merge_density;
        if(!io.Fonts->AddFontFromFileTTF(argv[4],base.SizePixels,&merge,full?bmp:corpus.Data)) return 6;
    }
    unsigned char* alpha=nullptr;int width=0,height=0,bpp=0;
    io.Fonts->GetTexDataAsAlpha8(&alpha,&width,&height,&bpp);if(!alpha || bpp!=1) return 7;
    const auto ms=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-began).count();
    constexpr int cell=96,columns=8,rows=3;std::vector<unsigned char> strip(cell*columns*cell*rows);
    std::ofstream report(argv[5]);report<<std::setprecision(9)<<std::boolalpha;
    report<<"{\"imgui_version\":\""<<ImGui::GetVersion()<<"\",\"mode\":\""<<mode
        <<"\",\"logical\":"<<logical<<",\"framebuffer_density\":"<<density
        <<",\"merge_density\":"<<merge_density<<",\"wchar_bits\":"<<sizeof(ImWchar)*8
        <<",\"font_size\":"<<font->FontSize<<",\"glyphs\":"<<font->Glyphs.Size
        <<",\"atlas_width\":"<<width<<",\"atlas_height\":"<<height
        <<",\"cpu_alpha_bytes\":"<<std::uint64_t(width)*height
        <<",\"rgba32_upload_estimated_bytes\":"<<std::uint64_t(width)*height*4
        <<",\"build_ms\":"<<ms<<",\"samples\":[";
    for(std::size_t index=0;index<sizeof(samples)/sizeof(samples[0]);++index) {
        const auto code=samples[index];const bool supported_code=code<=std::numeric_limits<ImWchar>::max();
        const auto* glyph=supported_code?font->FindGlyphNoFallback(static_cast<ImWchar>(code)):nullptr;
        const auto* drawn=supported_code?font->FindGlyph(static_cast<ImWchar>(code)):nullptr;
        std::uint64_t nonzero=0,hash=14695981039346656037ULL;
        if(glyph) {
            const int x0=std::lround(glyph->U0*width),y0=std::lround(glyph->V0*height);
            const int x1=std::lround(glyph->U1*width),y1=std::lround(glyph->V1*height);
            for(int y=y0;y<y1;++y) for(int x=x0;x<x1;++x) {
                const auto value=alpha[y*width+x];nonzero+=value!=0;hash=(hash^value)*1099511628211ULL;
                const int sx=int(index%columns)*cell+2+x-x0,sy=int(index/columns)*cell+2+y-y0;
                if(sx<(int(index%columns)+1)*cell && sy<(int(index/columns)+1)*cell) strip[sy*cell*columns+sx]=value;
            }
        }
        if(index) report<<',';
        report<<"{\"codepoint\":"<<code<<",\"present\":"<<(glyph!=nullptr)
              <<",\"drawn_codepoint\":"<<(drawn?drawn->Codepoint:0)<<",\"advance\":"<<(glyph?glyph->AdvanceX:0)
              <<",\"nonzero_raster_pixels\":"<<nonzero<<",\"raster_fnv1a64\":\""<<hash<<"\"}";
    }
    report<<"]}\n";report.close();
    std::ofstream image(argv[6],std::ios::binary);image<<"P5\n"<<cell*columns<<' '<<cell*rows<<"\n255\n";
    image.write(reinterpret_cast<const char*>(strip.data()),strip.size());image.close();
    std::cout<<mode<<" logical="<<logical<<" density="<<density<<" glyphs="<<font->Glyphs.Size
             <<" atlas="<<width<<'x'<<height<<" alpha_bytes="<<std::uint64_t(width)*height<<'\n';
    ImGui::DestroyContext();
}
