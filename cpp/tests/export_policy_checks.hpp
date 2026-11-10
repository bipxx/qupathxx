#pragma once
#include "../export_output.hpp"
#include <fstream>
#include <random>
#include <iostream>
namespace export_policy_checks {
inline void require(bool condition,const char* message) {if(!condition)throw std::runtime_error(message);}
inline std::string read(const std::filesystem::path& p) {std::ifstream f(p,std::ios::binary);return {std::istreambuf_iterator<char>(f),{}};}
inline void write(const std::filesystem::path& p,const std::string& bytes) {std::ofstream f(p,std::ios::binary);f<<bytes;require(bool(f),"write policy fixture");}
template<class F> void refuses(F&& call) {bool failed=false;try{call();}catch(const std::exception&){failed=true;}require(failed,"Missing export refusal");}
template<class Writer> void writer(Writer&& save,const std::string& expected,const char* name) {
    using namespace qpx;namespace fs=std::filesystem;
    const auto dir=fs::absolute(fs::current_path())/(std::string("export-policy-")+name+"-"+std::to_string(std::random_device{}()));
    require(fs::create_directory(dir),"Create unique export fixture");
    const auto file=dir/"out.data";write(file,"sentinel");
    refuses([&]{save(file.u8string(),ExportWritePolicy::CreateNew,{});});require(read(file)=="sentinel","Create-only replaced bytes");
    for(auto policy:{ExportWritePolicy::CreateNew,ExportWritePolicy::ReplaceExisting}) {
        refuses([&]{save(file.u8string(),policy,{file.u8string()});});require(read(file)=="sentinel","Protected file changed");
        refuses([&]{save(file.u8string(),policy,{dir.u8string()});});require(read(file)=="sentinel","Protected directory changed");
    }
    save(file.u8string(),ExportWritePolicy::ReplaceExisting,{});require(read(file)==expected,"Replacement differs from native output");
    const auto fresh=dir/"new.data";save(fresh.u8string(),ExportWritePolicy::CreateNew,{});require(read(fresh)==expected,"Create-only native bytes differ");
    const auto future=dir/"future-wal";
    refuses([&]{save(future.u8string(),ExportWritePolicy::ReplaceExisting,{future.u8string()});});require(!fs::exists(future),"Protected unused path created");
#ifdef _WIN32
    refuses([&]{save((dir/"FUTURE-WAL").u8string(),ExportWritePolicy::ReplaceExisting,{future.u8string()});});
#endif
    refuses([&]{save((dir/".."/dir.filename()/"future-wal").u8string(),ExportWritePolicy::ReplaceExisting,{future.u8string()});});
    // Deterministic appearance after prepare, before publication; no test hook.
    const auto race=dir/"race.data";
    {
        detail::AtomicExportFile output(race.u8string(),1024);output.write("new",3);write(race,"appeared");
        refuses([&]{detail::commit_export_output(output,race.u8string(),ExportWritePolicy::CreateNew);});
    }
    require(read(race)=="appeared","Publication race replaced appeared file");
    std::atomic<bool> cancelled{false};
    {
        detail::AtomicExportFile output(file.u8string(),1024,&cancelled);output.write("cancel",6);cancelled.store(true);
        refuses([&]{detail::commit_export_output(output,file.u8string(),ExportWritePolicy::ReplaceExisting);});
    }
    require(read(file)==expected,"Pre-commit cancellation changed bytes");
    for(const auto& entry:fs::directory_iterator(dir))require(entry.path().filename().u8string().find(".qpx-tmp-")==std::string::npos,"Temporary output survived refusal");
    fs::remove(file);fs::remove(fresh);fs::remove(race);require(fs::remove(dir),"Export fixture cleanup");
    std::cout<<"Guarded export policy: "<<name<<" create/replace/protection/race/cancel passed\n";
}
}
