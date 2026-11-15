#pragma once
#include "gui_record_list_checks.hpp"
#include <tuple>
namespace qpx::detail {
struct ModelSafetyTestAccess {
    static bool unsaved(const GuiPixelClassifier& p){return p.model_unsaved_;}
    static bool unsaved(const GuiRawPixel& p){return p.model_unsaved_;}
    static auto& options(GuiPixelClassifier& p){return p.options_;}
    static auto& parameters(GuiPixelClassifier& p){return p.parameters_;}
    static auto& options(GuiRawPixel& p){return p.options_;}
    static void scales(GuiPixelClassifier& p){p.scales_text();}
    static auto& jobs(GuiRawPixel& p){return p.jobs_;}
    static void select(GuiPixelClassifier& p,std::uint64_t id){p.archives_.select(id);}
    static void select(GuiRawPixel& p,std::uint64_t id){p.record_=id;}
    static auto live(const GuiPixelClassifier& p){return p.prediction_.result();}
    static bool live_request(const GuiPixelClassifier& p,const Document& doc){return p.prediction_request_ && p.prediction_request_->matches(doc);}
    static auto palette(const GuiPixelClassifier& p){return p.preview_palette();}
    static auto palette(const GuiRawPixel& p,const Document& doc){auto r=p.jobs_.report();return p.palette(doc,r.historical->class_counts(),r.historical_model);}
    static const auto& history(const GuiRawPixel& p){return p.history_;}
    static auto display_source(const GuiPixelClassifier& p){return p.preview_source();}
    static auto native_contours(const GuiPixelClassifier& p){return p.contours_.result();}
    static auto& palette_items(GuiPixelClassifier& p){return p.historical_palette_;}
    static auto opacity_bounds(const GuiPixelClassifier& p){return p.opacity_bounds_;}
    static float opacity(const GuiPixelClassifier& p){return p.opacity_;}
    static auto live_palette(const GuiPixelClassifier& p){return p.palette_;}
    static bool io(const GuiPixelClassifier& p){return p.io_.valid();}
};
}
namespace model_safety_checks {
using namespace qpx;
using A=detail::ModelSafetyTestAccess;
inline auto features(const PixelFeatureOptions& f){return std::make_tuple(f.rgb,f.optical_density,f.stains,f.gradient_magnitude,f.laplacian,f.sigmas,f.work_bytes);}
inline auto training(const PixelTrainingOptions& o){return std::make_tuple(features(o.features),o.level,o.calibrated,o.balanced,o.tile_size,o.max_samples,o.max_samples_per_class,o.work_bytes,o.seed,o.max_scanned_pixels);}
inline auto forest(const ForestParameters& p){return std::make_tuple(p.trees,p.max_depth,p.min_samples,p.active_features,p.seed,p.work_bytes);}
inline auto raw_training(const RawPixelTrainingSettings& o){
    std::vector<double> sigmas;for(auto v:o.features.sigmas){sigmas.push_back(v.x);sigmas.push_back(v.y);}
    return std::make_tuple(o.features.offsets,o.features.scales,sigmas,o.features.gradient_magnitude,o.features.laplacian,o.features.work_bytes,
        o.selection.first_channel,o.selection.channels,o.selection.z,o.selection.t,o.selection.work_bytes,
        o.sampling.max_samples,o.sampling.max_samples_per_class,o.sampling.work_bytes,o.sampling.seed,o.sampling.max_scanned_samples,o.sampling.balanced,
        o.tiles.width,o.tiles.height,o.tiles.max_windows,forest(o.forest),o.physical_sigmas,o.checkpoint_path,o.checkpoint.resume_path,
        o.checkpoint.limits.max_bytes,o.checkpoint.limits.work_bytes,o.checkpoint.max_new_windows,o.checkpoint.replace_existing,
        o.lattice?std::to_string(o.lattice->image_width())+","+std::to_string(o.lattice->image_height())+","+std::to_string(o.lattice->x().samples())+","+std::to_string(o.lattice->y().samples()):std::string{});
}
inline void mask(const detail::GuiPixelPreview& preview,const char* title="Historical mask pixels"){
    ImGui::SetNextWindowPos({1580,40},ImGuiCond_Always);ImGui::SetNextWindowSize({560,400},ImGuiCond_Always);ImGui::Begin(title);
    const auto at=ImGui::GetCursorScreenPos();preview.draw_local(ImGui::GetWindowDrawList(),at,{500,300});ImGui::Dummy({500,300});ImGui::End();
}
inline std::string logged(simplification_tools_checks::NativeUi& host,const std::function<void()>& draw) {
    std::string text;auto log=[&]{ImGui::LogToBuffer();host.context->LogWindow=nullptr;draw();text=host.context->LogBuffer.c_str();ImGui::LogFinish();};host.frame(log);return text;
}
inline int occurrences(const std::string& text,const std::string& word){int n=0;for(std::size_t i=0;(i=text.find(word,i))!=std::string::npos;i+=word.size())++n;return n;}
struct GlHost: simplification_tools_checks::NativeUi {
    GLFWwindow* window;
    explicit GlHost(GLFWwindow* w):window(w) {
        ImGui_ImplGlfw_InitForOpenGL(window,true);ImGui_ImplOpenGL3_Init("#version 330");
        int max=0;glGetIntegerv(GL_MAX_TEXTURE_SIZE,&max);ImGui::GetIO().Fonts->Clear();ImGui::GetIO().FontDefault=detail::configure_gui_fonts(*ImGui::GetIO().Fonts,max);ImGui_ImplOpenGL3_NewFrame();
        ImGui::GetIO().DisplaySize={2200,1600};
    }
    ~GlHost(){ImGui_ImplOpenGL3_Shutdown();ImGui_ImplGlfw_Shutdown();}
    void capture(const std::filesystem::path& path,const std::function<void()>& draw) {
        for(int frame=0;frame<5;++frame) {
            ImGui_ImplOpenGL3_NewFrame();ImGui_ImplGlfw_NewFrame();this->frame(draw);
            check(ui::layout_stats.overflow_items==0,"Model warning capture overflow: "+ui::layout_stats.first_overflow);
            glViewport(0,0,2200,1600);glClearColor(.04f,.04f,.05f,1);glClear(GL_COLOR_BUFFER_BIT);ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
            if(frame==4)dialog_checks::capture(path,window);glfwSwapBuffers(window);
        }
    }
};
template<class Panel,class Draw>inline void click(GlHost& host,Panel& panel,GuiInputTarget target,Draw& draw) {
    if constexpr(std::is_same_v<Panel,detail::GuiPixelClassifier>) {panel.reveal_target(target);host.settle(draw);}
    host.click(panel.bounds(target),draw);
}
template<class Panel,class Draw>inline void captures(GlHost& host,Panel& panel,Draw& draw,const std::filesystem::path& dir,const char* family,bool warning) {
    const auto base_style=ImGui::GetStyle();
    for(int locale:{0,2}) {
        ui::i18n::choose(locale);ImGui::GetStyle()=base_style;
        if(locale==2){ImGui::GetStyle().ScaleAllSizes(2);ImGui::GetStyle().FontSizeBase=32;}
        const auto title=ui::i18n::label(std::is_same_v<Panel,detail::GuiPixelClassifier>?"Pixel classifiers":"Raw pixel classifier");
        host.settle(draw);auto* w=ImGui::FindWindowByName(title.c_str());check(w,"Model capture window missing");
        ImGui::SetWindowPos(w,{24,24});ImGui::SetWindowSize(w,locale==2?ImVec2{1500,1500}:ImVec2{1000,1300});ImGui::SetScrollY(w,0);host.settle(draw);
        const auto prefix=std::string(family)+(warning?"-warning":"-kept")+(locale==2?"-fr200":"-normal");
        host.capture(dir/(prefix+"-load.ppm"),draw);
        if constexpr(std::is_same_v<Panel,detail::GuiPixelClassifier>) {
            panel.reveal_target(GuiInputTarget::UsePixelRecordModel);host.settle(draw);host.capture(dir/(prefix+"-use.ppm"),draw);
        }
    }
    ui::i18n::choose(0);ImGui::GetStyle()=base_style;host.fit(std::is_same_v<Panel,detail::GuiPixelClassifier>?"Pixel classifiers":"Raw pixel classifier",1000,1500,draw);
}
inline void pixel(const std::filesystem::path& dir,GLFWwindow* window) {
    GlHost host(window);std::vector<std::uint8_t> rgb(32*16*3);for(int y=0;y<16;++y)for(int x=0;x<32;++x){auto i=(y*32+x)*3;rgb[i]=x<16?220:30;rgb[i+1]=x<16?30:220;rgb[i+2]=50;}
    Document doc(Image(32,16,std::move(rgb)));doc.set_class("Old A",0xff0000);doc.set_class("Old B",0x0000ff);
    const auto a=doc.add_rectangle({0,0,14,16}),b=doc.add_rectangle({18,0,14,16}),target=doc.add_rectangle({0,0,32,16});doc.classify_annotation(a,"Old A");doc.classify_annotation(b,"Old B");ObjectSelection selection(doc);selection.replace({target});
    GuiOptions o;o.pixel_open=true;o.pixel_object_id=target;o.pixel_preview_budget=1024*1024;
    auto opts=std::make_shared<PixelTrainingOptions>();opts->calibrated=false;opts->features.sigmas={0};opts->max_samples_per_class=64;o.pixel_training_options=opts;
    auto params=std::make_shared<ForestParameters>();params->trees=3;params->max_depth=3;o.pixel_parameters=params;
    detail::GuiPixelClassifier panel(o);detail::OutputGate gate;std::map<ui::DialogAction,Rect> modal;
    auto draw=[&]{panel.update(doc,selection);panel.update_preview();panel.draw(doc,selection,false,true,[](GuiInputTarget){},[](bool){return StainTransform({{1,0,0},{0,1,0}});},{0,0,32,16},nullptr,0,&gate,{dir.u8string()});if(panel.historical_result())mask(panel.preview());gate.draw([](const auto&){return true;},[&](ui::DialogAction a){const auto x=ImGui::GetItemRectMin(),y=ImGui::GetItemRectMax();modal[a]={x.x,x.y,y.x-x.x,y.y-x.y};});};
    host.fit("Pixel classifiers",1000,1500,draw);
    auto train=[&]{++A::parameters(panel).seed;const auto previous=panel.model();click(host,panel,GuiInputTarget::TrainPixelClassifier,draw);host.until(draw,[&]{return panel.model()!=previous && panel.trained();},"Pixel training failed");check(A::unsaved(panel),"Pixel training did not mark session model");};
    train();click(host,panel,GuiInputTarget::KeepPixelTraining,draw);host.until(draw,[&]{return !panel.archives().busy();},"Pixel Keep training failed");check(!A::unsaved(panel),"Pixel Keep success did not clear warning");
    const auto old=panel.model();
    {auto supplied=o;auto settings=std::make_shared<PixelTrainingOptions>(*opts);settings->features.sigmas={0,2};supplied.pixel_training_options=settings;supplied.pixel_model=old;
     detail::GuiPixelClassifier initial(supplied);initial.update(doc,selection);check(features(A::options(initial).features)==features(old->options()) && !A::unsaved(initial),"Supplied Pixel model no longer initializes its feature controls");}
    click(host,panel,GuiInputTarget::PredictPixelClasses,draw);host.until(draw,[&]{return panel.predicted();},"Old Pixel prediction failed");click(host,panel,GuiInputTarget::KeepPixelPrediction,draw);host.until(draw,[&]{return !panel.archives().busy();},"Pixel Keep mask failed");const auto record=panel.archives().kept();check(doc.pixel_classifier_record(record)->archive->model()->sha256()==old->sha256(),"Kept old model identity differs");
    doc.classify_annotation(a,"New A");doc.classify_annotation(b,"New B");doc.set_class("New A",0x00ff00);doc.set_class("New B",0xffff00);
    A::options(panel).features.sigmas={0,1};A::options(panel).balanced=false;A::options(panel).seed=7;A::parameters(panel).seed=9;A::scales(panel);train();
    click(host,panel,GuiInputTarget::PredictPixelClasses,draw);host.until(draw,[&]{return panel.predicted();},"Working Pixel prediction failed");
    const auto working=panel.model();const auto sha=working->sha256();const auto options=training(A::options(panel));const auto parameters=forest(A::parameters(panel));const auto live=A::live(panel);const auto live_sha=live->sha256();
    A::select(panel,record);click(host,panel,GuiInputTarget::PreviewPixelRecord,draw);host.settle(draw);
    check(panel.model()==working && panel.model()->sha256()==sha && training(A::options(panel))==options && forest(A::parameters(panel))==parameters,"Historical Pixel preview replaced working model/options");
    check(A::live(panel)==live && A::live(panel)->sha256()==live_sha && A::live_request(panel,doc) && panel.predicted() && panel.trained(),"Historical Pixel preview changed live prediction/training state");
    check(panel.historical_result()==doc.pixel_classifier_record(record)->archive->result() && panel.historical_result()->model().classes()==old->classes(),"Pixel historical mask did not come from exact archive/classes");
    auto expected=detail::class_palette(doc,old->classes());for(auto& c:expected)c=(c&0xffffffu)|(115u<<24);check(A::palette(panel)==expected && panel.preview_ready(),"Historical Pixel palette uses working classes");
    check(occurrences(logged(host,draw),ui::i18n::text(ui::session_model_warning()))==2,"Pixel warning absent beside Load or Use");
    captures(host,panel,draw,dir,"pixel",true);
    click(host,panel,GuiInputTarget::KeepPixelTraining,draw);host.until(draw,[&]{return !panel.archives().busy();},"New Pixel Keep failed");check(!A::unsaved(panel) && occurrences(logged(host,draw),ui::session_model_warning())==0,"Pixel Keep left warning");captures(host,panel,draw,dir,"pixel",false);
    train();const auto save=dir/"pixel-model.qpc";save_pixel_classifier(*old,save.u8string());core_path_checks::find(panel.paths(),"Export pixel model file").set(save.u8string());
    click(host,panel,GuiInputTarget::SavePixelModel,draw);check(gate.pending() && A::unsaved(panel) && !A::io(panel),"Pixel Save submission cleared warning before confirmation");host.click(modal.at(ui::DialogAction::Cancel),draw);check(!gate.pending() && A::unsaved(panel) && load_pixel_classifier(save.u8string()).sha256()==old->sha256(),"Pixel overwrite Cancel lost warning or file");
    click(host,panel,GuiInputTarget::SavePixelModel,draw);check(A::unsaved(panel),"Pixel warning cleared while dialog open");host.click(modal.at(ui::DialogAction::Confirm),draw);host.until(draw,[&]{return !A::io(panel);},"Pixel Save failed");check(panel.io_state()=="saved" && !A::unsaved(panel) && load_pixel_classifier(save.u8string()).sha256()==panel.model()->sha256(),"Pixel successful Save did not clear exact model warning");
    detail::OutputGateTestAccess::clear_success(gate);train();A::select(panel,record);click(host,panel,GuiInputTarget::UsePixelRecordModel,draw);check(panel.model()==doc.pixel_classifier_record(record)->archive->model() && !A::unsaved(panel),"Use kept model no longer adopts/clears");
    train();core_path_checks::find(panel.paths(),"Pixel model file").set(save.u8string());click(host,panel,GuiInputTarget::LoadPixelModel,draw);host.until(draw,[&]{return !A::io(panel);},"Pixel Load failed");check(panel.io_state()=="loaded" && !A::unsaved(panel) && panel.model()->sha256()==load_pixel_classifier(save.u8string()).sha256(),"Pixel Load did not replace and clear");
    std::cout<<"Pixel: exact working pointer/SHA/all feature+training options/live prediction preserved; archive classes/palette; warning Keep/Cancel/Save/Use/Load; normal/French200 overflow0\n";
}
inline void raw(const std::filesystem::path& dir,GLFWwindow* window) {
    GlHost host(window);qpx_test::raw_pixel_tiff_fixture(dir/"raw.tiff",12,8);Document doc(open_tiff((dir/"raw.tiff").u8string()));
    const auto a=doc.add_rectangle({0,0,5,8}),b=doc.add_rectangle({7,0,5,8});doc.classify_annotation(a,"Old A");doc.classify_annotation(b,"Old B");doc.set_class("Old A",0xff0000);doc.set_class("Old B",0x0000ff);ObjectSelection selection(doc);
    ScientificRawLattice lattice(12,8,{0,12,12,2},{0,8,8,2});GuiOptions o;auto options=std::make_shared<GuiRawPixelOptions>();options->open=true;options->training.lattice=lattice;options->training.selection.channels=2;options->training.features.offsets={0,0};options->training.features.scales={1,1};options->training.features.sigmas={{0,0}};options->training.forest.trees=3;options->training.forest.max_depth=3;options->training.sampling.max_samples_per_class=32;o.raw_pixel=options;
    detail::GuiRawPixel panel(o);detail::OutputGate gate;std::map<ui::DialogAction,Rect> modal;
    auto draw=[&]{panel.update(doc);panel.draw(doc,selection,nullptr,false,true,[](GuiInputTarget){},nullptr,0,&gate,{dir.u8string()});if(panel.report().historical)mask(A::history(panel));gate.draw([](const auto&){return true;},[&](ui::DialogAction a){const auto x=ImGui::GetItemRectMin(),y=ImGui::GetItemRectMax();modal[a]={x.x,x.y,y.x-x.x,y.y-x.y};});};host.fit("Raw pixel classifier",1000,1500,draw);
    auto train_model=[&]{click(host,panel,GuiInputTarget::RawPixelTrainingTab,draw);const auto before=panel.report().model;click(host,panel,GuiInputTarget::RawPixelTrain,draw);host.until(draw,[&]{return !A::jobs(panel).busy();},"Raw train failed");check(panel.report().state=="trained" && panel.report().model!=before && A::unsaved(panel),"Raw train did not mark model: state="+panel.report().state+" error="+panel.report().error+" changed="+std::to_string(panel.report().model!=before)+" unsaved="+std::to_string(A::unsaved(panel)));};
    check(host.action("Keep raw training").reason==ui::i18n::text("Train a model first."),"Raw training reason suggests loading a model");
    train_model();host.settle(draw);check(host.action("Keep raw training").enabled && host.action("Keep raw training").reason.empty(),"Training did not enable Raw Keep training");
    click(host,panel,GuiInputTarget::RawPixelKeepTraining,draw);check(!A::unsaved(panel),"Raw Keep did not clear warning");const auto old=panel.report().model;
    const auto file=dir/"old.qws";save_raw_pixel_stream(RawPixelTiledPredictionRequest(doc,*old,lattice),file.u8string());const auto record=doc.keep_raw_pixel_stream(prepare_raw_pixel_stream_link(file.u8string()),"Old mask");
    doc.classify_annotation(a,"New A");doc.classify_annotation(b,"New B");A::options(panel).training.features.gradient_magnitude=true;A::options(panel).training.forest.seed=8;train_model();
    click(host,panel,GuiInputTarget::RawPixelFilesTab,draw);click(host,panel,GuiInputTarget::RawPixelPredict,draw);host.until(draw,[&]{return !A::jobs(panel).busy();},"Raw live prediction failed");check(bool(panel.report().prediction),"Raw live prediction absent");
    const auto working=panel.report().model;const auto sha=working->sha256();const auto recipe=working->recipe().bytes();const auto live=panel.report().prediction;const auto parameters=raw_training(A::options(panel).training);
    auto unchanged=[&]{check(panel.report().model==working && panel.report().model->sha256()==sha && working->recipe().bytes()==recipe && panel.report().prediction==live && raw_training(A::options(panel).training)==parameters && A::options(panel).training.features.gradient_magnitude && A::unsaved(panel),"Raw history replaced working model/options/live prediction/warning");check(panel.report().historical_model->sha256()==old->sha256() && panel.report().historical_model->forest().classes()==old->forest().classes() && A::history(panel).ready(),"Raw history lost own model/classes");auto expected=detail::class_palette(doc,old->forest().classes());for(auto& c:expected)c=(c&0xffffffu)|(115u<<24);check(A::palette(panel,doc)==expected,"Raw history palette uses live classes");};
    core_path_checks::find(panel.paths(),"Open prediction file").set(file.u8string());click(host,panel,GuiInputTarget::RawPixelOpenFile,draw);host.until(draw,[&]{return !A::jobs(panel).busy();},"Raw file open failed");unchanged();
    A::select(panel,record);click(host,panel,GuiInputTarget::RawPixelRecordsTab,draw);click(host,panel,GuiInputTarget::RawPixelOpenRecord,draw);host.until(draw,[&]{return !A::jobs(panel).busy();},"Raw record open failed");unchanged();check(std::filesystem::equivalent(std::filesystem::u8path(panel.report().link->path()),file),"Raw record opened wrong file");
    click(host,panel,GuiInputTarget::RawPixelFilesTab,draw);check(occurrences(logged(host,draw),ui::session_model_warning())==1,"Raw Load warning missing");captures(host,panel,draw,dir,"raw",true);
    click(host,panel,GuiInputTarget::RawPixelTrainingTab,draw);click(host,panel,GuiInputTarget::RawPixelKeepTraining,draw);check(!A::unsaved(panel),"Raw Keep left warning");click(host,panel,GuiInputTarget::RawPixelFilesTab,draw);check(occurrences(logged(host,draw),ui::session_model_warning())==0,"Raw warning visible after Keep");captures(host,panel,draw,dir,"raw",false);
    train_model();click(host,panel,GuiInputTarget::RawPixelFilesTab,draw);const auto save=dir/"raw-model.qrm";save_raw_pixel_classifier(*old,save.u8string());core_path_checks::find(panel.paths(),"Export model path").set(save.u8string());
    click(host,panel,GuiInputTarget::RawPixelSaveModel,draw);check(gate.pending() && A::unsaved(panel) && !A::jobs(panel).busy(),"Raw Save submission cleared warning");host.click(modal.at(ui::DialogAction::Cancel),draw);check(!gate.pending() && A::unsaved(panel) && load_raw_pixel_classifier(save.u8string()).sha256()==old->sha256(),"Raw overwrite Cancel lost warning/file");
    click(host,panel,GuiInputTarget::RawPixelSaveModel,draw);check(A::unsaved(panel),"Raw warning cleared in overwrite dialog");host.click(modal.at(ui::DialogAction::Confirm),draw);host.until(draw,[&]{return !A::jobs(panel).busy();},"Raw Save failed");check(panel.report().state=="saved" && !A::unsaved(panel) && load_raw_pixel_classifier(save.u8string()).sha256()==panel.report().model->sha256(),"Raw Save did not clear exact saved model warning");detail::OutputGateTestAccess::clear_success(gate);
    train_model();click(host,panel,GuiInputTarget::RawPixelFilesTab,draw);core_path_checks::find(panel.paths(),"Model path").set(save.u8string());click(host,panel,GuiInputTarget::RawPixelLoadModel,draw);host.until(draw,[&]{return !A::jobs(panel).busy();},"Raw Load failed");check(panel.report().state=="loaded" && !A::unsaved(panel) && panel.report().model->sha256()==load_raw_pixel_classifier(save.u8string()).sha256(),"Raw Load did not replace/clear");
    std::cout<<"Raw: file+record history already separate; exact working pointer/SHA/recipe/options/live prediction preserved; own historical classes/palette; warning Keep/Cancel/Save/Load; normal/French200 overflow0\n";
}
inline void run(const std::filesystem::path& directory) {
    const auto dir=std::filesystem::absolute(directory).lexically_normal();
    check(has_classifier_training(),"Model safety requires retained training support");check(glfwInit()!=0,"Model safety GLFW");glfwWindowHint(GLFW_VISIBLE,GLFW_FALSE);glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR,3);glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR,3);
    auto* window=glfwCreateWindow(2200,1600,"Model safety",nullptr,nullptr);check(window,"Model safety window");glfwMakeContextCurrent(window);
    pixel(dir,window);raw(dir,window);glfwDestroyWindow(window);glfwTerminate();
}
}
