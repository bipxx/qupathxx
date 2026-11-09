#include "qupathxx/core.hpp"
#include "qupathxx/project.hpp"
#include "qupathxx/gene_view.hpp"
#include "qupathxx/landmark_registration.hpp"
#include "qupathxx/object_properties.hpp"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <cmath>
#include <sstream>
#include <algorithm>
using namespace qpx;
namespace fs=std::filesystem;
void check(bool value,const char* message) {if(!value)throw std::runtime_error(message);}
std::string bytes(const fs::path& file) {std::ifstream stream(file,std::ios::binary);return {std::istreambuf_iterator<char>(stream),{}};}
void write(const fs::path& file,const std::string& text) {std::ofstream stream(file,std::ios::binary);stream<<text;check(bool(stream),"Cannot write GUI fixture");}
void preference(const fs::path& file,const GeneViewOptions& view,const std::string& source={}) {
    write(file,"; qupathxx workspace layout v1\n; qupathxx language=en\n; qupathxx theme=dark\n; qupathxx gene-view="+gene_workspace_preference(view,source)+"\n");
}
Rect comparison_bounds(const std::string& ini) {
    std::istringstream lines(ini);std::string line;bool found=false;Rect bounds{};
    while(std::getline(lines,line)) {
        if(line.rfind("[Window][",0)==0)found=line=="[Window][Gene comparison]";
        if(!found)continue;
        if(line.rfind("Pos=",0)==0) {std::replace(line.begin(),line.end(),',',' ');std::istringstream values(line.substr(4));values>>bounds.x>>bounds.y;}
        if(line.rfind("Size=",0)==0) {std::replace(line.begin(),line.end(),',',' ');std::istringstream values(line.substr(5));values>>bounds.width>>bounds.height;}
    }
    check(bounds.width>0 && bounds.height>0,"Comparison has saved usable window bounds");return bounds;
}
Point yellow_center(const Image& image,const Rect& window,unsigned pane,double sx,double sy) {
    const auto left=int((window.x+10+pane*window.width/2)*sx),right=int((window.x+(pane+1)*window.width/2-10)*sx);
    const auto top=int((window.y+70)*sy),bottom=int((window.y+window.height-70)*sy);Point sum{};std::size_t count=0;
    for(int y=top;y<bottom;++y)for(int x=left;x<right;++x) {
        check(x>=0 && y>=0 && x<image.width() && y<image.height(),"Comparison measurement crop is in framebuffer");
        const auto at=(std::size_t(y)*image.width()+x)*3;
        if(image.pixels()[at]==255 && image.pixels()[at+1]==230 && image.pixels()[at+2]==65) {sum.x+=x;sum.y+=y;++count;}
    }
    check(count>20,"Comparison native maximum-color marker is visible");return {sum.x/count,sum.y/count};
}
int main(int argc,char** argv) {try {
    Project identity;const auto root=argc>1?fs::u8path(argv[1]):fs::current_path()/(identity.identity()+"-gene-gui");fs::create_directories(root);
    auto project=std::make_shared<Project>();auto doc=std::make_shared<Document>(Image(320,240,std::vector<std::uint8_t>(320*240*3,80)));
    for(unsigned i=0;i<4;++i) {
        const auto id=doc->add_detection(Roi::points({{288.,48.+i*48.}}));
        const double a[]={0,2,10,99},b[]={10,8,0,5};
        std::map<std::string,double> counts{{"omics.count:G1",a[i]},{"omics.count:G2",b[i]}};
        if(i==3)counts.erase("omics.count:G1");
        doc->set_object_properties(id,std::make_shared<ObjectProperties>(std::map<std::string,std::string>{{"omics.profile","qpx.spatial-counts/1"},
            {"omics.matrix_sha256",std::string(64,'a')},{"omics.features_sha256",std::string(64,'b')},{"omics.positions_sha256",std::string(64,'c')},
            {"omics.observation_id","spot-"+std::to_string(i)}},counts));
    }
    project->add_document(doc);const auto file=root/"input.qpx";project->save_new(file.u8string());const auto original=bytes(file);
    ExpressionCaptureOptions capture;capture.coordinate_frame="image-level-zero-px";const auto dataset=ExpressionDataset::capture(*doc,capture);
    GeneViewOptions view;view.enabled=true;view.genes={"G1","G2"};view.radius=10;
    GuiOptions options;options.width=1200;options.height=850;options.docked_workspace=true;options.settle_tiles=true;options.autosave_interval_seconds=0;
    options.layout_path=(root/"genes.ini").u8string();preference(fs::u8path(options.layout_path),view);
    const auto first=run_project_gui(project,25,(root/"G1.ppm").u8string(),options);
    check(first.tiles_settled && !first.pending_tiles,"Expression GUI settles native work");
    const auto image=Image::read_ppm((root/"G1.ppm").u8string());
    const auto expected=gene_display_field(GeneDisplayRequest(dataset,"G1"));
    const auto canvas=first.canvas_content_bounds;
    const Viewport viewport{first.final_origin,first.final_zoom};
    for(std::size_t i=0;i<4;++i) {
        const auto point=dataset.observations()[i].position;
        const auto screen=viewport.image_to_screen(point);
        const auto logical_x=canvas.x+screen.x;
        const auto logical_y=canvas.y+screen.y;
        const auto x=static_cast<int>(std::lround(logical_x*first.framebuffer_width/options.width));
        const auto y=static_cast<int>(std::lround(logical_y*first.framebuffer_height/options.height));
        check(x>=0 && y>=0 && x<image.width() && y<image.height(),"Projected point lies in captured canvas");
        const auto at=(std::size_t(y)*image.width()+x)*3;
        for(unsigned c=0;c<3;++c)check(std::abs(int(image.pixels()[at+c])-int(expected->colors[i][c]))<=1,"Native gene color is drawn at its scientific position");
    }
    check(bytes(file)==original && ExpressionDataset::capture(*doc,capture).sha256()==dataset.sha256(),"Display does not change project bytes/counts/coordinates");
    check(bytes(fs::u8path(options.layout_path)).find("; qupathxx gene-view="+gene_workspace_preference(view,dataset.source_id()))!=std::string::npos,"Display/source choices persist in workspace only");
    const auto extra=doc->add_detection(Roi::points({{32,32}}));auto other=doc->annotation(1).properties->metadata();other["omics.matrix_sha256"]=std::string(64,'d');other["omics.observation_id"]="other-source";
    doc->set_object_properties(extra,std::make_shared<ObjectProperties>(other,std::map<std::string,double>{{"omics.count:OtherGene",99}}));capture.source_id=dataset.source_id();
    view.compare=true;view.display.transform=GeneDisplayTransform::Log1p;preference(fs::u8path(options.layout_path),view,dataset.source_id());
    const auto comparison=run_project_gui(project,25,(root/"comparison.ppm").u8string(),options);
    check(comparison.tiles_settled && comparison.final_center.x==first.final_center.x && comparison.final_center.y==first.final_center.y,"Comparison keeps main camera synchronized state separate");
    check(bytes(file)==original && ExpressionDataset::capture(*doc,capture).sha256()==dataset.sha256(),"Comparison preserves original data/project");
    const auto bounds=comparison_bounds(bytes(fs::u8path(options.layout_path)));const auto before_pan=Image::read_ppm((root/"comparison.ppm").u8string());
    GuiInput down;down.frame=5;down.kind=GuiInputKind::LeftDown;down.target=GuiInputTarget::Window;
    down.x=(bounds.x+bounds.width*.25)/options.width;down.y=(bounds.y+bounds.height*.5)/options.height;
    auto move=down;move.frame=6;move.kind=GuiInputKind::Move;move.x-=40./options.width;
    auto up=move;up.frame=7;up.kind=GuiInputKind::LeftUp;auto leave=up;leave.frame=9;leave.kind=GuiInputKind::Move;leave.x=.98;leave.y=.98;
    options.inputs={down,move,up,leave};const auto panned=run_project_gui(project,25,(root/"comparison-panned.ppm").u8string(),options);options.inputs.clear();
    const auto after_pan=Image::read_ppm((root/"comparison-panned.ppm").u8string());const auto sx=double(panned.framebuffer_width)/options.width,sy=double(panned.framebuffer_height)/options.height;
    for(unsigned pane=0;pane<2;++pane) {
        const auto a=yellow_center(before_pan,bounds,pane,sx,sy),b=yellow_center(after_pan,bounds,pane,sx,sy);
        check(std::abs((b.x-a.x)+40*sx)<2 && std::abs(b.y-a.y)<2,"Dragging either comparison canvas moves both gene maps by the same screen displacement");
    }
    check(panned.final_center.x==comparison.final_center.x && panned.final_center.y==comparison.final_center.y && bytes(file)==original,"Comparison drag preserves main camera/project");
    const auto fit=fit_landmarks({{"shift",{288,48},{248,48}}},{LandmarkModel::Translation});
    CoordinateFrame source;source.id=dataset.coordinate_frame();source.source_id=dataset.source_id();source.plane=dataset.plane();
    accept_landmark_registration(*doc,"gene-alignment",RegistrationRequest(dataset,source,image_coordinate_frame(*doc,project->identity()+":1"),fit.transform()),fit);
    view.compare=false;view.display.transform=GeneDisplayTransform::Linear;view.genes={"G1","G2"};options.inputs.clear();options.layout_path=(root/"registered-genes.ini").u8string();preference(fs::u8path(options.layout_path),view,dataset.source_id());
    const auto aligned=run_project_gui(project,25,(root/"registered-G1.ppm").u8string(),options);
    const auto registered_image=Image::read_ppm((root/"registered-G1.ppm").u8string());const Viewport registered_view{aligned.final_origin,aligned.final_zoom};
    for(std::size_t i=0;i<dataset.observations().size();++i) {
        const auto p=registered_view.image_to_screen(fit.transform().map(dataset.observations()[i].position));
        const auto x=int(std::lround((aligned.canvas_content_bounds.x+p.x)*aligned.framebuffer_width/options.width));
        const auto y=int(std::lround((aligned.canvas_content_bounds.y+p.y)*aligned.framebuffer_height/options.height));
        check(x>=0 && y>=0 && x<registered_image.width() && y<registered_image.height(),"Registered glyph lies in canvas");
        const auto at=(std::size_t(y)*registered_image.width()+x)*3;
        for(unsigned c=0;c<3;++c)check(std::abs(int(registered_image.pixels()[at+c])-int(expected->colors[i][c]))<=1,"Native gene color uses accepted registered position");
    }
    check(ExpressionDataset::capture(*doc,capture).sha256()==dataset.sha256() && bytes(file)==original,"Registered display preserves raw source and original project file");
    if(argc>2) {
        auto real=Project::open_without_journal_recovery(argv[2]);auto real_doc=real->document(1);
        const auto scopes=expression_matrix_spatial_scopes(*real_doc);
        if(!scopes.empty()) {
            check(scopes.size()==1 && scopes[0].observations==2702,"Real indexed scope contains admitted observation cohort");
            const auto original_project=bytes(fs::u8path(argv[2]));
            view.genes={"ENSMUSG00000051951","ENSMUSG00000070880"};view.radius=3;view.compare=true;view.display.transform=GeneDisplayTransform::Log1p;
            options.layout_path=(root/"real-indexed.ini").u8string();preference(fs::u8path(options.layout_path),view,scopes[0].sha256());
            const auto shown=run_project_gui(real,60,(root/"real-indexed-comparison.ppm").u8string(),options);
            check(shown.tiles_settled && shown.uploaded_tiles && !shown.pending_tiles,"Full app indexed maps, asynchronous source/catalogue/value jobs and tiles settle");
            check(bytes(fs::u8path(argv[2]))==original_project && expression_matrix_spatial_scopes(*real_doc)[0].sha256()==scopes[0].sha256(),"Real indexed display preserves project and coordinate scope");
            check(bytes(fs::u8path(options.layout_path)).find(gene_workspace_preference(view,scopes[0].sha256()))!=std::string::npos,"Real indexed gene/source choices persist only in workspace");
            write(root/"real-indexed-result.json","{\"tiles_settled\":true,\"observations\":2702,\"scope_sha256\":\""+scopes[0].sha256()+"\",\"comparison\":true,\"project_unchanged\":true}\n");
        } else {
        auto real_capture=capture;real_capture.source_id.clear();const auto actual=ExpressionDataset::capture(*real_doc,real_capture);
        view.genes={actual.features()[0],actual.features()[1]};view.radius=3;options.layout_path=(root/"real-genes.ini").u8string();preference(fs::u8path(options.layout_path),view,actual.source_id());
        run_project_gui(real,30,(root/"real-comparison.ppm").u8string(),options);
        check(ExpressionDataset::capture(*real_doc,real_capture).sha256()==actual.sha256(),"Real six-marker navigation preserves counts/coordinates");
        }
    }
    std::cout<<"Gene GUI: native colors at exact point positions, zero/missing legends, comparison, workspace persistence and unchanged project/counts passed\n";
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}}
