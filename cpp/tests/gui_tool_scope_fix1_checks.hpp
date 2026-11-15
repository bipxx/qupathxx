#pragma once
#include "gui_tool_scope_checks.hpp"
namespace tool_scope_fix1_checks {
using namespace qpx;
using Host=tool_scope_checks::Host;
inline std::filesystem::path six_planes(const std::filesystem::path& dir){
    const auto file=dir/"six.ome.tif";
    const std::string xml="<OME xmlns=\"http://www.openmicroscopy.org/Schemas/OME/2016-06\"><Image ID=\"Image:0\"><Pixels ID=\"Pixels:0\" DimensionOrder=\"XYCZT\" Type=\"uint8\" SizeX=\"2\" SizeY=\"2\" SizeC=\"1\" SizeZ=\"6\" SizeT=\"1\"><Channel ID=\"Channel:0\" SamplesPerPixel=\"1\"/><TiffData IFD=\"0\" PlaneCount=\"6\"/></Pixels></Image></OME>";
    const unsigned description=8+6*150,pixels=description+unsigned(xml.size()+1);std::vector<unsigned char> bytes(pixels+24,80);
    auto u16=[&](unsigned at,unsigned v){bytes[at]=v&255;bytes[at+1]=(v>>8)&255;};auto u32=[&](unsigned at,unsigned v){for(unsigned i=0;i<4;++i)bytes[at+i]=(v>>(8*i))&255;};
    bytes[0]=bytes[1]='I';u16(2,42);u32(4,8);std::memcpy(bytes.data()+description,xml.c_str(),xml.size()+1);
    for(unsigned p=0;p<6;++p){const unsigned at=8+150*p;unsigned n=0;auto tag=[&](unsigned id,unsigned type,unsigned count,unsigned value){const unsigned t=at+2+12*n++;u16(t,id);u16(t+2,type);u32(t+4,count);u32(t+8,value);};
        tag(256,4,1,2);tag(257,4,1,2);tag(258,3,1,8);tag(259,3,1,1);tag(262,3,1,1);if(!p)tag(270,2,unsigned(xml.size()+1),description);tag(273,4,1,pixels+4*p);tag(277,3,1,1);tag(278,4,1,2);tag(279,4,1,4);tag(284,3,1,1);tag(339,3,1,1);u16(at,n);u32(at+2+12*n,p==5?0:at+150);}
    std::ofstream out(file,std::ios::binary);out.write(reinterpret_cast<const char*>(bytes.data()),bytes.size());out.close();check(bool(out),"Six-plane fixture write failed");return file;
}
inline std::uint64_t spot(Document& doc,unsigned z,char identity){
    tool_scope_checks::view(doc,z);const auto id=doc.add_detection(Roi::points({{.5,.5}}),"Spot");
    doc.set_object_properties(id,std::make_shared<ObjectProperties>(std::map<std::string,std::string>{{"omics.profile","qpx.spatial-counts/1"},{"omics.matrix_sha256",std::string(64,identity)},{"omics.features_sha256",std::string(64,'b')},{"omics.positions_sha256",std::string(64,'c')},{"omics.observation_id",std::string("spot-")+identity+std::to_string(z)}},std::map<std::string,double>{{"omics.count:G",2}}));return id;
}
inline void genes_source(const std::filesystem::path& dir,GLFWwindow* window){
    Host host(window);Document doc(open_tiff(six_planes(dir).u8string()));spot(doc,0,'a');const auto expected=ExpressionDataset::sources(doc,doc.active_plane()).at(0);
    detail::GuiGeneDisplay genes;GeneViewOptions options;options.enabled=true;options.genes={"G","G"};genes.load_preference(gene_workspace_preference(options,std::string(64,'f')));
    auto draw=[&]{genes.update(doc,true);genes.draw({});};host.settle(draw);host.until(draw,[&]{return genes.settled();},"Preference bind timeout");
    const auto actual=parse_gene_workspace_preference(genes.preference()).source_id;std::cout<<"Gene missing preference source: selected="<<actual<<" expected="<<expected<<" error="<<genes.error()<<'\n';
    check(actual==expected && genes.session().dataset() && genes.error().empty(),"Missing preference source did not fall back to the single source");
    std::cout<<"Gene missing preference source: single source bound with no error passed\n";
}
inline void registration_source(const std::filesystem::path& dir,GLFWwindow* window){
    Host host(window);Document doc(open_tiff(six_planes(dir).u8string()));const auto old=spot(doc,0,'a');detail::GuiLandmarkRegistration reg;reg.open();auto draw=[&]{reg.draw(doc,"source-test",{});};host.settle(draw);
    const auto initial=reg.expression_source();reg.set_landmarks({{"A",{.5,.5},{.6,.6}}},{LandmarkModel::Translation});tool_scope_checks::click(host,"Fit preview",draw);const auto fit=reg.fit();check(bool(fit),reg.error());
    spot(doc,0,'d');host.settle(draw);doc.remove_annotation(old);const auto expected=ExpressionDataset::sources(doc,doc.active_plane()).at(0);host.settle(draw);
    std::cout<<"Registration disappearing source: initial="<<initial<<" selected="<<reg.expression_source()<<" expected="<<expected<<" error="<<reg.error()<<'\n';
    check(reg.expression_source()==expected && reg.error().empty() && reg.landmarks().size()==1 && reg.fit()==fit,"Missing registration source did not fall back while retaining pairs/fit");
    std::cout<<"Registration disappearing source: single source selected; pairs and fit retained passed\n";
}
inline void planes(const std::filesystem::path& dir,GLFWwindow* window){
    Host host(window);Document doc(open_tiff(six_planes(dir).u8string()));for(unsigned z=0;z<6;++z)spot(doc,z,'a');tool_scope_checks::view(doc,0);
    detail::GuiLandmarkRegistration reg;reg.open();detail::GuiGeneDisplay genes;GeneViewOptions options;options.enabled=true;options.genes={"G","G"};genes.load_preference(gene_workspace_preference(options));
    auto draw=[&]{reg.draw(doc,"plane-test",{});genes.update(doc,true);genes.draw({});};std::array<std::shared_ptr<const GeneDisplayPair>,6> results;std::size_t maximum=0,stashed=0;
    for(unsigned z=0;z<6;++z){tool_scope_checks::view(doc,z);host.until(draw,[&]{return genes.settled() && genes.session().result() && genes.session().dataset()->plane()==doc.active_plane();},"Plane result timeout");maximum=std::max(maximum,genes.retained_plane_states());results[z]=genes.session().result();reg.set_landmarks({{"P"+std::to_string(z),{.5,.5},{.6,.6}}},{LandmarkModel::Translation});tool_scope_checks::click(host,"Fit preview",draw);check(bool(reg.fit()),reg.error());stashed=std::max(stashed,reg.stashed_fit_count());check(reg.stashed_fit_count()<=3,"More than three other-plane fits were stashed");}
    std::size_t fits=0;for(int z=5;z>=0;--z){tool_scope_checks::view(doc,unsigned(z));host.settle(draw);if(reg.fit())++fits;check(reg.landmarks().size()==1 && reg.landmarks()[0].id=="P"+std::to_string(z),"Bounded cache lost landmark pairs");if(z>=2)check(genes.session().result()==results[std::size_t(z)],"Recently shown plane lost its gene result");}
    std::cout<<"Six-plane retention: maximum gene states="<<maximum<<" returning fits="<<fits<<" maximum stashed fits="<<stashed<<'\n';check(maximum<=4 && fits==4,"Plane result/fit caches exceed four planes");
    std::cout<<"Six-plane retention: recent results return, all pairs survive, four-plane bound passed\n";
}
inline void sketch(const std::filesystem::path& dir,bool points){
    const auto file=tool_scope_checks::source_file(dir,points?"points.ome.tif":"empty.ome.tif");auto project=std::make_shared<Project>();auto doc=std::make_shared<Document>(open_tiff(file.u8string()));project->add_document(doc,"Sketch",file.u8string());auto o=::options(dir,points?"sketch-points":"sketch-empty","annotations");o.width=1600;o.height=1200;o.settle_tiles=false;o.inputs={key(3,GuiInputKind::PolygonKey)};
    if(points){int f=6;for(Point p:std::vector<Point>{{.2,.2},{.7,.2},{.7,.7}}){auto e=key(f,GuiInputKind::LeftDown);e.target=GuiInputTarget::Image;e.x=p.x;e.y=p.y;o.inputs.push_back(e);e.frame++;e.kind=GuiInputKind::LeftUp;o.inputs.push_back(e);f+=3;}}
    dispatch_checks::TraceScope observed;int stage=0;bool choose=false;const auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(25);observed.workflow.trace.before_frame=[&](int){check(std::chrono::steady_clock::now()<deadline,"Sketch fix deadline");};
    observed.trace.replay_input=[&](const auto& emit){if(choose){emit(key(0,GuiInputKind::RectangleKey));choose=false;}};
    observed.trace.after_frame=[&](const auto&,const auto&){if(stage==0){if(observed.trace.tool!=3 || (points && observed.trace.draft_points!=3))return;tool_scope_checks::view(*doc,1);++stage;return;}if(stage==1){choose=true;++stage;return;}if(stage==2){if(observed.trace.tool!=1)return;tool_scope_checks::view(*doc,0);++stage;return;}std::cout<<"Sketch return: points="<<observed.trace.draft_points<<" tool="<<observed.trace.tool<<" expected="<<(points?3:1)<<'\n';check(observed.trace.tool==(points?3:1) && observed.trace.draft_points==(points?3:0),"Plane return changed a tool without a sketch or lost a real sketch");observed.trace.end_session=true;};
    run_project_gui(project,20000,"",o);check(stage==3 && observed.trace.end_session,"Sketch scene incomplete");std::cout<<(points?"Point sketch and its tool restored":"Empty plane leaves the current tool alone")<<" passed\n";
}
inline void run(const std::filesystem::path& directory,const std::string& part){
    const auto dir=std::filesystem::absolute(directory).lexically_normal();std::filesystem::create_directories(dir);
    if(part=="sketch-empty" || part=="sketch-points"){sketch(dir,part=="sketch-points");return;}
    check(glfwInit()!=0,"Scope fix GLFW");glfwWindowHint(GLFW_VISIBLE,GLFW_FALSE);auto* window=glfwCreateWindow(2200,1600,"Scope fix checks",nullptr,nullptr);check(window,"Scope fix window");glfwMakeContextCurrent(window);
    if(part=="all") {for(const auto& name:{"genes","registration","planes"}){const auto out=dir/name;std::filesystem::create_directories(out);if(std::string_view(name)=="genes")genes_source(out,window);else if(std::string_view(name)=="registration")registration_source(out,window);else planes(out,window);}std::cout<<"Scope fix all: three windowed parts passed; sketch-empty and sketch-points require their own process\n";}
    else if(part=="genes")genes_source(dir,window);else if(part=="registration")registration_source(dir,window);else if(part=="planes")planes(dir,window);else throw std::runtime_error("Unknown scope fix part");glfwDestroyWindow(window);glfwTerminate();
}
}
