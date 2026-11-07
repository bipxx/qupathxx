#include "../sha256.hpp"
#include <algorithm>
#include <iostream>
#include <stdexcept>
#include <vector>
void check(bool v) {if(!v) throw std::runtime_error("SHA-256 check failed");}
int main() {
    try {
        for(bool portable:{false,true}) {
            qpx::Sha256 empty(portable);check(empty.finish()=="e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855");
            qpx::Sha256 abc(portable);abc.update("a",1);abc.update("bc",2);
            check(abc.finish()=="ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad");
            try {abc.update("x",1);throw std::runtime_error("accepted update after finish");} catch(const std::logic_error&) {}
            qpx::Sha256 million(portable);std::string chunk(1000,'a');for(int i=0;i<1000;++i) million.update(chunk.data(),chunk.size());
            check(million.finish()=="cdc76e5c9914fb9281a1c7e284d73e67f1809a48a497200e046d39ccc7112cd0");
        }
        for(std::size_t size:{55,56,63,64,65,255,4096}) {
            std::vector<std::uint8_t> data(size);for(std::size_t i=0;i<size;++i) data[i]=static_cast<std::uint8_t>(i);
            qpx::Sha256 reference;reference.update(data.data(),data.size());const auto expected=reference.finish();
            qpx::Sha256 portable(true);for(auto byte:data) portable.update(&byte,1);check(portable.finish()==expected);
        }
        for(std::size_t size:{0,1,7,8,55,56,63,64,65,127,128,255,4096,4097,16389}) {
            std::vector<std::uint8_t> data(size);for(std::size_t i=0;i<size;++i) data[i]=static_cast<std::uint8_t>(i*31+7);
            qpx::Sha256 reference(true);reference.update(data.data(),size);const auto expected=reference.finish();
            for(bool portable:{false,true}) for(std::size_t chunk:{1,3,8,31,63,64,65,97,4096}) {
                qpx::Sha256 hash(portable);hash.update(nullptr,0);
                for(std::size_t at=0;at<size;) {const auto n=std::min(chunk,size-at);hash.update(data.data()+at,n);hash.update(nullptr,0);at+=n;}
                check(hash.finish()==expected);
                bool refused=false;try {hash.finish();} catch(const std::logic_error&) {refused=true;}check(refused);
                refused=false;try {hash.update(nullptr,0);} catch(const std::logic_error&) {refused=true;}check(refused);
            }
            for(bool portable:{false,true}) {
                qpx::Sha256 hash(portable);const std::size_t chunks[]={3,8193,1,7,64};std::size_t turn=0;
                for(std::size_t at=0;at<size;++turn) {const auto n=std::min(chunks[turn%5],size-at);hash.update(data.data()+at,n);at+=n;}
                check(hash.finish()==expected);
            }
        }
        std::vector<std::uint8_t> encoded;
        for(std::uint64_t v=0;v<1000;++v) for(unsigned byte=0;byte<8;++byte) encoded.push_back(static_cast<std::uint8_t>((v*0x123456789ULL)>>(8*byte)));
        qpx::Sha256 reference(true);reference.update(encoded.data(),encoded.size());const auto expected=reference.finish();
        for(bool portable:{false,true}) {qpx::Sha256 hash(portable);for(std::uint64_t v=0;v<1000;++v) hash.integer(v*0x123456789ULL);check(hash.finish()==expected);}
        std::cout<<"SHA-256 passed\n";
    } catch(const std::exception& e) {std::cerr<<e.what()<<'\n';return 1;}
}
