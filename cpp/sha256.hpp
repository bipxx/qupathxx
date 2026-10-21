#pragma once
#include <cstdint>
#include <memory>
#include <string>
namespace qpx {
// Streaming input digest. force_portable exercises the fallback on every OS.
class Sha256 {
public:
    explicit Sha256(bool force_portable=false);
    ~Sha256();
    void update(const void*,std::size_t);
    void integer(std::uint64_t);
    std::string finish();
private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
}
