#include "qupathxx/ome_zarr.hpp"
#include "qupathxx/core.hpp"
#include "qupathxx/provenance.hpp"
#include "../provenance_codec.hpp"
#include "../project_source_internal.hpp"
#include "../reader_file_guard.hpp"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <chrono>
using namespace qpx;
namespace fs=std::filesystem;
void check(bool value,const char* message){if(!value)throw std::runtime_error(message);}
template<class F>void rejects(F fn){bool failed=false;try{fn();}catch(const std::exception&){failed=true;}check(failed,"Expected native directory/view refusal");}
void real_store(const std::string& original,const std::string& store,const fs::path& output){
    const auto partner=(fs::u8path(original).parent_path()/"tubhiswt_C1.ome.tif").u8string();
    const auto first=fingerprint_file(original),second=fingerprint_file(partner);
    check(first->sha256=="a1d99f0651baad26a6b5004c115a9c3f4e33d3d6e0b279b34cabbbe88d813ef2" && second->sha256=="c17cb626103d9a8b3f77b0d49fbff67ced2a6da83f6b2ae8beae183fb1a2a51f","Real fluorescence source bytes differ");
    TiffOpenOptions tiff;OmeZarrOpenOptions zarr;tiff.view.channels=zarr.view.channels={0,1,-1};tiff.view.lower=zarr.view.lower={7,0,0};tiff.view.upper=zarr.view.upper={48,255,1};
    const auto before=open_tiff(original,tiff),after=open_ome_zarr(store,zarr);const auto a=before->raw_metadata(),b=after->raw_metadata();
    check(a->width==512 && a->height==512 && a->channels==2 && b->width==a->width && b->height==a->height && b->channels==a->channels && b->z_planes==a->z_planes && b->timepoints==a->timepoints && b->sample_type==a->sample_type && b->sample_type==SampleType::UInt8 && a->pixel_width_um==b->pixel_width_um && a->pixel_height_um==b->pixel_height_um,"Full real image axes/calibration/type changed");
    RawRegionRequest request;request.region={0,0,0,512,512};request.channels=2;const auto left=before->read_raw(request),right=after->read_raw(request);
    check(left.storage()==right.storage() && left.valid()==right.valid(),"Not all524288 original fluorescence samples/validity match");
    const auto s=raw_region_statistics(*before,request,128),t=raw_region_statistics(*after,request,128);
    check(s.valid_pixels==262144 && t.valid_pixels==s.valid_pixels && s.means==t.means && s.minima==t.minima && s.maxima==t.maxima && s.finite_samples==t.finite_samples && s.nonfinite_samples==t.nonfinite_samples,"Real native raw measurements changed");
    const auto rgb=after->read_region({0,0,0,512,512});check(rgb.bytes()==before->read_region({0,0,0,512,512}).bytes(),"Real native fluorescence RGB viewport differs with the same explicit projection");
    const auto image=output/"real-zarr.ppm";check(!fs::exists(image),"Never overwrite native real render evidence");std::ofstream file(image,std::ios::binary);file<<"P6\n512 512\n255\n";for(std::size_t i=0;i<rgb.bytes().size();i+=4)file.write(reinterpret_cast<const char*>(rgb.bytes().data()+i),3);file.close();check(bool(file),"Cannot save real native render evidence");
    after->require_raw_current();check(fingerprint_file(original)->sha256==first->sha256 && fingerprint_file(partner)->sha256==second->sha256,"Original fluorescence data mutated");
    std::cout<<"Real full512x512x2: all524288 samples/validity, axes/calibration, raw means/min/max/finite counts and explicit RGB viewport match; source hashes unchanged; render="<<image.u8string()<<'\n';
}
int main(int argc,char** argv){try{
    const auto root=fs::current_path()/("zarr-server-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));fs::create_directory(root);const auto input=root/"image.zarr";fs::copy(fs::path(QPX_ZARR_FIXTURES)/"plain",input,fs::copy_options::recursive);
    OmeZarrOpenOptions options;options.view.z=1;options.view.t=1;options.view.channels={0,1,2};options.view.lower={10100,11100,12100};options.view.upper={10146,11146,12146};
    auto server=open_ome_zarr(input.u8string(),options);const auto observed=server->reader_source();check(observed->scope=="directory_store" && observed->companions.empty() && observed->directory_sha256.size()==64,"Directory inventory was truncated into companions");
    check(!server->tiff_view() && server->raw_view() && server->raw_view()->z==1 && server->ome_zarr_options()->work_bytes==options.work_bytes,"Generic raw view impersonated TIFF or lost options");
    const auto view=ome_zarr_view_encode(*server->raw_view());check(ome_zarr_view_encode(ome_zarr_view_decode(view))==view && view.rfind("qpx.ome-zarr-view/1",0)==0,"Zarr view persistence recipe changed");rejects([&]{tiff_view_decode(view);});auto corrupt_view=view;corrupt_view.back()^=1;rejects([&]{ome_zarr_view_decode(corrupt_view);});
    const auto rgb=server->read_region({3,2,0,1,1});check(rgb.bytes()==std::vector<std::uint8_t>{128,128,128,255},"Native RGB view windows/channel axes differ");
    const auto padded=server->read_region({-1,-1,0,2,2});check(padded.bytes()[3]==0 && padded.bytes()[15]==255,"RGB view padding lost transparency");
    RawRegionRequest request;request.region={0,0,0,7,5};request.channels=3;request.z=1;request.t=1;const auto stats=raw_region_statistics(*server,request,16);
    check(stats.valid_pixels==35 && stats.means==std::vector<std::optional<double>>{10123,11123,12123},"Native quantitative measurements use display colors or wrong axes");
    auto channels=original_channel_source(server);check(channels->read(request).sample_type()==SampleType::UInt16,"Shared native channel authority lost typed samples");
    Document document(server);check(document.active_plane()==ObjectPlane(-1,1,1),"Generic quantitative active plane was dropped");auto next=*server->raw_view();next.z=0;next.t=0;document.set_raw_view(next);check(document.active_plane()==ObjectPlane(-1,0,0) && server->raw_view()->z==1,"View change mutated an existing immutable projection");
    auto invalid=next;invalid.channels[0]=99;rejects([&]{document.set_raw_view(invalid);});check(document.active_plane()==ObjectPlane(-1,0,0),"Invalid view changed document state");
    const auto original=capture_provenance(*server,1,1,1,false,{},{});const auto bytes=ProvenanceCodec::encode(*original);check(bytes[3]==5,"Directory object provenance did not get its own encoding version");const auto decoded=ProvenanceCodec::decode(bytes.data(),bytes.size());check(decoded->reader_source->directory_sha256==observed->directory_sha256 && ProvenanceCodec::encode(*decoded)==bytes,"Directory provenance token lost in round trip");
    const auto source=capture_source_provenance(*server,1,{},{});const auto source_bytes=ProvenanceCodec::encode(*source,true);check(source_bytes[3]==6 && ProvenanceCodec::decode(source_bytes.data(),source_bytes.size(),true)->source_only,"Directory source-only provenance lost explicit scope");rejects([&]{ProvenanceCodec::decode(source_bytes.data(),source_bytes.size());});
    const auto fingerprint=fingerprint_file(observed->primary->path);const auto association=associate_source_file(*observed,*fingerprint);check(association.status=="matching_primary_metadata","Metadata fingerprint overclaims complete store bytes");
    FileFingerprintJob job;check(job.request_source(*observed) && job.wait().state=="succeeded" && job.association()->status=="matching_primary_metadata","Native fingerprint job tried to fingerprint a directory as a file");
    const auto verification=detail::encode_source_verification(association);check(verification[3]==2,"Directory source verification version missing");const auto restored=detail::decode_source_verification(verification.data(),verification.size());check(restored->source.directory_sha256==observed->directory_sha256 && detail::encode_source_verification(*restored)==verification,"Directory verification dropped inventory identity");
    {auto guards=detail::lock_source_verification_files(*restored);check(!guards.empty(),"Verified directory lost primary metadata guard");}
    const auto report=root/"source.json";save_source_file_association(association,report.u8string());check(fs::exists(report),"Directory association export missing");rejects([&]{save_source_file_association(association,(input/"forbidden.json").u8string());});check(!fs::exists(input/"forbidden.json"),"Association export mutated its source tree");
    auto bad=*observed;bad.directory_sha256=std::string(64,'g');rejects([&]{bad.validate();});bad=*observed;bad.scope="single_file";rejects([&]{bad.validate();});bad=*observed;auto outside=*bad.primary;outside.path=(input/".."/"outside.zattrs").u8string();bad.primary=std::make_shared<const FileVersion>(outside);rejects([&]{bad.validate();});
    {std::ofstream out(input/"0/0.0.0.0.0",std::ios::binary|std::ios::app);out.put('x');}
    check(associate_source_file(*observed,*fingerprint).status=="matching_primary_metadata","Historical metadata-only association unexpectedly became whole-store evidence");rejects([&]{channels->require_current();});rejects([&]{detail::lock_source_verification_files(*restored);});rejects([&]{server->read_region({3,2,0,1,1});});
    server->close();rejects([&]{server->raw_metadata();});rejects([&]{channels->read(request);});
    if(argc==3)real_store(argv[1],argv[2],root);else check(argc==1,"Optional real check requires original TIFF and complete Zarr directory");
    std::cout<<"Native Zarr image view/raw measurements/planes/directory provenance/QPV5-6/QSV2/primary-only fingerprint/stale guards pass\n";
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
