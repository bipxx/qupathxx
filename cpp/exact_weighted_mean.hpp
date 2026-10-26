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
template<bool Wide> class BasicExactWeightedMean {
    static_assert(sizeof(double)==8 && std::numeric_limits<double>::is_iec559 &&
                  std::numeric_limits<double>::digits==53 && std::numeric_limits<double>::max_exponent==1024);
    static_assert(sizeof(float)==4 && std::numeric_limits<float>::is_iec559 && std::numeric_limits<float>::digits==24);
    using Words=std::array<std::uint32_t,68>;
    using Weight=std::conditional_t<Wide,std::array<std::uint32_t,3>,std::uint32_t>;
    using SampleWeight=std::conditional_t<Wide,std::uint64_t,std::uint32_t>;
    Words positive_{},negative_{};
    Weight weight_{};
    Weight checked_weight(SampleWeight weight) const {
        if(!weight) throw std::invalid_argument("exact mean requires positive weights");
        if constexpr(!Wide) {if(weight>maximum_weight-weight_) throw std::length_error("exact mean exceeds scientific weight bound");return weight_+weight;}
        else {auto next=weight_;add_word(next,0,std::uint32_t(weight));if(weight>>32) add_word(next,1,std::uint32_t(weight>>32));if(next[2]>255) throw std::length_error("raw window exact mean exceeds 72-bit weight bound");return next;}
    }
    template<std::size_t N> static void add_word(std::array<std::uint32_t,N>& words,unsigned index,std::uint32_t value) {
        std::uint64_t carry=value;
        while(carry) {
            if(index>=words.size()) throw std::overflow_error("exact mean accumulator proof exceeded");
            const auto total=std::uint64_t(words[index])+carry;
            words[index++]=std::uint32_t(total);carry=total>>32;
        }
    }
    void add_component(std::uint64_t value,std::uint32_t weight,unsigned shift,bool negative) {
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
    }
    void add_magnitude(std::uint64_t value,SampleWeight weight,unsigned shift,bool negative) {
        const auto next=checked_weight(weight);add_component(value,std::uint32_t(weight),shift,negative);
        if constexpr(Wide) {if(weight>>32) add_component(value,std::uint32_t(weight>>32),shift+32,negative);}
        weight_=next;
    }
    template<std::size_t N> static int compare(const std::array<std::uint32_t,N>& a,const std::array<std::uint32_t,N>& b) {
        for(int i=int(a.size())-1;i>=0;--i) if(a[i]!=b[i]) return a[i]>b[i]?1:-1;
        return 0;
    }
    template<std::size_t N> static std::array<std::uint32_t,N> subtract(const std::array<std::uint32_t,N>& a,const std::array<std::uint32_t,N>& b) {
        std::array<std::uint32_t,N> out{};std::uint64_t borrow=0;
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
    static Weight product(std::uint64_t a,std::uint64_t b) {
        if(!a || !b) throw std::invalid_argument("exact mean footprint requires positive weights");
        if constexpr(!Wide) {if(!a || !b || a>maximum_weight/b) throw std::length_error("exact mean footprint exceeds weight bound");return std::uint32_t(a*b);}
        else {Weight out{};for(unsigned i=0;i<2;++i) for(unsigned j=0;j<2;++j) {const auto v=std::uint64_t(std::uint32_t(a>>(32*i)))*std::uint32_t(b>>(32*j));add_word(out,i+j,std::uint32_t(v));if(v>>32) add_word(out,i+j+1,std::uint32_t(v>>32));}if(out[2]>255) throw std::length_error("raw window footprint exceeds 72-bit weight bound");return out;}
    }
public:
    static constexpr std::uint32_t maximum_weight=1u<<28;
    auto weight() const {return weight_;}
    static void validate_footprint(std::uint64_t x,std::uint64_t y) {(void)product(x,y);}
    bool matches_weight(std::uint64_t x,std::uint64_t y) const {return weight_==product(x,y);}
    template<class T,std::enable_if_t<std::is_integral_v<T> && !std::is_same_v<T,bool>,int> =0>
    void add(T value,SampleWeight weight) {
        static_assert(sizeof(T)<=8);
        (void)checked_weight(weight);bool negative=false;
        if constexpr(std::is_signed_v<T>) negative=value<0;
        auto magnitude=static_cast<std::uint64_t>(value);
        if(negative) magnitude=std::uint64_t(0)-magnitude; // includes INT64_MIN
        add_magnitude(magnitude,weight,1074,negative);
    }
    void add(double value,SampleWeight weight) {
        (void)checked_weight(weight);std::uint64_t raw;std::memcpy(&raw,&value,8);
        const auto exponent=unsigned((raw>>52)&2047);
        if(exponent==2047) throw std::invalid_argument("exact mean requires finite inputs");
        auto significand=raw&((std::uint64_t(1)<<52)-1);
        if(exponent) significand|=std::uint64_t(1)<<52;
        add_magnitude(significand,weight,exponent?exponent-1:0,bool(raw>>63));
    }
    void add(float value,SampleWeight weight) {
        std::uint32_t raw;std::memcpy(&raw,&value,4);
        if(((raw>>23)&255)==255) throw std::invalid_argument("exact mean requires finite inputs");
        add(static_cast<double>(value),weight); // every finite binary32 value is exact in binary64
    }
    void add(bool,SampleWeight)=delete;
    double mean() const {
        if(weight_==Weight{}) throw std::invalid_argument("exact mean requires at least one sample");
        const auto order=compare(positive_,negative_);
        if(!order) return 0.; // exact cancellation, including signed source zeros, is canonical +0
        auto quotient=order>0?subtract(positive_,negative_):subtract(negative_,positive_);
        Weight remainder{};
        if constexpr(!Wide) {for(int i=int(quotient.size())-1;i>=0;--i) {const auto value=(std::uint64_t(remainder)<<32)|quotient[i];quotient[i]=std::uint32_t(value/weight_);remainder=std::uint32_t(value%weight_);}}
        else if(!weight_[1] && !weight_[2] && weight_[0]<=maximum_weight) {std::uint64_t rest=0;for(int i=int(quotient.size())-1;i>=0;--i) {const auto value=(rest<<32)|quotient[i];quotient[i]=std::uint32_t(value/weight_[0]);rest=value%weight_[0];}remainder[0]=std::uint32_t(rest);}
        else {const auto numerator=quotient;quotient={};for(int at=int(bits(numerator))-1;at>=0;--at) {std::uint32_t carry=std::uint32_t(bit(numerator,unsigned(at)));for(auto& word:remainder) {const auto next=word>>31;word=(word<<1)|carry;carry=next;}if(compare(remainder,weight_)>=0) {remainder=subtract(remainder,weight_);quotient[unsigned(at)/32]|=std::uint32_t(1)<<(unsigned(at)%32);}}}
        const auto size=bits(quotient);const auto sign=order<0?std::uint64_t(1)<<63:0;
        const auto round_remainder=[&](std::uint64_t value) {
            if constexpr(!Wide) return 2*std::uint64_t(remainder)>weight_ || (2*std::uint64_t(remainder)==weight_ && (value&1));
            else {auto twice=remainder;std::uint32_t carry=0;for(auto& word:twice) {const auto next=word>>31;word=(word<<1)|carry;carry=next;}const auto comparison=compare(twice,weight_);return comparison>0 || (comparison==0 && (value&1));}
        };
        if(size<=52) {
            auto value=(std::uint64_t(quotient[1])<<32)|quotient[0];
            value+=round_remainder(value);
            return number(sign|value); // includes subnormal-to-normal carry and negative underflow zero
        }
        const auto drop=size-53;std::uint64_t significand=0;
        for(unsigned i=0;i<53;++i) significand=(significand<<1)|std::uint64_t(bit(quotient,size-1-i));
        const auto increment=drop?
            (bit(quotient,drop-1) && (below(quotient,drop-1) || remainder!=Weight{} || (significand&1))):
            round_remainder(significand);
        significand+=increment;int exponent=int(size)-1-1074;
        if(significand==(std::uint64_t(1)<<53)) {significand>>=1;++exponent;}
        if(exponent>1023) throw std::overflow_error("exact finite mean exceeded binary64 range");
        const auto packed=(std::uint64_t(exponent+1023)<<52)|(significand&((std::uint64_t(1)<<52)-1));
        return number(sign|packed);
    }
};
using ExactWeightedMean=BasicExactWeightedMean<false>;
// Window weights may have large irreducible denominators even when only a few
// source pixels are visited. 72 total weight bits require <=2170 numerator bits;
// the existing 2176-bit accumulator and one nearest-even rounding still suffice.
using ExactRawWindowMean=BasicExactWeightedMean<true>;
}
