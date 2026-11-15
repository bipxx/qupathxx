#pragma once
// Internal generic access to the existing verified Windows PE/package authority.
#include <memory>
#include <string>
#include <vector>
namespace qpx::detail {
struct NativePluginFile {std::string path,sha256;};
struct NativePluginDependency {std::string name,sha256;};
class NativePluginModule {
public:
    static std::shared_ptr<NativePluginModule> admit(const std::string& package_root,
        const NativePluginFile& module,const std::vector<NativePluginFile>& dependencies);
    ~NativePluginModule();
    const std::string& module_sha256() const noexcept;
    const std::vector<NativePluginDependency>& dependencies() const noexcept;
    void* symbol(const char*) const;
    void callback_address(const void*) const;
    template<class Function>void callback(Function function) const {
        callback_address(reinterpret_cast<const void*>(function));
    }
    // Release verification buffers/file locks after all API/descriptor checks.
    // The admitted mapped module remains pinned until process exit.
    void finish_admission();
private:
    struct Impl;explicit NativePluginModule(std::unique_ptr<Impl>);
    std::unique_ptr<Impl> impl_;
};
}
