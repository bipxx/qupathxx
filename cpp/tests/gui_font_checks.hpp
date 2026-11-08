#pragma once
#include "../gui_fonts.hpp"
#include "../sha256.hpp"
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <map>
#include <stdexcept>
namespace gui_font_checks {
inline void require(bool value) {if(!value) throw std::runtime_error("GUI dynamic font check failed");}
template<class Function> inline void refuses(Function action) {
    try {action();} catch(const std::runtime_error&) {return;}
    throw std::runtime_error("GUI font asset was not rejected");
}
inline void resources() {
    const auto& assets=qpx::detail::gui_font_assets();
    require(assets.cjk.size()==16437364 && assets.emoji.size()==1982596);
    require(&assets==&qpx::detail::gui_font_assets());
    const auto directory=std::filesystem::temp_directory_path()/("qpx-font-asset-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    require(std::filesystem::create_directory(directory));const auto path=directory/std::filesystem::u8path(u8"\u5b57\u4f53.bin");
    const std::string data="bounded font resource fixture";qpx::Sha256 h;h.update(data.data(),data.size());const auto digest=h.finish();
    refuses([&] {qpx::detail::load_gui_font_asset(path,data.size(),digest);});
    {std::ofstream file(path,std::ios::binary);file<<data;require(bool(file));}
    require(qpx::detail::load_gui_font_asset(path,data.size(),digest)==std::vector<unsigned char>(data.begin(),data.end()));
    refuses([&] {qpx::detail::load_gui_font_asset(path,data.size()+1,digest);});
    {std::fstream file(path,std::ios::binary|std::ios::in|std::ios::out);file.put('!');require(bool(file));}
    refuses([&] {qpx::detail::load_gui_font_asset(path,data.size(),digest);});
    std::filesystem::remove(path);require(std::filesystem::is_empty(directory));std::filesystem::remove(directory);
}
inline void frame_textures(std::map<ImTextureID,std::size_t>& gpu,std::uint64_t& next,std::size_t& cpu_peak,std::size_t& gpu_peak) {
    auto* textures=ImGui::GetDrawData()->Textures;require(textures!=nullptr);std::size_t cpu=0;
    for(auto* texture:*textures) {
        require(texture->Width<=2048 && texture->Height<=2048);
        if(texture->Pixels) cpu+=static_cast<std::size_t>(texture->GetSizeInBytes());
        if(texture->Status==ImTextureStatus_WantCreate) {
            const ImTextureID id=next++;texture->SetTexID(id);texture->SetStatus(ImTextureStatus_OK);gpu[id]=texture->GetSizeInBytes();
        }else if(texture->Status==ImTextureStatus_WantUpdates) texture->SetStatus(ImTextureStatus_OK);
        else if(texture->Status==ImTextureStatus_WantDestroy) {
            gpu.erase(texture->GetTexID());texture->SetTexID(ImTextureID_Invalid);texture->SetStatus(ImTextureStatus_Destroyed);
        }
    }
    std::size_t total=0;for(const auto& value:gpu) total+=value.second;
    cpu_peak=std::max(cpu_peak,cpu);gpu_peak=std::max(gpu_peak,total);
}
inline void glyphs(float logical,float density,bool stress) {
    struct Context {ImGuiContext* value=ImGui::CreateContext();~Context(){ImGui::DestroyContext(value);}} context;
    auto& io=ImGui::GetIO();io.IniFilename=nullptr;io.LogFilename=nullptr;io.DisplaySize={640,480};io.DisplayFramebufferScale={density,density};io.DeltaTime=1.f/60;
    io.BackendFlags|=ImGuiBackendFlags_RendererHasTextures;
    ImGui::GetPlatformIO().Renderer_TextureMaxWidth=ImGui::GetPlatformIO().Renderer_TextureMaxHeight=2048;
    qpx::detail::configure_gui_fonts(*io.Fonts,2048);ImGui::GetStyle().FontSizeBase=13*logical;
    const ImWchar samples[]={0x41,0xe9,0x3b1,0x539f,0x59cb,0x96ea,0x56fe,0x50cf,0x65e5,0x672c,0x8a9e,
        0x3042,0x30a2,0xd55c,0xad6d,0xc5b4,0x2000b,0x1f52c,0x1f469,0x301,0x200d};
    std::map<ImTextureID,std::size_t> gpu;std::uint64_t next=1;std::size_t cpu_peak=0,gpu_peak=0;int stress_present=0;
    for(int frame=0;frame<5;++frame) {
        ImGui::NewFrame();ImGui::Begin("Packaged font check");ImGui::TextUnformatted("A_e");
        auto* baked=ImGui::GetFontBaked();require(std::abs(baked->RasterizerDensity-density)<.01f);
        if(frame==0) require(baked->Glyphs.Size<100);
        if(frame==1) for(const auto code:samples) {
            const auto* glyph=baked->FindGlyphNoFallback(code);require(glyph && glyph->Codepoint==code);
            if(code==0x301 || code==0x200d) require(glyph->AdvanceX==0);
        }
        if(frame==2 && stress) for(unsigned code=0x4e00;code<0x4e00+1600;++code) {
            require(ImGui::GetFont()->IsGlyphInFont(code));const auto* glyph=baked->FindGlyphNoFallback(code);stress_present+=glyph && glyph->Codepoint==code;
        }
        if(frame==2 && !stress) for(const auto code:samples) {
            const auto* glyph=baked->FindGlyphNoFallback(code);require(glyph && glyph->Codepoint==code);
            if(code==0x200d) continue;
            auto& texture=*io.Fonts->TexData;require(texture.BytesPerPixel==4);
            const int x0=std::lround(glyph->U0*texture.Width),x1=std::lround(glyph->U1*texture.Width),y0=std::lround(glyph->V0*texture.Height),y1=std::lround(glyph->V1*texture.Height);
            std::size_t nonzero=0;const auto* pixels=static_cast<const unsigned char*>(texture.GetPixels());
            for(int y=y0;y<y1;++y) for(int x=x0;x<x1;++x) nonzero+=pixels[(std::size_t(y)*texture.Width+x)*4+3]!=0;
            require(nonzero>0);if(code==0x41 && logical==1) require(glyph->AdvanceX==7);
        }
        ImGui::End();ImGui::Render();frame_textures(gpu,next,cpu_peak,gpu_peak);
    }
    if(stress) {require(stress_present>0 && stress_present<1600);require(io.Fonts->TexData->Width==2048 && io.Fonts->TexData->Height==2048);}
    // This measured protocol case permits old/new versions simultaneously.
    require(cpu_peak<=2u*2048*2048*4 && gpu_peak<=2u*2048*2048*4);
    for(auto* texture:ImGui::GetPlatformIO().Textures) {texture->SetTexID(ImTextureID_Invalid);texture->SetStatus(ImTextureStatus_Destroyed);}
}
inline void run() {
    static_assert(sizeof(ImWchar)==4,"GUI font lookup requires full Unicode scalars");resources();
    glyphs(1,1,false);glyphs(1.5f,1.25f,false);glyphs(3,2,false);glyphs(6,4,false);glyphs(3,2,true);
    std::cout<<"GUI packaged dynamic fonts, bounded atlas and recoverable glyph exhaustion passed\n";
}
}
