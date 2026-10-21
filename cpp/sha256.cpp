#include "sha256.hpp"
#include <algorithm>
#include <array>
#include <cstring>
#include <limits>
#include <stdexcept>
#ifdef _WIN32
#define NOMINMAX
#include <windows.h>
#include <bcrypt.h>
#endif
namespace qpx {
namespace {
constexpr std::uint32_t constants[]={
0x428a2f98,0x71374491,0xb5c0fbcf,0xe9b5dba5,0x3956c25b,0x59f111f1,0x923f82a4,0xab1c5ed5,
0xd807aa98,0x12835b01,0x243185be,0x550c7dc3,0x72be5d74,0x80deb1fe,0x9bdc06a7,0xc19bf174,
0xe49b69c1,0xefbe4786,0x0fc19dc6,0x240ca1cc,0x2de92c6f,0x4a7484aa,0x5cb0a9dc,0x76f988da,
0x983e5152,0xa831c66d,0xb00327c8,0xbf597fc7,0xc6e00bf3,0xd5a79147,0x06ca6351,0x14292967,
0x27b70a85,0x2e1b2138,0x4d2c6dfc,0x53380d13,0x650a7354,0x766a0abb,0x81c2c92e,0x92722c85,
0xa2bfe8a1,0xa81a664b,0xc24b8b70,0xc76c51a3,0xd192e819,0xd6990624,0xf40e3585,0x106aa070,
0x19a4c116,0x1e376c08,0x2748774c,0x34b0bcb5,0x391c0cb3,0x4ed8aa4a,0x5b9cca4f,0x682e6ff3,
0x748f82ee,0x78a5636f,0x84c87814,0x8cc70208,0x90befffa,0xa4506ceb,0xbef9a3f7,0xc67178f2};
std::uint32_t rotate(std::uint32_t v,unsigned n) {return (v>>n)|(v<<(32-n));}
}
struct Sha256::Impl {
    bool finished=false;
    std::uint64_t count=0;
    std::size_t used=0;
    std::array<std::uint8_t,64> block{};
    std::array<std::uint32_t,8> state={0x6a09e667,0xbb67ae85,0x3c6ef372,0xa54ff53a,0x510e527f,0x9b05688c,0x1f83d9ab,0x5be0cd19};
#ifdef _WIN32
    BCRYPT_ALG_HANDLE algorithm=nullptr;
    BCRYPT_HASH_HANDLE hash=nullptr;
    ~Impl() {if(hash) BCryptDestroyHash(hash);if(algorithm) BCryptCloseAlgorithmProvider(algorithm,0);}
#endif
    void compress(const std::uint8_t* p) {
        std::uint32_t w[64];
        for(unsigned i=0;i<16;++i) w[i]=(std::uint32_t(p[i*4])<<24)|(std::uint32_t(p[i*4+1])<<16)|(std::uint32_t(p[i*4+2])<<8)|p[i*4+3];
        for(unsigned i=16;i<64;++i) {
            const auto s0=rotate(w[i-15],7)^rotate(w[i-15],18)^(w[i-15]>>3);
            const auto s1=rotate(w[i-2],17)^rotate(w[i-2],19)^(w[i-2]>>10);
            w[i]=w[i-16]+s0+w[i-7]+s1;
        }
        auto a=state[0],b=state[1],c=state[2],d=state[3],e=state[4],f=state[5],g=state[6],h=state[7];
        for(unsigned i=0;i<64;++i) {
            const auto t1=h+(rotate(e,6)^rotate(e,11)^rotate(e,25))+((e&f)^(~e&g))+constants[i]+w[i];
            const auto t2=(rotate(a,2)^rotate(a,13)^rotate(a,22))+((a&b)^(a&c)^(b&c));
            h=g;g=f;f=e;e=d+t1;d=c;c=b;b=a;a=t1+t2;
        }
        state[0]+=a;state[1]+=b;state[2]+=c;state[3]+=d;state[4]+=e;state[5]+=f;state[6]+=g;state[7]+=h;
    }
};
Sha256::Sha256(bool portable):impl_(std::make_unique<Impl>()) {
#ifdef _WIN32
    if(!portable && (BCryptOpenAlgorithmProvider(&impl_->algorithm,BCRYPT_SHA256_ALGORITHM,nullptr,0)<0 ||
       BCryptCreateHash(impl_->algorithm,&impl_->hash,nullptr,0,nullptr,0,0)<0)) throw std::runtime_error("SHA-256 provider initialization failed");
#else
    (void)portable;
#endif
}
Sha256::~Sha256()=default;
void Sha256::update(const void* data,std::size_t size) {
    auto& s=*impl_;
    if(s.finished) throw std::logic_error("SHA-256 is finalized");
    if(size>std::numeric_limits<std::uint64_t>::max()/8-s.count) throw std::overflow_error("SHA-256 input too long");
    s.count+=size; const auto* p=static_cast<const std::uint8_t*>(data);
#ifdef _WIN32
    if(s.hash) {
        while(size) {const auto n=static_cast<ULONG>(std::min<std::size_t>(size,1u<<30));
            if(BCryptHashData(s.hash,const_cast<PUCHAR>(p),n,0)<0) throw std::runtime_error("SHA-256 update failed");
            p+=n;size-=n;
        }
        return;
    }
#endif
    while(size) {
        if(s.used==0 && size>=64) {s.compress(p);p+=64;size-=64;continue;}
        const auto n=std::min(size,64-s.used);std::memcpy(s.block.data()+s.used,p,n);s.used+=n;p+=n;size-=n;
        if(s.used==64) {s.compress(s.block.data());s.used=0;}
    }
}
void Sha256::integer(std::uint64_t value) {std::uint8_t bytes[8];for(unsigned i=0;i<8;++i) bytes[i]=static_cast<std::uint8_t>(value>>(i*8));update(bytes,8);}
std::string Sha256::finish() {
    auto& s=*impl_;if(s.finished) throw std::logic_error("SHA-256 is finalized");s.finished=true;
    std::array<std::uint8_t,32> digest{};
#ifdef _WIN32
    if(s.hash) {if(BCryptFinishHash(s.hash,digest.data(),32,0)<0) throw std::runtime_error("SHA-256 finalization failed");}
    else
#endif
    {
        s.block[s.used++]=0x80;
        if(s.used>56) {std::fill(s.block.begin()+s.used,s.block.end(),std::uint8_t{0});s.compress(s.block.data());s.used=0;}
        std::fill(s.block.begin()+s.used,s.block.begin()+56,std::uint8_t{0});
        const auto bits=s.count*8;for(unsigned i=0;i<8;++i) s.block[63-i]=static_cast<std::uint8_t>(bits>>(i*8));
        s.compress(s.block.data());
        for(unsigned i=0;i<32;++i) digest[i]=static_cast<std::uint8_t>(s.state[i/4]>>(24-8*(i%4)));
    }
    constexpr char hex[]="0123456789abcdef";std::string result;result.reserve(64);
    for(auto byte:digest) {result+=hex[byte>>4];result+=hex[byte&15];}return result;
}
}
