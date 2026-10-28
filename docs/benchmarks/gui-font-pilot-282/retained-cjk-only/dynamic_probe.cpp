#include "imgui.h"
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <map>
#include <stdexcept>
#include <string>
#include <vector>

void require(bool value,const char* message) {if(!value) throw std::runtime_error(message);}
struct Glyph {
    unsigned int code;bool source=false,present=false;float advance=0;
    std::uint64_t nonzero=0,hash=14695981039346656037ULL;
};
int main(int argc,char** argv) {
    try {
    if(argc!=6) return 2;
    const float logical=std::stof(argv[3]),density=std::stof(argv[4]);
    const int stress=std::stoi(argv[5]);
    require(logical>=.5f && logical<=6 && density>=.5f && density<=4 && stress>=0 && stress<=12000,"invalid pilot options");
    const auto start=std::chrono::steady_clock::now();
    IMGUI_CHECKVERSION();
    struct Context {ImGuiContext* value=ImGui::CreateContext();~Context(){ImGui::DestroyContext(value);}} context;
    auto& io=ImGui::GetIO();io.IniFilename=nullptr;io.LogFilename=nullptr;
    io.BackendFlags|=ImGuiBackendFlags_RendererHasTextures;
    io.BackendRendererName="qpx_cpu_texture_protocol_pilot";
    io.DisplaySize={640,480};io.DisplayFramebufferScale={density,density};io.DeltaTime=1.f/60;
    auto& atlas=*io.Fonts;atlas.TexMinWidth=512;atlas.TexMinHeight=128;
    atlas.TexMaxWidth=atlas.TexMaxHeight=2048;
    auto& platform=ImGui::GetPlatformIO();platform.Renderer_TextureMaxWidth=platform.Renderer_TextureMaxHeight=2048;
    ImFontConfig base;base.SizePixels=13*logical;
    auto* font=atlas.AddFontDefaultBitmap(&base);require(font!=nullptr,"default font missing");
    ImFontConfig merge;merge.MergeMode=true;merge.PixelSnapH=true;merge.SizePixels=13*logical;
    require(atlas.AddFontFromFileTTF(argv[1],merge.SizePixels,&merge)!=nullptr,"CJK font load failed");
    ImGui::GetStyle().FontSizeBase=13*logical;
    const unsigned int samples[]={0x41,0xe9,0x3b1,0x539f,0x59cb,0x96ea,0x56fe,0x50cf,
        0x65e5,0x672c,0x8a9e,0x3042,0x30a2,0xd55c,0xad6d,0xc5b4,0x2000b,0x1f52c,0x1f469};
    std::vector<Glyph> glyphs;for(auto code:samples) glyphs.push_back({code});
    std::map<ImTextureID,std::uint64_t> simulated_gpu;
    std::uint64_t next_id=1,peak_cpu=0,peak_gpu=0,creates=0,updates=0,destroys=0,stress_source=0,stress_present=0;
    int ascii_glyphs=0,final_glyphs=0,final_width=0,final_height=0;float actual_density=0;
    std::vector<std::uint64_t> atlas_ids;
    for(int frame=0;frame<6;++frame) {
        ImGui::NewFrame();ImGui::Begin("Font pilot");ImGui::TextUnformatted("ASCII_A");
        auto* baked=ImGui::GetFontBaked();actual_density=baked->RasterizerDensity;
        if(frame==0) ascii_glyphs=baked->Glyphs.Size;
        if(frame==1) for(auto& sample:glyphs) {
            sample.source=font->IsGlyphInFont(sample.code);
            const auto* glyph=baked->FindGlyphNoFallback(sample.code);
            sample.present=glyph && glyph->Codepoint==sample.code;
        }
        if(frame==2) for(int i=0;i<stress;++i) {
            const auto code=static_cast<ImWchar>(0x4e00+i);
            stress_source+=font->IsGlyphInFont(code);
            const auto* glyph=baked->FindGlyphNoFallback(code);
            stress_present+=glyph && glyph->Codepoint==code;
        }
        if(frame==3) {
            // All requests above are finished: recover current UVs after any
            // repack instead of retaining glyph/texture pointers across growth.
            for(auto& sample:glyphs) {
                const auto* glyph=baked->FindGlyphNoFallback(sample.code);
                sample.present=glyph && glyph->Codepoint==sample.code;
                if(!sample.present) continue;
                sample.advance=glyph->AdvanceX;
                auto& texture=*atlas.TexData;
                const int x0=std::lround(glyph->U0*texture.Width),y0=std::lround(glyph->V0*texture.Height);
                const int x1=std::lround(glyph->U1*texture.Width),y1=std::lround(glyph->V1*texture.Height);
                const auto* pixels=static_cast<const unsigned char*>(texture.GetPixels());
                for(int y=y0;y<y1;++y) for(int x=x0;x<x1;++x) {
                    const auto value=pixels[(std::size_t(y)*texture.Width+x)*texture.BytesPerPixel+(texture.BytesPerPixel==4?3:0)];
                    sample.nonzero+=value!=0;sample.hash=(sample.hash^value)*1099511628211ULL;
                }
            }
        }
        final_glyphs=baked->Glyphs.Size;
        if(atlas.TexData) {final_width=atlas.TexData->Width;final_height=atlas.TexData->Height;atlas_ids.push_back(atlas.TexData->UniqueID);}
        ImGui::End();ImGui::Render();
        auto* textures=ImGui::GetDrawData()->Textures;require(textures!=nullptr,"texture protocol absent");
        std::uint64_t cpu=0;
        for(auto* texture:*textures) {
            require(texture->Width<=2048 && texture->Height<=2048,"atlas exceeded configured dimensions");
            if(texture->Pixels) cpu+=texture->GetSizeInBytes();
            if(texture->Status==ImTextureStatus_WantCreate) {
                const ImTextureID id=next_id++;texture->SetTexID(id);texture->SetStatus(ImTextureStatus_OK);
                simulated_gpu[id]=texture->GetSizeInBytes();++creates;
            }else if(texture->Status==ImTextureStatus_WantUpdates) {texture->SetStatus(ImTextureStatus_OK);++updates;}
            else if(texture->Status==ImTextureStatus_WantDestroy) {
                simulated_gpu.erase(texture->GetTexID());texture->SetTexID(ImTextureID_Invalid);texture->SetStatus(ImTextureStatus_Destroyed);++destroys;
            }
        }
        std::uint64_t gpu=0;for(const auto& entry:simulated_gpu) gpu+=entry.second;
        peak_cpu=std::max(peak_cpu,cpu);peak_gpu=std::max(peak_gpu,gpu);
    }
    require(sizeof(ImWchar)==4,"pilot must use32-bit characters");
    require(std::abs(actual_density-density)<.01f,"framebuffer density was not applied");
    require(ascii_glyphs<100 && final_glyphs>=ascii_glyphs,"dynamic lazy loading not observed");
    std::ofstream report(argv[2]);report<<std::boolalpha;
    report<<"{\"imgui_version\":\""<<ImGui::GetVersion()<<"\",\"logical\":"<<logical<<",\"framebuffer_density\":"<<density
          <<",\"actual_rasterizer_density\":"<<actual_density<<",\"wchar_bits\":32,\"ascii_frame_glyphs\":"<<ascii_glyphs
          <<",\"final_glyphs\":"<<final_glyphs<<",\"atlas_width\":"<<final_width<<",\"atlas_height\":"<<final_height
          <<",\"atlas_limit_rgba_bytes\":16777216,\"peak_cpu_texture_bytes\":"<<peak_cpu<<",\"simulated_peak_gpu_texture_bytes\":"<<peak_gpu
          <<",\"texture_creates\":"<<creates<<",\"texture_updates\":"<<updates<<",\"texture_destroys\":"<<destroys
          <<",\"stress_requested\":"<<stress<<",\"stress_font_supported\":"<<stress_source<<",\"stress_present\":"<<stress_present
          <<",\"elapsed_ms\":"<<std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count()<<",\"atlas_ids\":[";
    for(std::size_t i=0;i<atlas_ids.size();++i) {if(i) report<<',';report<<atlas_ids[i];}
    report<<"],\"samples\":[";
    for(std::size_t i=0;i<glyphs.size();++i) {const auto& s=glyphs[i];if(i) report<<',';
        report<<"{\"codepoint\":"<<s.code<<",\"font_supported\":"<<s.source<<",\"present\":"<<s.present
              <<",\"advance\":"<<s.advance<<",\"nonzero_raster_pixels\":"<<s.nonzero<<",\"raster_fnv1a64\":\""<<s.hash<<"\"}";}
    report<<"]}\n";require(bool(report),"report write failed");
    std::cout<<"dynamic logical="<<logical<<" density="<<density<<" glyphs="<<final_glyphs<<" atlas="<<final_width<<'x'<<final_height
             <<" cpu_peak="<<peak_cpu<<" simulated_gpu_peak="<<peak_gpu<<" stress="<<stress_present<<'/'<<stress_source<<'\n';
    }catch(const std::exception& error) {std::cerr<<error.what()<<'\n';return 1;}
}
