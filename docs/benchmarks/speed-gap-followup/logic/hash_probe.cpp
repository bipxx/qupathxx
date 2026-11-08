
#include <chrono>
#include <cstdio>
#include <array>
#include "sha256.hpp"
int main(){constexpr size_t n=512*512;std::array<unsigned char,8192> bytes{};for(size_t i=0;i<bytes.size();i+=8)bytes[i]=1;
 for(int run=0;run<12;++run){auto start=std::chrono::steady_clock::now();qpx::Sha256 small;for(size_t i=0;i<n;++i)small.integer(1);auto a=small.finish();auto middle=std::chrono::steady_clock::now();qpx::Sha256 batch;for(size_t i=0;i<n;i+=1024)batch.update(bytes.data(),bytes.size());auto b=batch.finish();auto end=std::chrono::steady_clock::now();if(a!=b)return 2;std::printf("{\"iteration\":%d,\"scalar_ms\":%.6f,\"buffered_ms\":%.6f,\"digest\":\"%s\"}\n",run,std::chrono::duration<double,std::milli>(middle-start).count(),std::chrono::duration<double,std::milli>(end-middle).count(),a.c_str());}
}
