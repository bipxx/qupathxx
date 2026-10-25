#pragma once
#include <array>
#include <cstdint>
#include <cstring>
#include <limits>
#include <stdexcept>
#include <type_traits>

namespace qpx::detail {
// Exact finite weighted numerator in units of 2^-1074. Under the scientific
// footprint bound, sum(weights)<=2^28, 2126 bits suffice even for DBL_MAX.
// Extra capacity is intentional. Division/nearest-even rounding happens once.
class ExactWeightedMean {
    static_assert(sizeof(double)==8 && std::numeric_limits<double>::is_iec559 &&
                  std::numeric_limits<double>::digits==53 && std::numeric_limits<double>::max_exponent==1024);
    static_assert(sizeof(float)==4 && std::numeric_limits<float>::is_iec559 && std::numeric_limits<float>::digits==24);
    using Words=std::array<std::uint32_t,68>;
    Words positive_{},negative_{};
    std::uint32_t weight_=0;
    void check_weight(std::uint32_t weight) const {
        if(!weight) throw std::invalid_argument("exact mean requires positive weights");
        if(weight>maximum_weight-weight_) throw std::length_error("exact mean exceeds scientific weight bound");
    }
    static void add_word(Words& words,unsigned index,std::uint32_t value) {
        std::uint64_t carry=value;
        while(carry) {
            if(index>=words.size()) throw std::overflow_error("exact mean accumulator proof exceeded");
            const auto total=std::uint64_t(words[index])+carry;
            words[index++]=std::uint32_t(total);carry=total>>32;
        }
    }
    void add_magnitude(std::uint64_t value,std::uint32_t weight,unsigned shift,bool negative) {
        const auto low=std::uint64_t(std::uint32_t(value))*weight;
        const auto high=(value>>32)*weight+(low>>32);
        const std::uint32_t digits[]={std::uint32_t(low),std::uint32_t(high),std::uint32_t(high>>32)};
        auto& words=negative?negative_:positive_;
        const auto word=shift/32,offset=shift%32;
        std::uint64_t carry=0;
        for(unsigned i=0;i<3;++i) {
            const auto part=(std::uint64_t(digits[i])<<offset)|carry;
            add_word(words,word+i,std::uint32_t(part));carry=part>>32;
        }
        if(carry) add_word(words,word+3,std::uint32_t(carry));
        weight_+=weight;
    }
    static int compare(const Words& a,const Words& b) {
        for(int i=int(a.size())-1;i>=0;--i) if(a[i]!=b[i]) return a[i]>b[i]?1:-1;
        return 0;
    }
    static Words subtract(const Words& a,const Words& b) {
        Words out{};std::uint64_t borrow=0;
        for(unsigned i=0;i<a.size();++i) {
            const auto right=std::uint64_t(b[i])+borrow;
            out[i]=std::uint32_t(std::uint64_t(a[i])-right);borrow=std::uint64_t(a[i])<right;
        }
        if(borrow) throw std::logic_error("exact mean subtraction order differs");
        return out;
    }
    static unsigned bits(const Words& words) {
        for(int i=int(words.size())-1;i>=0;--i) if(words[i]) {
            auto value=words[i];unsigned n=0;while(value) {++n;value>>=1;}
            return unsigned(i)*32+n;
        }
        return 0;
    }
    static bool bit(const Words& words,unsigned at) {return (words[at/32]>>(at%32))&1u;}
    static bool below(const Words& words,unsigned count) {
        for(unsigned i=0;i<count/32;++i) if(words[i]) return true;
        const auto tail=count%32;
        return tail && (words[count/32]&((std::uint32_t(1)<<tail)-1));
    }
    static double number(std::uint64_t bits) {double value;std::memcpy(&value,&bits,8);return value;}
public:
    static constexpr std::uint32_t maximum_weight=1u<<28;
    std::uint32_t weight() const {return weight_;}
    template<class T,std::enable_if_t<std::is_integral_v<T> && !std::is_same_v<T,bool>,int> =0>
    void add(T value,std::uint32_t weight) {
        static_assert(sizeof(T)<=8);
        check_weight(weight);bool negative=false;
        if constexpr(std::is_signed_v<T>) negative=value<0;
        auto magnitude=static_cast<std::uint64_t>(value);
        if(negative) magnitude=std::uint64_t(0)-magnitude; // includes INT64_MIN
        add_magnitude(magnitude,weight,1074,negative);
    }
    void add(double value,std::uint32_t weight) {
        check_weight(weight);std::uint64_t raw;std::memcpy(&raw,&value,8);
        const auto exponent=unsigned((raw>>52)&2047);
        if(exponent==2047) throw std::invalid_argument("exact mean requires finite inputs");
        auto significand=raw&((std::uint64_t(1)<<52)-1);
        if(exponent) significand|=std::uint64_t(1)<<52;
        add_magnitude(significand,weight,exponent?exponent-1:0,bool(raw>>63));
    }
    void add(float value,std::uint32_t weight) {
        std::uint32_t raw;std::memcpy(&raw,&value,4);
        if(((raw>>23)&255)==255) throw std::invalid_argument("exact mean requires finite inputs");
        add(static_cast<double>(value),weight); // every finite binary32 value is exact in binary64
    }
    void add(bool,std::uint32_t)=delete;
    double mean() const {
        if(!weight_) throw std::invalid_argument("exact mean requires at least one sample");
        const auto order=compare(positive_,negative_);
        if(!order) return 0.; // exact cancellation, including signed source zeros, is canonical +0
        auto quotient=order>0?subtract(positive_,negative_):subtract(negative_,positive_);
        std::uint64_t remainder=0;
        for(int i=int(quotient.size())-1;i>=0;--i) {
            const auto value=(remainder<<32)|quotient[i];
            quotient[i]=std::uint32_t(value/weight_);remainder=value%weight_;
        }
        const auto size=bits(quotient);const auto sign=order<0?std::uint64_t(1)<<63:0;
        const auto round_remainder=[&](std::uint64_t value) {
            return 2*remainder>weight_ || (2*remainder==weight_ && (value&1));
        };
        if(size<=52) {
            auto value=(std::uint64_t(quotient[1])<<32)|quotient[0];
            value+=round_remainder(value);
            return number(sign|value); // includes subnormal-to-normal carry and negative underflow zero
        }
        const auto drop=size-53;std::uint64_t significand=0;
        for(unsigned i=0;i<53;++i) significand=(significand<<1)|bit(quotient,size-1-i);
        const auto increment=drop?
            (bit(quotient,drop-1) && (below(quotient,drop-1) || remainder || (significand&1))):
            round_remainder(significand);
        significand+=increment;int exponent=int(size)-1-1074;
        if(significand==(std::uint64_t(1)<<53)) {significand>>=1;++exponent;}
        if(exponent>1023) throw std::overflow_error("exact finite mean exceeded binary64 range");
        const auto packed=(std::uint64_t(exponent+1023)<<52)|(significand&((std::uint64_t(1)<<52)-1));
        return number(sign|packed);
    }
};
}
