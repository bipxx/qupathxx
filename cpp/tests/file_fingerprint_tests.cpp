#include "qupathxx/file_fingerprint.hpp"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <random>
#include <stdexcept>
using namespace qpx;
void check(bool v) {if(!v) throw std::runtime_error("file fingerprint check failed");}
template<class E,class F> void rejects(F f) {try {f();} catch(const E&) {return;}throw std::logic_error("missing file fingerprint rejection");}
int main() {try {
    const auto folder=std::filesystem::current_path()/("file-fingerprint-"+std::to_string(std::random_device{}()));std::filesystem::create_directory(folder);
    const auto input=folder/"abc.bin",output=folder/"fingerprint.json",alias=folder/"alias.bin";
    {std::ofstream stream(input,std::ios::binary);stream<<"abc";}
    const auto value=fingerprint_file(input.u8string());check(value->bytes==3 && value->sha256=="ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad");
    check(!value->identity.empty() && file_fingerprint_json(*value).find("qpx.file-fingerprint/1")!=std::string::npos);
    FileFingerprintOptions options;options.byte_limit=2;rejects<std::length_error>([&]{fingerprint_file(input.u8string(),options);});
    options.byte_limit=3;check(fingerprint_file(input.u8string(),options)->sha256==value->sha256);
    std::atomic<bool> cancelled{true};rejects<std::runtime_error>([&]{fingerprint_file(input.u8string(),options,&cancelled);});
    rejects<std::invalid_argument>([&]{save_file_fingerprint(*value,input.u8string());});
    std::filesystem::create_hard_link(input,alias);rejects<std::invalid_argument>([&]{save_file_fingerprint(*value,alias.u8string());});
    FileFingerprintJob job;check(job.request(input.u8string(),output.u8string()));const auto status=job.wait();
    check(status.state=="succeeded" && status.bytes==status.total_bytes && status.completed==1 && job.result()->sha256==value->sha256);
    std::ifstream stream(output,std::ios::binary);const std::string json{std::istreambuf_iterator<char>(stream),{}};stream.close();check(json==file_fingerprint_json(*job.result()));
    options.byte_limit=0;check(job.request(input.u8string(),output.u8string(),options));check(job.wait().state=="failed" && !job.result());
    {std::ifstream stream(output,std::ios::binary);check(std::string(std::istreambuf_iterator<char>(stream),{})==json);}
    {std::ofstream stream(input,std::ios::binary|std::ios::trunc);}
    check(fingerprint_file(input.u8string(),options)->sha256=="e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855");
    check(value->bytes==3);std::filesystem::remove(alias);std::filesystem::remove(input);std::filesystem::remove(output);std::filesystem::remove(folder);
    std::cout<<"Streaming file digests, identity/stamp observations, limits, aliases and atomic worker files passed\n";
} catch(const std::exception& e) {std::cerr<<e.what()<<'\n';return 1;}}
