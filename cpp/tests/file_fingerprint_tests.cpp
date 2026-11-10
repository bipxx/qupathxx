#include "qupathxx/file_fingerprint.hpp"
#include "qupathxx/source_identity.hpp"
#include "export_policy_checks.hpp"
#include "../workflow_file_version.hpp"
#include "../filesystem_access.hpp"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <random>
#include <stdexcept>
using namespace qpx;
void check(bool v) {if(!v) throw std::runtime_error("file fingerprint check failed");}
template<class E,class F> void rejects(F f) {try {f();} catch(const E&) {return;}throw std::logic_error("missing file fingerprint rejection");}
int main() {try {
#ifdef _WIN32
    const wchar_t* locators[][3]={
        {L"C:\\study\\source.ppm",L"C:\\study\\out",L"..\\source.ppm"},
        {L"C:\\study\\source.ppm",L"\\\\?\\C:\\study\\out",L"..\\source.ppm"},
        {L"\\\\?\\C:\\study\\source.ppm",L"C:\\study\\out",L"..\\source.ppm"},
        {L"\\\\?\\c:\\study\\source.ppm",L"C:\\study\\out",L"..\\source.ppm"},
        {L"C:/study/source.ppm",L"\\\\?\\C:\\study\\out",L"..\\source.ppm"},
        {L"C:\\study\\\u8f93\u5165.ppm",L"\\\\?\\C:\\study\\out",L"..\\\u8f93\u5165.ppm"},
        {L"\\\\host\\share\\source.ppm",L"\\\\?\\UNC\\host\\share\\out",L"..\\source.ppm"},
        {L"\\\\?\\UNC\\host\\share\\source.ppm",L"\\\\host\\share\\out",L"..\\source.ppm"},
        {L"\\\\?\\unc\\host\\share\\source.ppm",L"\\\\host\\share\\out",L"..\\source.ppm"},
        {L"C:\\study\\COM0.ppm",L"\\\\?\\C:\\study\\out",L"..\\COM0.ppm"},
        {L"C:\\study\\COM10.ppm",L"\\\\?\\C:\\study\\out",L"..\\COM10.ppm"},
        {L"C:\\study\\source.ppm",L"D:\\out",L""},
        {L"\\\\host\\share1\\source.ppm",L"\\\\?\\UNC\\host\\share2\\out",L""},
        {L"\\\\host1\\share\\source.ppm",L"\\\\?\\UNC\\host2\\share\\out",L""},
        {L"\\\\?\\GLOBALROOT\\Device\\source.ppm",L"C:\\out",L""},
        {L"\\\\.\\C:\\source.ppm",L"C:\\out",L""},
        {L"\\\\?\\C:\\study\\source.ppm.",L"C:\\out",L""},
        {L"\\\\?\\C:\\study\\source.ppm ",L"C:\\out",L""},
        {L"\\\\?\\C:\\study\\source:stream",L"C:\\out",L""},
        {L"\\\\?\\C:\\study\\CON.ppm",L"C:\\out",L""},
        {L"\\\\?\\C:\\study\\COM\u00b9.ppm",L"C:\\out",L""},
        {L"\\\\?\\C:\\study\\LPT9.ppm",L"C:\\out",L""},
        {L"C:\\study\\source.ppm",L"\\\\?\\C:\\out. ",L""}
    };
    for(const auto& row:locators) {
        const std::filesystem::path source(row[0]),base(row[1]);
        check(detail::relative_locator(source,base)==std::filesystem::path(row[2]));
    }
#else
    check(detail::relative_locator("/missing-study/source.ppm","/missing-study/out")=="../source.ppm");
#endif
    std::cout<<"Namespace-stable locators, missing-source lexical comparison and incompatible/literal root fallback passed\n";
    const auto folder=std::filesystem::current_path()/("file-fingerprint-"+std::to_string(std::random_device{}()));std::filesystem::create_directory(folder);
    const auto input=folder/"abc.bin",output=folder/"fingerprint.json",alias=folder/"alias.bin";
    {std::ofstream stream(input,std::ios::binary);stream<<"abc";}
    const auto value=fingerprint_file(input.u8string());check(value->bytes==3 && value->sha256=="ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad");
    check(!value->identity.empty() && file_fingerprint_json(*value).find("qpx.file-fingerprint/1")!=std::string::npos);
    export_policy_checks::writer([&](const std::string& path,ExportWritePolicy policy,const std::vector<std::string>& protected_paths) {save_file_fingerprint_with_policy(*value,path,policy,protected_paths);},file_fingerprint_json(*value),"fingerprint-json");
    const auto reader=make_reader_source("ppm",input.u8string(),"single_file",detail::workflow_file_version(input.u8string()));
    const auto association=associate_source_file(*reader,*value);
    export_policy_checks::writer([&](const std::string& path,ExportWritePolicy policy,const std::vector<std::string>& protected_paths) {detail::save_source_association(association,path,nullptr,policy,protected_paths);},source_file_association_json(association),"source-json");
    const auto safe_output=folder/"create-only.json";{std::ofstream f(safe_output);f<<"sentinel";}
    FileFingerprintJob safe_job;rejects<WorkflowOutputExists>([&]{safe_job.request_with_policy(input.u8string(),safe_output.u8string(),ExportWritePolicy::CreateNew);});
    rejects<WorkflowOutputExists>([&]{safe_job.request_source_with_policy(*reader,safe_output.u8string(),ExportWritePolicy::CreateNew);});
    check(!safe_job.busy() && !safe_job.result() && export_policy_checks::read(safe_output)=="sentinel");std::filesystem::remove(safe_output);
    check(safe_job.request_source_with_policy(*reader,safe_output.u8string(),ExportWritePolicy::CreateNew));check(safe_job.wait().state=="succeeded");
    check(export_policy_checks::read(safe_output)==source_file_association_json(*safe_job.association()));std::filesystem::remove(safe_output);
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
