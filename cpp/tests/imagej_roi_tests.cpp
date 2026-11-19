// SPDX-License-Identifier: AGPL-3.0-only
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include "geometry_archive.hpp"
#include "qupathxx/format_import.hpp"
#include "qupathxx/object_properties.hpp"
#include "qupathxx/project.hpp"
#include "qupathxx/raw_image.hpp"
#include <fstream>
#include <iostream>
#include <nlohmann/json.hpp>
#include <windows.h>
namespace fs=std::filesystem;
using namespace qpx;
using Json=nlohmann::json;
namespace {
void check(bool ok,const std::string& message){if(!ok)throw std::runtime_error(message);}
template<class F>void refuses(F&& call){try{call();}catch(const std::exception& e){check(*e.what(),"Empty native refusal");return;}throw std::runtime_error("Expected ImageJ refusal");}
Json json(const fs::path& path){std::ifstream f(path);check(bool(f),"Missing JSON oracle");return Json::parse(f);}
std::string hex(const std::vector<std::uint8_t>& data){std::string out;for(auto b:data){out+="0123456789abcdef"[b>>4];out+="0123456789abcdef"[b&15];}return out;}
Json objects(const Document& doc){Json rows=Json::array();for(const auto& [id,a]:doc.objects())rows.push_back({{"id",id},{"name",a.name},{"class",a.class_name},{"geometry",hex(GeometryArchiveCodec::encode(a.geometry))},{"c",a.plane.c},{"z",a.plane.z},{"t",a.plane.t},{"properties",a.properties?Json(a.properties->metadata()):Json::object()},{"color",a.properties && a.properties->color_rgb()?Json(*a.properties->color_rgb()):Json()}});return rows;}
std::shared_ptr<Document> target(const fs::path& dir){auto s=open_tiff((dir/"image.tif").u8string());RawRegionRequest rr;rr.region={0,0,0,16,16};rr.channels=3;const auto raw=s->read_raw(rr);check(raw.valid()==std::vector<std::uint8_t>(256,1) && std::get<std::vector<std::uint8_t>>(raw.storage())==std::vector<std::uint8_t>(768,80),"Literal source samples differ");auto d=std::make_shared<Document>(s);d->set_class_definition(ObjectClass::definition("Native",0x123456));d->clear_history();return d;}
AnnotationPluginImportOptions options(const Json& expected){AnnotationPluginImportOptions o;o.plugin_work_bytes=16u*1024*1024;o.work_bytes=64u*1024*1024;for(const auto& row:expected)if(!row.value("class",std::string{}).empty())o.class_map.emplace(row.at("class").get<std::string>(),"Native");for(const auto& row:expected){const auto& facts=row.at("facts");if((facts.contains("imagej.legacy-position") && facts.at("imagej.legacy-position")!="0") || (facts.contains("imagej.point-position") && facts.at("imagej.point-position")!="0"))o.legacy_position_axis=AnnotationLinearAxis::Z;}return o;}
void save(Document& d,const fs::path& path){Project p;auto shared=std::shared_ptr<Document>(&d,[](Document*){});p.add_document(shared);p.save(path.u8string());}
void positive(const fs::path& fixtures,const fs::path& work,const Json& example){const auto file=example.at("file").get<std::string>();const auto& expected=example.at("rows");auto d=target(fixtures);const auto before=objects(*d);const auto rev=d->revision();
 auto p=preview_annotation_plugin(*d,"org.qupathxx.imagej-roi",(fixtures/file).u8string(),options(expected));check(objects(*d)==before && d->revision()==rev && !d->can_undo(),"Preview mutated document");check(p.objects().size()==expected.size(),file+": object count differs");
 for(std::size_t i=0;i<expected.size();++i){const auto& a=p.objects()[i];const auto& e=expected.at(i);const auto& m=a.properties->metadata();check(a.name==e.at("name") && hex(GeometryArchiveCodec::encode(a.geometry))==e.at("geometry"),file+": exact geometry/name differs");check(m.at("qpx.annotation-plugin.source_id")==e.at("id") && a.plane==ObjectPlane(e.value("c",-1),e.value("z",0u),e.value("t",0u)),file+": source identity or plane differs");check(a.class_name==(e.value("class",std::string{}).empty()?"":"Native"),file+": explicit class mapping differs");
  for(const auto& [key,value]:e.at("facts").items())check(m.at("qpx.annotation-plugin.source."+key)==value.get<std::string>(),file+": source fact differs: "+key);
  check((a.properties->color_rgb()?Json(*a.properties->color_rgb()):Json())==e.value("color",Json()),file+": preferred native color differs");
 }
 const auto result=apply_annotation_plugin(*d,p);check(result.objects.size()==expected.size() && d->can_undo() && !d->can_redo(),"Apply/undo boundary differs");const auto rows=objects(*d);for(std::size_t i=0;i<expected.size();++i)check(result.objects[i].source_id==expected.at(i).at("id") && result.objects[i].object_id==i+1,"ID mapping differs");
 d->undo();check(objects(*d)==before,"Undo did not remove exact import");d->redo();check(objects(*d)==rows,"Redo changed objects");refuses([&]{apply_annotation_plugin(*d,p);});
 const auto output=work/(file+".qpproj");save(*d,output);auto reopened=Project::open(output.u8string());check(objects(*reopened->document(1))==rows,"Persisted ImageJ properties/geometry differ");
 std::ofstream facts(work/(file+".expected.json"));facts<<rows.dump(2);std::ofstream report(work/(file+".preview.json"));report<<Json({{"preview_sha256",result.preview_sha256},{"receipt",Json::parse(format_plugin_receipt_json(result.receipt))}}).dump(2);
}
void validate(const fs::path& fixtures,const fs::path& work){const auto suite=json(fixtures/"fixtures.json");auto base=target(fixtures);save(*base,work/"base.qpproj");for(const auto& e:suite.at("positive"))positive(fixtures,work,e);
 for(const auto& e:suite.at("negative")){auto d=target(fixtures);const auto rows=objects(*d);const auto rev=d->revision();refuses([&]{preview_annotation_plugin(*d,"org.qupathxx.imagej-roi",(fixtures/e.get<std::string>()).u8string(),options(Json::array()));});check(objects(*d)==rows && d->revision()==rev && !d->can_undo(),"Refused ROI mutated native target");}
 auto d=target(fixtures);const auto file=fixtures/"rectangle.roi";auto small=options(Json::array());small.plugin_work_bytes=256*1024;small.work_bytes=64*1024;refuses([&]{preview_annotation_plugin(*d,"org.qupathxx.imagej-roi",file.u8string(),small);});
 std::atomic<bool> cancelled{true};refuses([&]{preview_annotation_plugin(*d,"org.qupathxx.imagej-roi",file.u8string(),options(Json::array()),{},&cancelled);});
 auto opts=options(Json::array());opts.max_source_bytes=1;refuses([&]{preview_annotation_plugin(*d,"org.qupathxx.imagej-roi",file.u8string(),opts);});
 opts=options(Json::array());opts.plugin_work_bytes=256*1024;refuses([&]{preview_annotation_plugin(*d,"org.qupathxx.imagej-roi",(fixtures/"parse-arena-limit.roi").u8string(),opts);});
 std::cout<<Json({{"positive",suite.at("positive").size()},{"negative",suite.at("negative").size()},{"passed",true}}).dump()<<'\n';
}
int dispatch(int argc,char** argv){try{check(argc==5,"MODE MANIFEST FIXTURES WORK required");const std::string mode=argv[1];auto f=fs::absolute(fs::u8path(argv[3])),w=fs::absolute(fs::u8path(argv[4]));
 if(mode=="offline"){const auto suite=json(f/"fixtures.json");for(const auto& e:suite.at("positive")){const auto file=e.at("file").get<std::string>();auto p=Project::open((w/(file+".qpproj")).u8string());check(objects(*p->document(1))==json(w/(file+".expected.json")),"Offline project differs");}return 0;}
 if(mode=="compare"){auto p=Project::open(f.u8string()),q=Project::open(w.u8string());check(objects(*p->document(1))==objects(*q->document(1)),"Native and CLI persisted results differ");return 0;}
 if(mode=="base"){auto d=target(f);save(*d,w/"base.qpproj");return 0;}
 configure_format_plugins(fs::absolute(fs::u8path(argv[2])).u8string());check(format_plugin_diagnostics().empty() && format_plugin_infos().size()==1 && format_plugin_infos().front().abi==3,"ImageJ catalog failed");validate(f,w);return 0;
 }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
}
int wmain(int argc,wchar_t** argv){std::vector<std::string> values;std::vector<char*> pointers;for(int i=0;i<argc;++i){const auto n=WideCharToMultiByte(CP_UTF8,WC_ERR_INVALID_CHARS,argv[i],-1,nullptr,0,nullptr,nullptr);if(n<=0)return 2;std::string s(std::size_t(n),'\0');if(!WideCharToMultiByte(CP_UTF8,WC_ERR_INVALID_CHARS,argv[i],-1,s.data(),n,nullptr,nullptr))return 2;s.pop_back();values.push_back(std::move(s));}for(auto& s:values)pointers.push_back(s.data());return dispatch(argc,pointers.data());}
