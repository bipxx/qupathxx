#ifdef NDEBUG
#undef NDEBUG
#endif
#include "../exact_weighted_mean.hpp"
#include "raw_scientific_mean_reference.hpp"
#include <algorithm>
#include <cassert>
#include <iostream>
#include <iterator>
#include <sstream>

namespace {
std::uint64_t bits(double value) {std::uint64_t raw;std::memcpy(&raw,&value,8);return raw;}
template<class T> T source(std::uint64_t raw) {
    T out;
    if constexpr(sizeof(T)==8) std::memcpy(&out,&raw,8);
    else if constexpr(sizeof(T)==4) {const auto v=std::uint32_t(raw);std::memcpy(&out,&v,4);}
    else if constexpr(sizeof(T)==2) {const auto v=std::uint16_t(raw);std::memcpy(&out,&v,2);}
    else {const auto v=std::uint8_t(raw);std::memcpy(&out,&v,1);}
    return out;
}
template<class E,class F> void rejects(F action) {
    bool rejected=false;try {action();} catch(const E&) {rejected=true;}
    assert(rejected);
}
}
void raw_scientific_numeric_tests() {
    using qpx::detail::ExactWeightedMean;
    std::array<bool,10> observed{};
    assert(std::size(raw_mean_reference::cases)==2891);
    for(const auto& c:raw_mean_reference::cases) {
        for(bool reverse:{false,true}) {
            ExactWeightedMean mean;
            assert(c.count && c.offset+c.count<=std::size(raw_mean_reference::terms));
            for(std::size_t i=0;i<c.count;++i) {
                const auto at=reverse?c.count-1-i:i;
                const auto term=raw_mean_reference::terms[c.offset+at];const auto raw=term.raw;const auto w=term.weight;
                switch(c.type) {
                    case 0:mean.add(source<std::uint8_t>(raw),w);break;
                    case 1:mean.add(source<std::uint16_t>(raw),w);break;
                    case 2:mean.add(source<std::int8_t>(raw),w);break;
                    case 3:mean.add(source<std::int16_t>(raw),w);break;
                    case 4:mean.add(source<std::uint32_t>(raw),w);break;
                    case 5:mean.add(source<std::int32_t>(raw),w);break;
                    case 6:mean.add(source<std::uint64_t>(raw),w);break;
                    case 7:mean.add(source<std::int64_t>(raw),w);break;
                    case 8:mean.add(source<float>(raw),w);break;
                    case 9:mean.add(source<double>(raw),w);break;
                    default:throw std::runtime_error("unrecognized raw mean reference type");
                }
            }
            const auto actual=bits(mean.mean());
            if(actual!=c.expected) {
                std::ostringstream message;message<<c.name<<": expected "<<std::hex<<c.expected<<", got "<<actual;
                throw std::runtime_error(message.str());
            }
        }
        observed[c.type]=true;
    }
    assert(std::all_of(observed.begin(),observed.end(),[](bool v){return v;}));
    ExactWeightedMean empty;
    rejects<std::invalid_argument>([&]{empty.mean();});
    rejects<std::invalid_argument>([&]{empty.add(1u,0);});
    rejects<std::length_error>([&]{empty.add(1u,ExactWeightedMean::maximum_weight+1);});
    assert(empty.weight()==0);
    ExactWeightedMean finite;finite.add(std::int64_t(-7),5);
    for(const auto raw:{0x7ff0000000000000ULL,0xfff0000000000000ULL,0x7ff0000000000001ULL,0x7ff8000000000123ULL})
        rejects<std::invalid_argument>([&]{finite.add(source<double>(raw),1);});
    for(const auto raw:{0x7f800000u,0xff800000u,0x7f800001u,0x7fc00123u})
        rejects<std::invalid_argument>([&]{finite.add(source<float>(raw),1);});
    rejects<std::invalid_argument>([&]{finite.add(1.,0);});
    rejects<std::length_error>([&]{finite.add(1.,ExactWeightedMean::maximum_weight);});
    assert(finite.weight()==5 && finite.mean()==-7);
    ExactWeightedMean limit;limit.add(std::numeric_limits<double>::max(),ExactWeightedMean::maximum_weight);
    assert(limit.mean()==std::numeric_limits<double>::max());
    rejects<std::length_error>([&]{limit.add(0.,1);});
    assert(limit.weight()==ExactWeightedMean::maximum_weight && limit.mean()==std::numeric_limits<double>::max());
    ExactWeightedMean negative_zero;negative_zero.add(-std::numeric_limits<double>::denorm_min(),1);negative_zero.add(0.,1);
    assert(bits(negative_zero.mean())==0x8000000000000000ULL);
    ExactWeightedMean cancel;cancel.add(-1.,1);cancel.add(1.,1);assert(bits(cancel.mean())==0);
    std::cout<<"2891 independent exact raw mean vectors across all ten sample types in both orders passed\n";
    std::cout<<"Native exact typed weighted means, subnormal/cancellation, atomic rejection and nearest-even rounding passed\n";
}
#ifdef QUPATHXX_EXACT_MEAN_STANDALONE
int main() {raw_scientific_numeric_tests();}
#endif
