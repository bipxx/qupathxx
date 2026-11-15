#pragma once
#include "gui_tool_scope_fix1_checks.hpp"
namespace review_fix_pass_checks {
using namespace qpx;
using Host=tool_scope_checks::Host;
inline void registration(const std::filesystem::path& dir,GLFWwindow* window){
    Host host(window);Document doc(open_tiff(tool_scope_fix1_checks::six_planes(dir).u8string()));detail::GuiLandmarkRegistration reg;auto draw=[&]{reg.draw(doc,"closed-registration",{});};
    host.frame(draw);doc.add_rectangle({0,0,.1,.1});host.frame(draw);check(reg.error().empty() && reg.source_refresh_count()==0,"Closed registration listed/captured a source or set an error");
    const auto old=tool_scope_fix1_checks::spot(doc,0,'a');host.frame(draw);reg.open();host.frame(draw);check(reg.source_refresh_count()==1 && !reg.expression_source().empty() && reg.error().empty(),"Registration did not refresh on its first open frame");
    reg.set_landmarks({{"A",{.5,.5},{.6,.6}}},{LandmarkModel::Translation});tool_scope_checks::click(host,"Fit preview",draw);const auto fit=reg.fit();check(bool(fit),reg.error());const auto count=reg.source_refresh_count();reg.close();
    doc.remove_annotation(old);tool_scope_fix1_checks::spot(doc,0,'d');host.frame(draw);tool_scope_checks::view(doc,1);host.frame(draw);tool_scope_checks::view(doc,0);host.frame(draw);
    check(reg.source_refresh_count()==count && reg.error().empty() && reg.fit()==fit && reg.landmarks().size()==1,"Closed registration did work or lost its stashed pair/fit");reg.open();host.frame(draw);
    check(reg.source_refresh_count()==count+1 && reg.expression_source()==ExpressionDataset::sources(doc,doc.active_plane()).at(0) && reg.fit()==fit,"Registration reopen missed the current source or lost fit evidence");
    std::cout<<"Registration closed: zero source refreshes/errors; first-open refresh; closed plane round-trip retains pair/fit passed\n";
}
inline void genes(const std::filesystem::path& dir,GLFWwindow* window){
    Host host(window);auto doc=std::make_shared<Document>(open_tiff(tool_scope_fix1_checks::six_planes(dir).u8string()));auto project=std::make_shared<Project>();project->add_document(doc,"Genes",(dir/"six.ome.tif").u8string());auto id=tool_scope_fix1_checks::spot(*doc,0,'a');
    detail::GuiGeneDisplay genes;GeneViewOptions options;options.enabled=true;options.genes={"G","G"};genes.load_preference(gene_workspace_preference(options));auto draw=[&]{genes.update(*doc,true,project.get(),1);genes.draw({},project.get(),1);};host.fit("Gene expression",700,1200,draw);host.until(draw,[&]{return genes.settled() && genes.session().result();},"Gene seed timeout");
    host.click(genes.molecular_bounds(detail::GuiGeneDisplay::Action::MolecularTab),draw);host.settle(draw);
    auto cleared=[&](const char* why){host.until(draw,[&]{return genes.settled();},"Gene clear timeout");check(!genes.error().empty() && !genes.session().dataset() && !genes.session().result(),std::string(why)+" kept the old gene dataset/result");for(auto action:{detail::GuiGeneDisplay::Action::Raw,detail::GuiGeneDisplay::Action::Normalized,detail::GuiGeneDisplay::Action::Keep})check(!genes.molecular_enabled(action),std::string(why)+" left a molecular action enabled");};
    doc->remove_annotation(id);cleared("Missing source");id=tool_scope_fix1_checks::spot(*doc,0,'a');host.until(draw,[&]{return genes.settled() && genes.session().result();},"Gene rebind timeout");
    tool_scope_fix1_checks::spot(*doc,0,'a');cleared("Capture exception");
    std::cout<<"Gene missing-source and capture-exception paths: dataset/result cleared; raw/normalized/Keep disabled passed\n";
}
inline void feedback(GLFWwindow* window){
    Host host(window);detail::AnalysisInputFeedback feedback;using Consumer=detail::InputConsumer;using Editor=detail::AnalysisInputFeedback::Editor;feedback.begin(Editor::Region);feedback.record(Consumer::Tissue,7);
    auto draw=[&]{ImGui::Begin("Feedback lifetime");feedback.draw(Editor::Region);ImGui::End();};const auto sentence=ui::i18n::text(detail::analysis_input_consumers[1].cancelled);
    check(model_safety_checks::logged(host,draw).find(sentence)!=std::string::npos,"Editor cancellation list never showed the active Tissue sentence");feedback.observe(Consumer::Tissue,8);
    check(feedback.last_edit[1] && !feedback.cancelled[1].active && model_safety_checks::logged(host,draw).find(sentence)==std::string::npos,"Editor cancellation list outlived the new native task generation");
    std::cout<<"Input editor list: active Tissue sentence shown; observe(new generation) removes it without changing edit history passed\n";
}
inline void labels(){
    const std::vector<std::string> pieces={"","A","-",u8"Ω",std::string(128,'r'),std::string("R\0x",3)};std::size_t count=0;
    for(const auto& row:pieces)for(const auto& column:pieces){const auto expected=row+"-"+column;for(const auto& name:std::vector<std::string>{expected,expected+"X","X"+expected,""}){check(detail::tma_label_name_matches(name,row,column)==(name==expected),"TMA name pieces differ from concatenation");++count;}}
    std::cout<<"TMA label comparison: "<<count<<" exact empty/hyphen/Unicode/long/NUL cases passed; comparison uses string_view pieces\n";
}
inline void references(const std::filesystem::path& dir,GLFWwindow* window){
    Host host(window);auto doc=record_fixture::source(dir);const auto ids=record_fixture::seed(*doc,dir);detail::GuiDarkFlat dark;detail::GuiLinearUnmixing unmix,replay;
    auto draw=[&]{dark.draw(*doc);unmix.draw(*doc);replay.poll(*doc);};host.settle(draw);unmix.set_saved_record(ids.unmix);replay.restore(*doc,ids.unmix);host.until(draw,[&]{return !replay.busy();},"Hidden replay preparation timeout");doc->add_rectangle({0,0,1,1});host.frame(draw);
    check(dark.reference_checks()==0 && unmix.reference_checks()==0 && replay.reference_checks()==0,"Closed correction/unmixing controllers parsed reference records");dark.open();unmix.open();host.frame(draw);
    check(dark.reference_checks()==1 && unmix.reference_checks()==1 && host.action("Remove correction recipe").enabled && host.action("Remove unmixing recipe").enabled,"First open did not evaluate current unreferenced recipes");dark.close();unmix.close();
    auto correction=replay_dark_flat_recipe(*doc,ids.correction);auto matrix=load_linear_unmix_recipe(*doc,ids.unmix).matrix;matrix.input_domain=correction.source()->domain();keep_linear_unmix_recipe(*doc,LinearUnmixRequest(*doc,correction,matrix));
    MarkerRule rule;rule.domain="qpx.linear-unmixed-samples/1";rule.unmix_record=ids.unmix;rule.gates={{"DNA",0}};rule.nodes={{MarkerOperation::Gate,0,{}}};keep_marker_rule(*doc,rule);host.frame(draw);
    check(dark.reference_checks()==1 && unmix.reference_checks()==1 && replay.reference_checks()==0,"Closed edit recomputed a recipe dependency");dark.open();unmix.open();host.frame(draw);
    for(const auto* name:{"Remove correction recipe","Remove unmixing recipe"})check(host.action(name).disabled && host.action(name).reason==ui::i18n::text("This recipe is used by kept results."),"Reopened Remove missed its current reference reason");
    check(dark.reference_checks()==2 && unmix.reference_checks()==2 && replay.reference_checks()==0,"Reopen reference counts or hidden replay gating differ");std::cout<<"Recipe references: closed/replay zero queries; first-open refresh and referenced Remove reason current passed\n";
}
inline void run(const std::filesystem::path& directory){
    const auto dir=std::filesystem::absolute(directory).lexically_normal();std::filesystem::create_directories(dir);labels();check(glfwInit()!=0,"Review fix GLFW");glfwWindowHint(GLFW_VISIBLE,GLFW_FALSE);auto* window=glfwCreateWindow(2200,1600,"Review fix checks",nullptr,nullptr);check(window,"Review fix window");glfwMakeContextCurrent(window);
    registration(dir,window);genes(dir,window);feedback(window);references(dir,window);glfwDestroyWindow(window);glfwTerminate();
}
}
