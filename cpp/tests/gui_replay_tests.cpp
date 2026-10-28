#include "../gui_replay.hpp"
#include <iostream>
#include <stdexcept>
#ifdef QPX_GUI_TEXT_INPUT_TESTS
#include <imgui.h>
#endif
void check(bool value) {if(!value) throw std::runtime_error("replay check failed");}
template<class Exception,class Function> void rejects(Function action) {try {action();} catch(const Exception&) {return;}throw std::logic_error("missing replay rejection");}
#ifdef QPX_GUI_TEXT_INPUT_TESTS
void unicode_text_input() {
    struct Sample {const char* utf8;const char16_t* utf16;const char32_t* utf32;};
    const Sample samples[]={
        {"ASCII_rename-123",u"ASCII_rename-123",U"ASCII_rename-123"},
        {u8"\u539f\u59cb\u96ea",u"\u539f\u59cb\u96ea",U"\u539f\u59cb\u96ea"},
        {u8"A\U0002000b\U0001f52c\u96ea",u"A\U0002000b\U0001f52c\u96ea",U"A\U0002000b\U0001f52c\u96ea"},
        {u8"e\u0301_\U0001f469\u200d\U0001f52c",u"e\u0301_\U0001f469\u200d\U0001f52c",U"e\u0301_\U0001f469\u200d\U0001f52c"}};
    for(const auto& sample:samples) for(int mode=0;mode<3;++mode) {
        IMGUI_CHECKVERSION();
        struct Context {ImGuiContext* value=ImGui::CreateContext();~Context(){ImGui::DestroyContext(value);}} context;
        auto& io=ImGui::GetIO();io.IniFilename=nullptr;io.LogFilename=nullptr;
        io.DisplaySize={640,480};io.DeltaTime=1.f/60;io.ConfigInputTrickleEventQueue=false;
        unsigned char* alpha=nullptr;int width=0,height=0;io.Fonts->GetTexDataAsAlpha8(&alpha,&width,&height);
        char name[4097]={};bool active=false;
        auto editor=[&](bool focus) {
            ImGui::Begin("Unicode name input");if(focus) ImGui::SetKeyboardFocusHere();
            ImGui::InputText("Name",name,sizeof(name));active=ImGui::IsItemActive();ImGui::End();
        };
        for(int frame=0;frame<3;++frame) {ImGui::NewFrame();editor(frame==0);ImGui::Render();}
        check(active);
        if(mode==0) io.AddInputCharactersUTF8(sample.utf8);
        else if(mode==1) for(const auto c:std::u16string(sample.utf16)) io.AddInputCharacterUTF16(static_cast<ImWchar16>(c));
        else for(const auto c:std::u32string(sample.utf32)) io.AddInputCharacter(static_cast<unsigned int>(c));
        ImGui::NewFrame();editor(false);ImGui::Render();
        if(std::string(name)!=sample.utf8) throw std::runtime_error("Unicode scalar input changed in active name editor");
        qpx::Document doc(qpx::Image::demo());const auto id=doc.add_rectangle({1,2,30,20});
        doc.rename_annotation(id,"Original");doc.rename_annotation(id,name);check(doc.annotation(id).name==sample.utf8);
        check(doc.undo() && doc.annotation(id).name=="Original");
        check(doc.redo() && doc.annotation(id).name==sample.utf8);
    }
    std::cout<<"GUI Unicode scalar input and native undo/redo passed\n";
}
#endif
int main() {
    try {
        using namespace qpx;using Clock=GuiReplayDeadline::Clock;
#ifdef QPX_GUI_TEXT_INPUT_TESTS
        unicode_text_input();
#else
        std::cout<<"GUI Unicode scalar input disabled (GUI unavailable)\n";
#endif
        GuiOptions aggregate{1440,900,DisplaySettings{}};
        check(aggregate.width==1440 && aggregate.height==900 && aggregate.replay_timeout_seconds==5 && !aggregate.settle_tiles);
        GuiInput down;down.frame=2;down.kind=GuiInputKind::LeftDown;down.target=GuiInputTarget::DiscardClose;down.wait_for=GuiInputWait::TargetReady;
        GuiInput up=down;up.frame=3;up.kind=GuiInputKind::LeftUp;up.wait_for=GuiInputWait::Immediate;
        GuiInput key;key.frame=3;key.kind=GuiInputKind::SaveKey;
        GuiReplay replay({key,down,up},1);std::vector<GuiInputKind> emitted;
        for(int physical=0;physical<7;++physical) {
            replay.dispatch([&](const auto&){return physical>=5;},[&](const auto& input){emitted.push_back(input.kind);},Clock::time_point{}+std::chrono::milliseconds(physical*10));
            replay.finish_frame();
        }
        check(replay.complete() && replay.waits()==3 && replay.frame()==4);
        // Stable ordering on one frame matches source order, including unsorted input.
        check(emitted==std::vector<GuiInputKind>{GuiInputKind::LeftDown,GuiInputKind::SaveKey,GuiInputKind::LeftUp});
        replay.dispatch([](const auto&){return false;},[&](const auto&){throw std::logic_error("duplicate event");},Clock::now());
        replay.finish_frame(false);check(replay.frame()==4);
        GuiReplay timeout({down},.1);
        auto never=[](const auto&){return false;};auto discard=[](const auto&){};
        timeout.finish_frame();timeout.finish_frame();
        timeout.dispatch(never,discard,Clock::time_point{});
        rejects<std::runtime_error>([&]{timeout.dispatch(never,discard,Clock::time_point{}+std::chrono::milliseconds(100));});
        GuiReplayDeadline deadline(.1);deadline.check(Clock::time_point{},"uploads");
        deadline.check(Clock::time_point{}+std::chrono::milliseconds(99),"uploads");
        rejects<std::runtime_error>([&]{deadline.check(Clock::time_point{}+std::chrono::milliseconds(100),"uploads");});
        deadline.reset();deadline.check(Clock::time_point{}+std::chrono::seconds(2),"uploads");
        rejects<std::invalid_argument>([]{GuiReplay bad({},0);});
        rejects<std::invalid_argument>([]{GuiReplayDeadline bad(61);});
        down.target=GuiInputTarget::Canvas;rejects<std::invalid_argument>([&]{GuiReplay bad({down},1);});
        down.target=GuiInputTarget::DiscardClose;down.kind=GuiInputKind::LeftUp;rejects<std::invalid_argument>([&]{GuiReplay bad({down},1);});
        down.wait_for=static_cast<GuiInputWait>(99);rejects<std::invalid_argument>([&]{GuiReplay bad({down},1);});
        // Legacy events do not query readiness and retain their physical frame schedule.
        GuiReplay legacy({up,key},1);std::size_t count=0;
        for(int i=0;i<5;++i) {legacy.dispatch([](const auto&)->bool{throw std::logic_error("legacy wait");},[&](const auto&){++count;},Clock::time_point{});legacy.finish_frame();}
        check(legacy.frame()==5 && legacy.waits()==0 && legacy.complete() && count==2);

        GuiInput resize;resize.kind=GuiInputKind::ResizeWindow;resize.x=1000;resize.y=600;
        GuiReplay size_ok({resize},1);
        for(double bad:{0.,639.,8193.,1000.5,std::numeric_limits<double>::infinity(),std::numeric_limits<double>::quiet_NaN()}) {
            auto invalid=resize;invalid.x=bad;rejects<std::invalid_argument>([&]{GuiReplay reject({invalid},1);});
        }
        GuiInput minimize;minimize.kind=GuiInputKind::MinimizeWindow;minimize.frame=2;
        GuiInput restore;restore.kind=GuiInputKind::RestoreWindow;restore.frame=5;
        auto after=key;after.frame=6;after.wait_for=GuiInputWait::WindowReady;
        GuiReplay lifecycle({minimize,restore,after},1);
        auto invalid=key;invalid.frame=3;
        rejects<std::invalid_argument>([&]{GuiReplay reject({minimize,invalid,restore},1);});
        restore.wait_for=GuiInputWait::WindowReady;
        rejects<std::invalid_argument>([&]{GuiReplay reject({restore},1);});
        const auto win=GuiScale::from(2,1,1),retina=GuiScale::from(2,2,1),override_scale=GuiScale::from(2,2,1.5);
        check(win.logical==2 && win.framebuffer==1 && retina.logical==1 && retina.framebuffer==2 && override_scale.logical==1.5f);
        check(GuiScale::from(0,0,1).same_fonts(GuiScale{}));
        check(!win.same_fonts(retina) && retina.same_fonts(GuiScale::from(2,2,1)));
        GuiInput scale;scale.kind=GuiInputKind::SetUiScale;scale.x=1.5;GuiReplay scale_ok({scale},1);
        for(double bad:{0.,.49,3.01,std::numeric_limits<double>::infinity(),std::numeric_limits<double>::quiet_NaN()}) {
            scale.x=bad;rejects<std::invalid_argument>([&]{GuiReplay reject({scale},1);});
        }
        for(auto target:{GuiInputTarget::TracePixelContours,GuiInputTarget::PublishPixelContours,GuiInputTarget::KeepPixelTraining,GuiInputTarget::KeepPixelPrediction,GuiInputTarget::UsePixelRecordModel,GuiInputTarget::PreviewPixelRecord,GuiInputTarget::RemovePixelRecord,GuiInputTarget::ExportPixelStream,GuiInputTarget::OpenPixelStreamBrowser,GuiInputTarget::OpenPixelStreamFile,GuiInputTarget::ReadPixelStreamBlock,GuiInputTarget::PreviousPixelStreamBlock,GuiInputTarget::NextPixelStreamBlock,GuiInputTarget::VerifyPixelStreamFile,GuiInputTarget::CancelPixelStreamRead,GuiInputTarget::ClosePixelStreamFile,GuiInputTarget::KeepPixelStreamLink,GuiInputTarget::OpenPixelStreamRecord,GuiInputTarget::RelinkPixelStreamRecord,GuiInputTarget::RemovePixelStreamRecord}) {
            GuiInput click;click.kind=GuiInputKind::LeftDown;click.target=target;click.wait_for=GuiInputWait::TargetReady;
            GuiReplay accepted({click},1);
        }
        for(auto condition:{GuiInputWait::PixelContoursReady,GuiInputWait::PixelContoursPublished,GuiInputWait::PixelRecordKept,GuiInputWait::PixelRecordModelLoaded,GuiInputWait::PixelHistoricalPreviewReady,GuiInputWait::PixelStreamFinished,GuiInputWait::PixelStreamReadReady,GuiInputWait::PixelStreamReadFinished,GuiInputWait::PixelStreamVerified,GuiInputWait::PixelStreamLinked}) {
            GuiInput wait;wait.kind=GuiInputKind::Move;wait.wait_for=condition;GuiReplay accepted({wait},1);
        }
        for(auto target:{GuiInputTarget::OverviewBudgetIncrease,GuiInputTarget::OverviewBudgetDecrease}) {
            GuiInput click;click.kind=GuiInputKind::LeftDown;click.target=target;click.wait_for=GuiInputWait::TargetReady;
            GuiReplay accepted({click},1);
        }
        std::cout<<"gui replay passed\n";
    } catch(const std::exception& e) {std::cerr<<e.what()<<'\n';return 1;}
}
