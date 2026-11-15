#pragma once
#include "../gui_dialogs.hpp"
#include "../gui_record_removal.hpp"
#include "qupathxx/tissue.hpp"
#include "../gui_import_picker.hpp"
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <set>
#include <type_traits>

namespace dialog_contract_checks {
namespace ui=qpx::ui;
inline void require(bool value,const char* message){if(!value)throw std::runtime_error(message);}
template<class Function>void refuses(Function function){try{function();}catch(const std::logic_error&){return;}throw std::runtime_error("Malformed dialog contract was accepted");}
inline ImGuiID action_id(const ui::DialogActionDefinition& action,ImGuiID seed) {
    if(action.scope){seed=ImHashStr("Entry fields",0,seed);seed=ImHashStr(action.scope,0,seed);}return ImHashStr(ui::i18n::label(action.label).c_str(),0,seed);
}
inline void definitions() {
    static_assert(!std::is_default_constructible_v<ui::DialogState>,"Owner and busy state must be explicit");
    static_assert(std::is_const_v<decltype(std::declval<ui::DialogState>().owner_valid)> && std::is_const_v<decltype(std::declval<ui::DialogState>().busy)>,"Ownership and busy snapshots must not change in a body callback");
    const auto language=ui::i18n::state;std::set<std::string> names;
    for(int index=0;index<static_cast<int>(ui::DialogKind::Count);++index) {
        const auto kind=static_cast<ui::DialogKind>(index);const auto& definition=ui::dialog_definition(kind);require(names.emplace(definition.name).second,"Duplicate modal identity");
        for(auto variant:ui::dialog_variants(kind))for(const auto& action:ui::dialog_actions(kind,variant)) {
            ui::DialogState ready(true,false,variant),invalid(false,false,variant),busy(true,true,variant);
            require(ui::dialog_action_enabled(ready,action) && !ui::dialog_action_enabled(busy,action),"Busy dialog action escaped its guard");
            require(ui::dialog_action_enabled(invalid,action)==(action.role==ui::DialogActionRole::Cancel || action.role==ui::DialogActionRole::Reload),"Invalid owner can publish a dialog command");
            ui::i18n::choose(0);const auto english=action_id(action,12345);
            for(int choice=0;choice<ui::i18n::language_count;++choice){ui::i18n::choose(choice);require(action_id(action,12345)==english,"Dialog command identity changed with language");}
        }
    }
    for(int index=0;index<static_cast<int>(ui::DialogKind::Count);++index)
        for(auto variant:{ui::DialogVariant::Folder,ui::DialogVariant::NewFolder})
            require(ui::dialog_variant_allowed(static_cast<ui::DialogKind>(index),variant)==(static_cast<ui::DialogKind>(index)==ui::DialogKind::ImportFile),"Folder variants escaped ImportFile");
    ui::i18n::state=language;
    refuses([]{ui::dialog_definition(static_cast<ui::DialogKind>(999));});
    refuses([]{ui::dialog_actions(ui::DialogKind::CloseProject,ui::DialogVariant::Details);});
    auto actions=ui::dialog_actions(ui::DialogKind::CloseProject,ui::DialogVariant::Default);actions.push_back(actions.front());refuses([&]{ui::validate_dialog_actions(actions);});
    actions=ui::dialog_actions(ui::DialogKind::CloseProject,ui::DialogVariant::Default);std::swap(actions.front(),actions.back());refuses([&]{ui::validate_dialog_actions(actions);});
    actions=ui::dialog_actions(ui::DialogKind::CloseProject,ui::DialogVariant::Default);actions.back().role=ui::DialogActionRole::Destructive;refuses([&]{ui::validate_dialog_actions(actions);});
    actions=ui::dialog_actions(ui::DialogKind::RemoveImage,ui::DialogVariant::Default);actions.back().label="Cancel";refuses([&]{ui::validate_dialog_actions(actions);});
    std::cout<<"Dialog definitions: all kinds/variants, role/order/identity, malformed rejection, owner/busy and localized stable IDs passed\n";
}
struct Fixture {
    ImGuiContext* context=ImGui::CreateContext();ui::DialogKind kind=ui::DialogKind::RemoveImage;
    bool owner=true,busy=false,requested=true,present=false,ambient_disabled=false;
    int published=0,cancelled=0;ui::DialogOutcome outcome=ui::DialogOutcome::Success;
    std::function<void(ui::DialogControls&)> content=[](ui::DialogControls&){};
    Fixture(){auto& io=ImGui::GetIO();io.IniFilename=nullptr;io.LogFilename=nullptr;io.DisplaySize={1000,900};io.DeltaTime=1.f/60;io.ConfigFlags|=ImGuiConfigFlags_NavEnableKeyboard;io.ConfigInputTrickleEventQueue=false;io.Fonts->AddFontDefault();unsigned char* pixels;int w,h;io.Fonts->GetTexDataAsRGBA32(&pixels,&w,&h);ui::design::apply(ImGui::GetStyle(),ui::design::Theme::Dark);}
    ~Fixture(){ImGui::DestroyContext(context);}
    void frame() {
        ImGui::NewFrame();ImGui::Begin("Contract host");if(requested){ui::OpenDialog(kind);requested=false;}
        ImGui::BeginDisabled(ambient_disabled);ui::DialogState state(owner,busy);
        ui::RenderDialog(kind,state,"Contract item",{},content,[&](ui::DialogAction action){if(action==ui::DialogAction::Cancel){++cancelled;return ui::DialogOutcome::Success;}++published;return outcome;});
        ImGui::EndDisabled();present=ui::IsDialogOpen(kind);ImGui::End();ImGui::Render();
    }
    void warm(){frame();frame();frame();}
    void activate(ui::DialogAction id) {
        const auto* window=ImGui::FindWindowByName(ui::dialog_definition(kind).name);require(window!=nullptr,"Contract fixture modal missing");
        for(const auto& action:ui::dialog_actions(kind,ui::DialogVariant::Default))if(action.id==id){ImGui::ActivateItemByID(action_id(action,window->ID));frame();frame();return;}
        throw std::runtime_error("Fixture action not declared");
    }
    void escape(){auto& io=ImGui::GetIO();io.AddKeyEvent(ImGuiKey_Escape,true);frame();io.AddKeyEvent(ImGuiKey_Escape,false);frame();frame();}
};
inline void interactions() {
    {Fixture f;f.owner=false;f.warm();f.activate(ui::DialogAction::Confirm);require(f.present && !f.published,"Invalid owner dispatched publication");f.activate(ui::DialogAction::Cancel);require(!f.present && f.cancelled==1,"Invalid owner could not cancel");}
    {Fixture f;f.busy=true;f.warm();f.activate(ui::DialogAction::Confirm);f.escape();f.activate(ui::DialogAction::Cancel);require(f.present && !f.published && !f.cancelled,"Busy dialog dispatched or dismissed");f.busy=false;f.escape();require(!f.present && f.cancelled==1,"Ready dialog did not resume cancellation");}
    {Fixture f;f.ambient_disabled=true;f.warm();f.activate(ui::DialogAction::Confirm);require(f.published==1 && !f.present,"Modal inherited the disabled workbench instead of its declared state");}
    {Fixture f;f.outcome=ui::DialogOutcome::Refused;f.warm();f.activate(ui::DialogAction::Confirm);require(f.present && f.published==1,"Refused command dismissed its dialog");f.outcome=ui::DialogOutcome::Pending;f.activate(ui::DialogAction::Confirm);require(f.present && f.published==2,"Pending command dismissed its dialog");f.outcome=ui::DialogOutcome::Success;f.activate(ui::DialogAction::Confirm);require(!f.present && f.published==3,"Successful confirmation did not dismiss");}
    {Fixture f;bool open_child=false,child_visible=false;f.content=[&](ui::DialogControls&){if(open_child){ImGui::OpenPopup("Child choice");open_child=false;}child_visible=ImGui::BeginPopup("Child choice");if(child_visible){ui::MenuItem("Choice");ImGui::EndPopup();}};f.warm();open_child=true;f.frame();f.frame();require(child_visible,"Child popup did not open");f.escape();require(f.present && !child_visible && !f.cancelled,"Escape propagated from a child popup into its parent");f.escape();require(!f.present && f.cancelled==1,"Second Escape did not dismiss the parent");}
    {Fixture f;char value[64]="original";f.content=[&](ui::DialogControls&){ui::DialogInput("Draft text",value,sizeof(value));};f.warm();auto* window=ImGui::FindWindowByName(ui::dialog_definition(f.kind).name);ImGui::ActivateItemByID(ImHashStr("Draft text",0,window->ID));GImGui->NavNextActivateFlags=ImGuiActivateFlags_PreferInput;f.frame();f.frame();ImGui::GetIO().AddInputCharactersUTF8(" changed");f.frame();f.escape();require(f.present && !f.cancelled && std::string(value)=="original","Escape did not remain owned by the text editor");f.escape();require(!f.present && f.cancelled==1,"Dialog did not cancel after text editing ended");}
    {Fixture f;f.kind=ui::DialogKind::CloseProject;f.content=[&](ui::DialogControls& controls){ui::dialog_detail::ControlAccess::request(controls,ui::DialogAction::Save);require(!ui::dialog_detail::ControlAccess::request(controls,ui::DialogAction::Discard),"Two dialog commands were selected in one frame");};f.warm();require(f.published==3 && f.present,"Single-dispatch keep-open contract failed");}
    {Fixture f;ImGui::NewFrame();ImGui::Begin("Exception host");auto* host=ImGui::GetCurrentWindow();ui::OpenDialog(f.kind);ui::DialogState state(true,false);bool caught=false;
        try{ui::RenderDialog(f.kind,state,"",{},[](ui::DialogControls&){throw std::runtime_error("body failure");},[](ui::DialogAction){return ui::DialogOutcome::Success;});}catch(const std::runtime_error&){caught=true;}
        require(caught && ImGui::GetCurrentWindow()==host && GImGui->BeginPopupStack.empty(),"Callback exception left renderer scopes unbalanced");ImGui::End();ImGui::Render();}
    std::cout<<"Dialog interaction contract: invalid/busy owners, modal input scope, refusal/pending/success, nested/text Escape and single dispatch passed\n";
}
inline void import_choice() {
    const auto root=std::filesystem::temp_directory_path()/("qpx-dialog-contract-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));std::filesystem::create_directory(root);
    const auto input=root/"metadata.txt",output=root/"new.csv";{std::ofstream file(input);file<<"unchanged input";}
    Fixture f;qpx::detail::GuiImportPicker picker;std::optional<std::string> selected;
    auto frame=[&]{ImGui::NewFrame();ImGui::Begin("Picker host");if(auto value=picker.draw())selected=value;ImGui::End();ImGui::Render();};
    auto activate=[&](const char* label){auto* window=ImGui::FindWindowByName("Choose import file");require(window!=nullptr,"Picker modal missing");ImGui::ActivateItemByID(ImHashStr(label,0,window->ID));frame();frame();};
    picker.open((root/"absent.txt").u8string(),{".txt"});frame();frame();activate("Choose file");require(!selected,"Invalid file choice published a path");activate("Cancel file choice");
    picker.open(input.u8string(),{".txt"});frame();frame();activate("Choose file");require(selected==input.u8string(),"Existing file choice did not preserve its exact path");selected.reset();
    picker.open(output.u8string(),{".csv"},true);frame();frame();activate("Choose new output");require(selected==output.u8string() && !std::filesystem::exists(output),"Output selection wrote a file");selected.reset();
    picker.open(input.u8string(),{".txt"},true);frame();frame();activate("Choose new output");require(!selected,"Legacy save picker accepted an existing output");activate("Cancel file choice");
    picker.open(input.u8string(),{".txt"},qpx::detail::PickerMode::NewFile);frame();frame();activate("Choose new output");require(selected==input.u8string(),"New file mode must defer replacement to its caller");selected.reset();
    picker.open(root.u8string(),{},qpx::detail::PickerMode::OpenFolder);frame();frame();activate("Choose folder");require(selected==root.u8string(),"Folder mode must select its current folder");selected.reset();
    picker.open((root/"new-folder").u8string(),{},qpx::detail::PickerMode::NewFolder);frame();frame();activate("Choose output folder");require(selected==(root/"new-folder").u8string() && !std::filesystem::exists(root/"new-folder"),"New folder choice must not create it");selected.reset();
    picker.open(input.u8string(),{".txt"});frame();frame();activate("Cancel file choice");require(!selected,"Picker Cancel selected a file");
    std::ifstream stream(input);std::string bytes{std::istreambuf_iterator<char>(stream),{}};require(bytes=="unchanged input","File choice modified its source");stream.close();std::filesystem::remove(input);std::filesystem::remove(root);
    std::cout<<"Typed import picker: invalid/refused, existing/new choice, cancellation and no filesystem writes passed\n";
}
inline void record_removal() {
    Fixture f;f.requested=false;f.kind=ui::DialogKind::RemoveKeptResult;
    qpx::Document doc(qpx::Image(8,8,std::vector<std::uint8_t>(8*8*3,20))),other(qpx::Image(8,8,std::vector<std::uint8_t>(8*8*3,20)));
    const auto object=doc.add_rectangle({0,0,8,8});qpx::StainTransform transform({{1,0,0},{0,1,0}});
    qpx::TissueRequest request(doc,object,transform);const auto id=doc.keep_tissue(request,qpx::detect_tissue(request));
    qpx::detail::RecordRemovalDialog dialog;std::string error;const qpx::Document* active=&doc;std::uint64_t epoch=4;
    std::string visible;bool replace=false;
    auto frame=[&] {ImGui::NewFrame();ImGui::Begin("Removal host");ImGui::LogToBuffer();
        if(replace){ui::OpenDialog(ui::DialogKind::RemoveImage);replace=false;}
        dialog.draw(*active,epoch,error);visible=GImGui->LogBuffer.c_str();ImGui::LogFinish();ImGui::End();ImGui::Render();};
    auto warm=[&] {frame();frame();frame();};
    auto activate=[&](ui::DialogAction command) {
        const auto* window=ImGui::FindWindowByName(ui::dialog_definition(f.kind).name);require(window!=nullptr,"Removal dialog absent");
        for(const auto& action:ui::dialog_actions(f.kind,ui::DialogVariant::Default))if(action.id==command)ImGui::ActivateItemByID(action_id(action,window->ID));
        frame();frame();
    };
    auto open=[&] {dialog.request_record(doc,4,"Tissue",id,&qpx::Document::tissue_record,&qpx::Document::remove_tissue);warm();};
    open();activate(ui::DialogAction::Cancel);require(doc.tissue_record_count()==1 && !dialog.pending(),"Removal Cancel changed records");
    open();active=&other;warm();require(visible.find("The selected item changed.")!=std::string::npos,"Stale removal owner has no visible notice");activate(ui::DialogAction::Confirm);require(doc.tissue_record_count()==1 && dialog.pending(),"Different document allowed removal");activate(ui::DialogAction::Cancel);
    active=&doc;open();epoch=5;warm();activate(ui::DialogAction::Confirm);require(doc.tissue_record_count()==1 && dialog.pending(),"Stale epoch allowed removal");activate(ui::DialogAction::Cancel);epoch=4;
    error="unrelated failure";open();require(visible.find(error)==std::string::npos,"Removal dialog inherited an unrelated error");
    replace=true;frame();frame();require(!dialog.pending(),"Replaced popup stranded removal owner");
    // Closing the replacement then requesting removal must work again.
    ImGui::ClosePopupToLevel(0,true);frame();open();require(dialog.pending(),"Removal could not be requested after popup replacement");
    activate(ui::DialogAction::Cancel);require(error=="unrelated failure","Removal Cancel cleared unrelated error");
    open();activate(ui::DialogAction::Confirm);require(error=="unrelated failure","Successful removal cleared unrelated error");require(!doc.tissue_record_count() && !dialog.pending(),"Confirmed removal did not remove exactly one record");
    open();activate(ui::DialogAction::Confirm);require(!doc.tissue_record_count() && dialog.pending(),"Missing record confirmation was not disabled");activate(ui::DialogAction::Cancel);
    require(error=="unrelated failure","Unrelated error was lost");error.clear();
    dialog.request({"Failure fixture",&doc,4,[]{return true;},[]{throw std::runtime_error("remove failed");}});warm();activate(ui::DialogAction::Confirm);
    require(dialog.pending() && error=="remove failed" && visible.find("remove failed")!=std::string::npos,"Removal failure was not isolated and propagated");activate(ui::DialogAction::Cancel);
    open();require(visible.find("remove failed")==std::string::npos,"New removal inherited its predecessor error");activate(ui::DialogAction::Cancel);
    std::cout<<"Record removal: native tissue count, Cancel/Confirm, stale document/epoch/missing record passed\n";
}
inline void run(){definitions();interactions();import_choice();record_removal();}
}
