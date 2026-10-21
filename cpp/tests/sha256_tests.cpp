#include "../sha256.hpp"
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
        std::cout<<"SHA-256 passed\n";
    } catch(const std::exception& e) {std::cerr<<e.what()<<'\n';return 1;}
}
