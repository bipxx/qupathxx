#include "qupathxx/source_identity.hpp"
#include "qupathxx/core.hpp"
#include "../reader_provenance.hpp"
#include "../reader_file_guard.hpp"
#include "../provenance_codec.hpp"
#include "../sha256.hpp"
#include <filesystem>
#include <chrono>
#include <fstream>
#include <iostream>
#include <random>
#include <stdexcept>
using namespace qpx;
void check(bool v) {if(!v) throw std::runtime_error("source identity check failed");}
template<class E,class F> void rejects(F f) {try {f();} catch(const E&) {return;}throw std::logic_error("missing source identity rejection");}
void checksum(std::vector<std::uint8_t>& bytes) {Sha256 hash;hash.update(bytes.data(),bytes.size()-64);const auto sum=hash.finish();std::copy(sum.begin(),sum.end(),bytes.end()-64);}
int main() {
 try {
    const auto path=std::filesystem::current_path()/("reader-source-"+std::to_string(std::random_device{}())+".ppm");
    {std::ofstream f(path,std::ios::binary);f<<"P6\n2 2\n255\n";for(int i=0;i<12;++i) f.put(char(i));}
    auto server=ppm_image_server(path.u8string());const auto source=server->reader_source();check(source && source->scope=="single_file" && source->primary && source->primary->bytes==23);
    const auto hash=fingerprint_file(path.u8string());auto association=associate_source_file(*source,*hash);check(association.status=="matching_metadata");
    auto changed=*hash;changed.bytes+=1;check(associate_source_file(*source,changed).status=="version_changed");changed=*hash;changed.identity+="1";check(associate_source_file(*source,changed).status=="different_file");
    const auto memory=dense_image_server(Image(1,1,{0,0,0}))->reader_source();check(memory->scope=="memory" && associate_source_file(*memory,*hash).status=="unobserved" && memory->generation!=source->generation);
    auto second=ppm_image_server(path.u8string());check(second->reader_source()->generation!=source->generation);
    Document doc(server);auto object=doc.add_rectangle({0,0,2,2});AnalysisRequest request(doc,object,StainTransform({{1,0,0},{0,1,0}}));auto p=request.provenance();check(p->reader_source==source);
    auto bytes=ProvenanceCodec::encode(*p);check(bytes[3]==2);auto restored=ProvenanceCodec::decode(bytes.data(),bytes.size());check(reader_source_json(*restored->reader_source)==reader_source_json(*source));
    check(ProvenanceCodec::encode(*restored)==bytes && reader_source_json(*source).find("qpx.reader-source/1")!=std::string::npos && reader_source_json(*source).find("companions")==std::string::npos);
    const auto companion_path=path.u8string()+".companion";
    {std::ofstream file(companion_path,std::ios::binary);file<<"independent companion";}
    std::shared_ptr<const ReaderSource> compound;
    {detail::ReaderFileGuard companion(companion_path);compound=make_compound_reader_source(source->reader,source->locator,source->primary,{*companion.version()});}
    check(compound->scope=="compound_files" && compound->companions.size()==1 && compound->generation!=source->generation);
    check(associate_source_file(*compound,*hash).status=="matching_primary_metadata");
    check(reader_source_json(*compound).find("qpx.reader-source/2")!=std::string::npos);
    const auto primary_association=associate_source_file(*compound,*hash);
    check(source_file_association_json(primary_association).find("qpx.source-file-association/2")!=std::string::npos);
    rejects<std::invalid_argument>([&]{save_source_file_association(primary_association,companion_path);});
    FileFingerprintJob compound_job;rejects<std::invalid_argument>([&]{compound_job.request_source(*compound,companion_path);});
    check(!compound_job.busy() && compound_job.request_source(*compound));check(compound_job.wait().state=="succeeded" && compound_job.association()->status=="matching_primary_metadata");
    auto captured=*p;captured.reader_source=compound;const auto compound_bytes=ProvenanceCodec::encode(captured);check(compound_bytes[3]==3);
    const auto decoded=ProvenanceCodec::decode(compound_bytes.data(),compound_bytes.size());
    check(reader_source_json(*decoded->reader_source)==reader_source_json(*compound) && ProvenanceCodec::encode(*decoded)==compound_bytes);
    check(detail::same_reader_files(*compound,*decoded->reader_source));
    auto changed_compound=*compound;changed_compound.companions[0].modified_token="7:0";check(!detail::same_reader_files(*compound,changed_compound));
    changed_compound=*compound;changed_compound.generation="different-process-generation";check(detail::same_reader_files(*compound,changed_compound));
    changed_compound=*compound;changed_compound.scope="single_file";rejects<std::invalid_argument>([&]{changed_compound.validate();});
    changed_compound=*compound;changed_compound.companions.clear();rejects<std::invalid_argument>([&]{changed_compound.validate();});
    changed_compound=*compound;changed_compound.companions.push_back(changed_compound.companions[0]);rejects<std::invalid_argument>([&]{changed_compound.validate();});
    changed_compound=*compound;changed_compound.companions[0]=*compound->primary;rejects<std::invalid_argument>([&]{changed_compound.validate();});
    changed_compound=*compound;changed_compound.companions[0].path="relative.tif";rejects<std::invalid_argument>([&]{changed_compound.validate();});
    changed_compound=*compound;changed_compound.companions[0].path=std::string(32769,'a');rejects<std::invalid_argument>([&]{changed_compound.validate();});
    auto companions=compound->companions;companions.resize(64,companions[0]);rejects<std::invalid_argument>([&]{make_compound_reader_source(source->reader,source->locator,source->primary,companions);});
    auto first=compound->companions[0],last=first;first.path+=".a";last.path+=".z";last.identity+="1";
    auto sorted=make_compound_reader_source(source->reader,source->locator,source->primary,{last,first});check(sorted->companions[0].path==first.path);
    changed_compound=*sorted;std::swap(changed_compound.companions[0],changed_compound.companions[1]);rejects<std::invalid_argument>([&]{changed_compound.validate();});
    companions.clear();
    for(unsigned i=0;i<63;++i) {auto value=first;value.path="/"+std::string(2500,'x')+std::to_string(i);value.identity="999:"+std::to_string(i);companions.push_back(std::move(value));}
    rejects<std::invalid_argument>([&]{make_compound_reader_source(source->reader,source->locator,source->primary,companions);});
    auto downgraded=compound_bytes;downgraded[3]=2;checksum(downgraded);rejects<std::invalid_argument>([&]{ProvenanceCodec::decode(downgraded.data(),downgraded.size());});
    auto damaged=compound_bytes;damaged.back()^=1;rejects<std::invalid_argument>([&]{ProvenanceCodec::decode(damaged.data(),damaged.size());});
    std::size_t suffix=8+16;const auto& file=compound->companions[0];for(const auto* text:{&file.path,&file.identity_scheme,&file.identity,&file.stamp_scheme,&file.modified_token,&file.change_token}) suffix+=8+text->size();
    for(auto count:{0,64}) {damaged=compound_bytes;const auto offset=damaged.size()-64-suffix;damaged[offset]=std::uint8_t(count);checksum(damaged);rejects<std::invalid_argument>([&]{ProvenanceCodec::decode(damaged.data(),damaged.size());});}
    std::filesystem::remove(std::filesystem::u8path(companion_path));
    check(compound->companions[0].bytes==21); // Owning historical observations outlive files.
    std::cout<<"Compound source bounds, primary-only fingerprint identity, QPV3 round trip and downgrade refusal passed\n";
    auto legacy=*p;legacy.reader_source.reset();bytes=ProvenanceCodec::encode(legacy);bytes.resize(bytes.size()-64-8);bytes[3]=1;Sha256 digest;digest.update(bytes.data(),bytes.size());auto sum=digest.finish();bytes.insert(bytes.end(),sum.begin(),sum.end());check(!ProvenanceCodec::decode(bytes.data(),bytes.size())->reader_source);
    auto foreign=*source->primary;foreign.path="/historical/other-host.ppm";foreign.validate();foreign.path="C:\\historical\\other-host.ppm";foreign.validate();foreign.path="relative.ppm";rejects<std::invalid_argument>([&]{foreign.validate();});
    FileFingerprintJob job;const auto out=path.u8string()+".json";check(job.request_source(*source,out));check(job.wait().state=="succeeded" && job.association()->status=="matching_metadata");
    rejects<std::invalid_argument>([&]{save_source_file_association(association,path.u8string());});
    {detail::ReaderFileGuard guard(path.u8string());check(guard.version()->identity==hash->identity);guard.verify();
#ifdef _WIN32
        std::ofstream writer(path,std::ios::binary);check(!writer.good());guard.verify();
#else
        {std::fstream writer(path,std::ios::binary|std::ios::in|std::ios::out);writer.put('X');}
        std::filesystem::last_write_time(path,std::filesystem::last_write_time(path)+std::chrono::seconds(2));rejects<std::runtime_error>([&]{guard.verify();});
#endif
    }
    server->close();second->close();std::filesystem::remove(path);check(source->primary->bytes==23 && source_file_association_json(association).find("matching_metadata")!=std::string::npos);
    std::filesystem::remove(std::filesystem::u8path(out));
    std::cout<<"Reader generations, observations, association, codecs and native jobs passed\n";
 } catch(const std::exception& e) {std::cerr<<e.what()<<'\n';return 1;}
}
