#pragma once
#include "../gui_calibration_editor.hpp"
#include <imgui_internal.h>
#include <iostream>
namespace calibration_checks {
inline void run() {
    const auto require=[](bool value,const char* why){if(!value)throw std::runtime_error(why);};
    struct Context {ImGuiContext* value=ImGui::CreateContext();~Context(){ImGui::DestroyContext(value);}} context;
    auto& io=ImGui::GetIO();io.IniFilename=nullptr;io.LogFilename=nullptr;io.DisplaySize={1000,800};io.DeltaTime=1.f/60;io.ConfigInputTrickleEventQueue=false;
    unsigned char* alpha;int width,height;io.Fonts->GetTexDataAsAlpha8(&alpha,&width,&height);
    qpx::Document document(qpx::Image::demo()),other(qpx::Image::demo());
    const auto raw=document.server()->read_region({0,0,0,1,1}).bytes();
    document.set_calibration(.12345678901234566,.5);other.set_calibration(2,3);
    qpx::detail::CalibrationEditor editor;auto* active=&document;std::uint64_t image=7,epoch=11;
    const auto frame=[&]{ImGui::NewFrame();ImGui::Begin("Host");editor.draw(active,image,epoch);ImGui::End();ImGui::Render();};
    const auto open=[&]{editor.open(document,7,11);frame();frame();};
    const auto id=[&](const char* label){auto* window=ImGui::FindWindowByName("Edit calibration");require(window!=nullptr,"Calibration popup missing");return ImHashStr(label,0,window->ID);};
    const auto activate=[&](const char* label){ImGui::ActivateItemByID(id(label));frame();frame();};
    const auto input=[&](const char* label,const char* text){
        const auto input_id=ImHashStr("##value",0,id(label));
        ImGui::ActivateItemByID(input_id);GImGui->NavNextActivateFlags=ImGuiActivateFlags_PreferInput;frame();frame();
        require(ImGui::GetActiveID()==input_id,"Calibration input did not activate");
        io.AddKeyEvent(ImGuiMod_Ctrl,true);io.AddKeyEvent(ImGuiKey_A,true);frame();
        io.AddKeyEvent(ImGuiKey_A,false);io.AddKeyEvent(ImGuiMod_Ctrl,false);frame();
        io.AddInputCharactersUTF8(text);frame();io.AddKeyEvent(ImGuiKey_Enter,true);frame();io.AddKeyEvent(ImGuiKey_Enter,false);frame();
    };
    auto revision=document.revision();open();activate("Apply calibration");
    require(!editor.active() && document.revision()==revision && document.pixel_width_um()==.12345678901234566,"No-op calibration apply changed exact value");
    open();input("Pixel width (um)","0.375");activate("Apply calibration");
    require(!editor.active() && document.pixel_width_um()==.375 && document.pixel_height_um()==.5,"Calibration Apply lost native per-axis values");
    require(document.undo() && document.pixel_width_um()==.12345678901234566,"Calibration Apply is not undoable");
    open();input("Pixel width (um)","-1");revision=document.revision();activate("Apply calibration");
    require(editor.active() && document.revision()==revision,"Invalid calibration escaped native validation");activate("Cancel");
    open();input("Pixel width (um)","4");revision=document.revision();activate("Cancel");
    require(!editor.active() && document.revision()==revision,"Calibration Cancel applied a draft");
    open();activate("Clear calibration");require(!document.pixel_width_um() && !document.pixel_height_um(),"Calibration Clear did not remove physical values");
    require(document.undo() && document.pixel_width_um()==.12345678901234566,"Calibration Clear is not undoable");
    open();document.set_calibration(8,9);revision=document.revision();activate("Apply calibration");
    require(editor.active() && document.revision()==revision && document.pixel_width_um()==8,"Stale calibration revision accepted draft");activate("Cancel");
    for(int stale=0;stale<3;++stale) {
        open();if(stale==0)active=&other;else if(stale==1)++image;else ++epoch;
        revision=document.revision();activate("Clear calibration");require(editor.active() && document.revision()==revision && other.pixel_width_um()==2,"Stale calibration owner cleared data");activate("Cancel");
        active=&document;image=7;epoch=11;
    }
    open();revision=document.revision();io.AddKeyEvent(ImGuiKey_Escape,true);frame();io.AddKeyEvent(ImGuiKey_Escape,false);frame();
    require(!editor.active() && document.revision()==revision,"Calibration Escape changed data");
    require(document.server()->read_region({0,0,0,1,1}).bytes()==raw,"Calibration changed raw pixels");
    std::cout<<"Calibration editor: exact no-op, Apply/Clear/Undo, invalid input, Cancel/Escape and stale document/image/epoch/revision passed\n";
}
}
