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
