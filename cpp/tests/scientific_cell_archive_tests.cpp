#include "../cell_archive.hpp"
#include "../provenance_codec.hpp"
#include "../geometry_archive.hpp"
#include "../sha256.hpp"
#include "qupathxx/project_save.hpp"
#include "qupathxx/provenance.hpp"
#include "grid_cell_archive_reference.hpp"
#include <filesystem>
#include <iostream>

namespace {
using namespace qpx;
void require(bool v) {if(!v) throw std::runtime_error("QCL5 cell archive contract failed");}
template<class F> void rejects(F f) {try {f();} catch(const std::exception&) {return;}throw std::runtime_error("QCL5 malformed archive did not reject");}
std::uint64_t integer(const std::vector<std::uint8_t>& b,std::size_t at) {require(at+8<=b.size());std::uint64_t v=0;for(unsigned i=0;i<8;++i) v|=std::uint64_t(b[at+i])<<(8*i);return v;}
void integer(std::vector<std::uint8_t>& b,std::size_t at,std::uint64_t v) {require(at+8<=b.size());for(unsigned i=0;i<8;++i) b[at+i]=std::uint8_t(v>>(8*i));}
void small(std::vector<std::uint8_t>& b,std::size_t at,std::uint32_t v) {require(at+4<=b.size());for(unsigned i=0;i<4;++i) b[at+i]=std::uint8_t(v>>(8*i));}
void sign(std::vector<std::uint8_t>& bytes,const Roi& roi) {
    Sha256 hash;const auto geometry=GeometryArchiveCodec::encode(roi);hash.integer(geometry.size());hash.update(geometry.data(),geometry.size());hash.update(bytes.data(),bytes.size()-64);
    const auto digest=hash.finish();std::copy(digest.begin(),digest.end(),bytes.end()-64);
}
std::vector<std::uint8_t> impulse() {std::vector<std::uint8_t> rgb(6*9*3,255);for(int y=3;y<6;++y) for(int x=2;x<4;++x) for(int c=0;c<3;++c) rgb[(y*6+x)*3+c]=32;return rgb;}
}
void check_scientific_cell_archive() {
    using namespace qpx;
    {
        const auto& data=grid_cell_archive_reference::bytes;ImageMetadata metadata;metadata.width=1;metadata.height=1;metadata.levels={{1,1,1}};
        const auto decoded=CellArchiveCodec::decode(data,sizeof(data),Roi::rectangle({0,0,1,1}),metadata);
        const auto encoded=CellArchiveCodec::encode(*decoded);require(encoded.size()==sizeof(data) && std::equal(encoded.begin(),encoded.end(),data));
        require(decoded->calibrated_feature(1).nucleus_sample_area_um2==.125 && decoded->features()[0].nucleus_rgb[0]==32);
        std::cout<<"Independent Python struct/hashlib QCL5 bytes decode and canonical re-encode passed\n";
    }
    const StainTransform transform({{1,0,0},{0,1,0}});auto live=std::make_shared<Document>(Image(6,9,impulse()));live->set_calibration(.5,.25);
    const auto object=live->add_rectangle({0,0,6,9});const ScientificGrid grid(6,9,{0,6,3,1},{0,9,3,1});
    GridCellParameters p;p.units=GridCellUnits::Micrometres;p.sigma=0;p.log_prominence=.2;p.min_nucleus_area=.75;p.cell_expansion=.9;p.contours.work_bytes=1024*1024;
    const CellRequest request(*live,object,transform,grid,p);const auto result=detect_cells(request);require(result->features().size()==1);
    const auto bytes=CellArchiveCodec::encode(*result);require(std::string(bytes.begin(),bytes.begin()+4)==std::string("QCL\5",4));
    require(integer(bytes,4)==4 && integer(bytes,12)==1 && integer(bytes,156)==6 && integer(bytes,164)==9); // independent fixed header offsets
    auto load=[&](const auto& packet) {return CellArchiveCodec::decode(packet.data(),packet.size(),request.roi(),live->metadata());};
    const auto restored=load(bytes);require(CellArchiveCodec::encode(*restored)==bytes);
    require(restored->nuclei()->labels()==result->nuclei()->labels() && restored->cells()->labels()==result->cells()->labels());
    require(restored->nuclei()->components()[0].centroid.x==result->nuclei()->components()[0].centroid.x && restored->markers()[0].center.y==result->markers()[0].center.y);
    require(restored->calibrated_feature(1).cell_sample_area_um2==2.25 && restored->nuclei()->provenance()->effective_pixel_width_um==.5);
    require(ProvenanceCodec::encode(*restored->nuclei()->provenance())==ProvenanceCodec::encode(*result->nuclei()->provenance()));
    ProvenanceCodec::attach(restored->nuclei(),result->nuclei()->provenance());
    auto other=std::make_shared<AnalysisProvenance>(*result->nuclei()->provenance());++other->captured_unix_ms;
    rejects([&]{ProvenanceCodec::attach(restored->nuclei(),other);});rejects([&]{ProvenanceCodec::attach(restored->nuclei(),{});});
    for(const auto length:{std::size_t(0),std::size_t(4),std::size_t(511),bytes.size()-1}) rejects([&]{CellArchiveCodec::decode(bytes.data(),length,request.roi(),live->metadata());});
    auto bad=bytes;bad[bad.size()/2]^=1;rejects([&]{load(bad);});
    for(auto at:{std::size_t(4),std::size_t(12),std::size_t(28),std::size_t(108),std::size_t(156),std::size_t(204),std::size_t(220)}) {
        bad=bytes;integer(bad,at,std::numeric_limits<std::uint64_t>::max());sign(bad,request.roi());rejects([&]{load(bad);});
    }
    bad=bytes;bad[3]=6;sign(bad,request.roi());rejects([&]{load(bad);});
    const auto provenance_size=std::size_t(integer(bytes,236)),q=244+provenance_size+96;
    bad=bytes;bad[q]^=1;sign(bad,request.roi());rejects([&]{load(bad);}); // resolved step differs
    bad=bytes;integer(bad,q+32,0);sign(bad,request.roi());rejects([&]{load(bad);}); // resolved minimum differs
    const auto stats=q+48,markers=std::size_t(integer(bytes,stats+24)),count=std::size_t(integer(bytes,stats+32));
    require(count==1 && markers==1);const auto marker=stats+40+8+64,feature=marker+20*markers;
    bad=bytes;integer(bad,stats+16,26);sign(bad,request.roi());rejects([&]{load(bad);}); // peak count cannot exceed full context samples
    bad=bytes;bad[marker]=0;sign(bad,request.roi());rejects([&]{load(bad);});
    bad=bytes;integer(bad,feature,0);sign(bad,request.roi());rejects([&]{load(bad);});
    auto block=[&](std::size_t at) {return at+16+std::size_t(integer(bytes,at+8))*(integer(bytes,at)?8:4);};
    const auto nucleus=feature+160,body=block(nucleus),domain=block(body);require(integer(bytes,domain)==4);
    bad=bytes;bad[domain+8]|=1;sign(bad,request.roi());rejects([&]{load(bad);}); // context cannot become eligible
    bad=bytes;bad[domain+8+3]|=128;sign(bad,request.roi());rejects([&]{load(bad);}); // domain padding
    bad=bytes;bad[body+16+(integer(bytes,body)?4:0)]=1;sign(bad,request.roi());rejects([&]{load(bad);});
    require(integer(bytes,body)==1 && integer(bytes,body+8)==7);
    bad=bytes;small(bad,body+16,8);small(bad,body+32,3);sign(bad,request.roi());
    bool historical_rejection=false;try {load(bad);} catch(const std::invalid_argument& error) {historical_rejection=std::string(error.what()).find("historical metric recipe")!=std::string::npos;}
    require(historical_rejection); // same counts and valid ROI, wrong body geometry
    bad=bytes;bad.insert(bad.end()-64,0);sign(bad,request.roi());rejects([&]{load(bad);});
    // Re-signed, internally valid QPV calibration still must agree with the
    // requested/resolved physical recipe stored next to it.
    other=std::make_shared<AnalysisProvenance>(*result->nuclei()->provenance());other->effective_pixel_width_um=.6;
    const auto altered=ProvenanceCodec::encode(*other);require(altered.size()==provenance_size);
    bad=bytes;std::copy(altered.begin(),altered.end(),bad.begin()+244);sign(bad,request.roi());rejects([&]{load(bad);});
    std::atomic<bool> cancelled{true};rejects([&]{CellArchiveCodec::decode(bytes.data(),bytes.size(),request.roi(),live->metadata(),&cancelled);});rejects([&]{CellArchiveCodec::encode(*result,&cancelled);});
    const auto record=live->keep_cells(request,result);require(live->cell_status(record)=="current");
    rejects([&]{live->keep_cells(request,restored);});Document copy=*live;require(copy.cell_status(record)=="unverified");
    const auto replay=CellRequest::replay(copy,record);require(copy.verify_cells(record,replay,*detect_cells(replay)) && copy.cell_status(record)=="current");
    auto different=p;different.cell_expansion=1.1;const CellRequest wrong(copy,object,transform,grid,different);rejects([&]{copy.verify_cells(record,wrong,*detect_cells(wrong));});
    if(has_geos()) {
        const auto ids=copy.publish_cell_record(record);require(ids.size()==1 && copy.cell_object_status(ids[0])=="current");
        require(copy.cell_object_feature(ids[0]).cell_pixels==3 && copy.undo() && copy.redo());
    }
    copy.set_calibration(1,1);require(copy.cell_status(record)=="calibration_changed");rejects([&]{CellRequest::replay(copy,record);});
    if(has_project()) {
        auto project=std::make_shared<Project>();const auto image=project->add_document(live);
        const auto path=(std::filesystem::current_path()/(project->identity()+"-grid-cell.qpxproj")).u8string();project->save(path);
        const auto opened=Project::open(path);const auto document=opened->document(image);const auto historical=document->cell_record(record)->result;
        require(document->cell_status(record)=="unverified" && CellArchiveCodec::encode(*historical)==bytes);
        require((!has_geos() || historical->nucleus_contours()) && historical->calibrated_feature(1).cell_sample_area_um2==2.25);
        const auto again=CellRequest::replay(*document,record);require(document->verify_cells(record,again,*detect_cells(again)) && document->cell_status(record)=="current");
        opened->save(path);const auto twice=Project::open(path)->document(image);require(CellArchiveCodec::encode(*twice->cell_record(record)->result)==bytes);
        std::filesystem::remove(std::filesystem::u8path(path));
        std::cout<<"QCL5 SQLite save, reopen, provenance attachment, replay and second save passed\n";
    }
    {
        Document empty(Image(1,1,{255,255,255}));const auto id=empty.add_rectangle({0,0,1,1});GridCellParameters unknown;
        unknown.sigma=0;unknown.min_nucleus_area=0;unknown.max_nucleus_area=0;unknown.cell_expansion=0;unknown.contours.work_bytes=1024*1024;
        const auto r=detect_cells(CellRequest(empty,id,transform,ScientificGrid(1,1,{0,1,1},{0,1,1}),unknown));
        const auto packet=CellArchiveCodec::encode(*r);const auto decoded=CellArchiveCodec::decode(packet.data(),packet.size(),r->nuclei()->roi(),empty.metadata());
        require(decoded->features().empty() && !decoded->nuclei()->provenance()->effective_pixel_width_um && CellArchiveCodec::encode(*decoded)==packet);
    }
    std::vector<Roi> shapes{Roi::ellipse({0,0,6,9})};
    if(has_geos()) shapes.push_back(Roi::polygon({{0,0},{6,0},{6,9},{0,9}},{{{.1,.1},{1.9,.1},{1.9,2.9},{.1,2.9}}}));
    for(const auto& roi:shapes) {
        auto shaped=std::make_shared<Document>(Image(6,9,impulse()));shaped->set_calibration(.5,.25);const auto id=shaped->add_roi(roi);
        auto options=p;options.sampling_policy=RoiSamplingPolicy::PolygonWinding;const CellRequest input(*shaped,id,transform,grid,options);const auto detected=detect_cells(input);
        require(detected->features().size()==1);const auto packet=CellArchiveCodec::encode(*detected);const auto geometry=GeometryArchiveCodec::encode(roi);
        const auto reconstructed=GeometryArchiveCodec::decode(geometry.data(),geometry.size());
        const auto decoded=CellArchiveCodec::decode(packet.data(),packet.size(),reconstructed,shaped->metadata());require(CellArchiveCodec::encode(*decoded)==packet);
        const auto run=shaped->keep_cells(input,detected);
        if(has_project()) {
            auto project=std::make_shared<Project>();const auto image=project->add_document(shaped);
            const auto path=(std::filesystem::current_path()/(project->identity()+"-grid-shape.qpxproj")).u8string();project->save(path);
            const auto reopened=Project::open(path)->document(image);require(CellArchiveCodec::encode(*reopened->cell_record(run)->result)==packet);
            const auto replay=CellRequest::replay(*reopened,run);require(reopened->verify_cells(run,replay,*detect_cells(replay)));
            std::filesystem::remove(std::filesystem::u8path(path));
        }
    }
    if(has_geos()) std::cout<<"QCL5 analytic ellipse, polygon hole and winding-policy geometry persistence passed\n";
    else std::cout<<"QCL5 analytic ellipse and winding-policy geometry persistence passed; GEOS polygon case unavailable\n";
    std::cout<<"QCL5 canonical encoding, semantic rejection, captured calibration, record authority and replay passed\n";
}
