#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>
#include <numeric>
#include <stdexcept>

namespace qpx {
// Geometry preparation only. These values do not enable a new detector recipe.
class GridFraction {
    std::int64_t numerator_,denominator_;
public:
    GridFraction(std::int64_t n,std::int64_t d):numerator_(n),denominator_(d) {
        if(d<=0 || d>2000000 || n < -4100000000000000000LL || n>4100000000000000000LL)
            throw std::invalid_argument("invalid grid fraction");
        const auto divisor=std::gcd(n,d);numerator_/=divisor;denominator_/=divisor;
    }
    std::int64_t numerator() const {return numerator_;}
    std::int64_t denominator() const {return denominator_;}
    std::int64_t floor() const {return numerator_/denominator_-(numerator_%denominator_<0);}
    std::int64_t ceil() const {return numerator_/denominator_+(numerator_%denominator_>0);}
    double binary64() const {
        static_assert(std::numeric_limits<double>::is_iec559 && std::numeric_limits<double>::digits==53);
        if(!numerator_) return 0.;
        // Integer long division, rounded once to nearest/even. Converting the
        // large numerator first or adding a rounded fraction to a negative
        // floor can each lose a bit (especially near zero).
        const auto magnitude=std::uint64_t(numerator_<0?-numerator_:numerator_);
        const auto denominator=std::uint64_t(denominator_);
        auto whole=magnitude/denominator,remainder=magnitude%denominator;
        std::uint64_t significand=0;int exponent=0,remaining=0;
        if(whole) {
            for(auto bits=whole;bits>1;bits>>=1) ++exponent;
            if(exponent>=53) {
                const auto shift=exponent-52;
                significand=whole>>shift;
                const auto low=whole&((std::uint64_t(1)<<shift)-1),half=std::uint64_t(1)<<(shift-1);
                if(low>half || (low==half && (remainder || (significand&1)))) ++significand;
            } else {significand=whole;remaining=52-exponent;}
        } else {
            exponent=-1;remainder=magnitude*2;
            while(remainder<denominator) {remainder*=2;--exponent;}
            significand=1;remainder-=denominator;remaining=52;
        }
        if(exponent<53) {
            for(int bit=0;bit<remaining;++bit) {
                remainder*=2;significand=(significand<<1)+(remainder>=denominator);
                if(remainder>=denominator) remainder-=denominator;
            }
            if(remainder*2>denominator || (remainder*2==denominator && (significand&1))) ++significand;
        }
        const auto value=std::ldexp(double(significand),exponent-52);
        return numerator_<0?-value:value;
    }
};

class ScientificGridAxis {
    std::int64_t origin_,extent_,samples_,context_;
public:
    static constexpr std::int64_t coordinate_limit=1000000000000LL;
    ScientificGridAxis(std::int64_t origin,std::int64_t extent,std::int64_t samples,std::int64_t context=0):
        origin_(origin),extent_(extent),samples_(samples),context_(context) {
        if(origin < -coordinate_limit || origin>coordinate_limit || extent<1 || extent>coordinate_limit ||
           origin>coordinate_limit-extent || samples<1 || samples>1000000 || context<0 || context>4096)
            throw std::invalid_argument("invalid scientific grid axis");
    }
    std::int64_t origin() const {return origin_;}
    std::int64_t extent() const {return extent_;}
    std::int64_t samples() const {return samples_;}
    std::int64_t context() const {return context_;}
    std::int64_t first_index() const {return -context_;}
    std::int64_t stop_index() const {return samples_+context_;}
    GridFraction edge(std::int64_t index) const {
        if(index<first_index() || index>stop_index()) throw std::out_of_range("grid edge outside captured context");
        return {origin_*samples_+index*extent_,samples_};
    }
    GridFraction center(std::int64_t index) const {
        if(index<first_index() || index>=stop_index()) throw std::out_of_range("grid sample outside captured context");
        // Admission keeps both products and their sum within signed int64.
        return {origin_*(2*samples_)+(2*index+1)*extent_,2*samples_};
    }
    GridFraction overlap(std::int64_t index,std::int64_t source_pixel) const {
        const auto left=edge(index),right=edge(index+1);
        if(source_pixel<left.floor() || source_pixel>=right.ceil()) return {0,1};
        const auto a=origin_*samples_+index*extent_,b=a+extent_;
        return {std::min(b,(source_pixel+1)*samples_)-std::max(a,source_pixel*samples_),samples_};
    }
    std::uint64_t overlap_visits() const {
        const auto a=first_index(),b=stop_index(),period=samples_/std::gcd(extent_,samples_);
        const auto integers=GridFraction(b-1,period).floor()-GridFraction(a,period).floor();
        return static_cast<std::uint64_t>(edge(b).ceil()-edge(a).floor()+b-a-1-integers);
    }
};

struct GridReadBounds {std::int64_t x,y,width,height;};
class ScientificGrid {
    ScientificGridAxis x_,y_;
    GridReadBounds read_,clipped_;
    std::uint64_t visits_=0,sampling_bytes_=0;
public:
    ScientificGrid(std::int64_t image_width,std::int64_t image_height,
        ScientificGridAxis x,ScientificGridAxis y,std::uint64_t work_bytes=256ULL*1024*1024):x_(x),y_(y) {
        if(image_width<1 || image_height<1 || image_width>ScientificGridAxis::coordinate_limit ||
           image_height>ScientificGridAxis::coordinate_limit || !work_bytes || work_bytes>1024ULL*1024*1024)
            throw std::invalid_argument("invalid scientific grid image or sampling budget");
        const auto left=x_.edge(x_.first_index()).floor(),right=x_.edge(x_.stop_index()).ceil();
        const auto top=y_.edge(y_.first_index()).floor(),bottom=y_.edge(y_.stop_index()).ceil();
        read_={left,top,right-left,bottom-top};
        const auto l=std::clamp(left,std::int64_t(0),image_width),r=std::clamp(right,std::int64_t(0),image_width);
        const auto t=std::clamp(top,std::int64_t(0),image_height),b=std::clamp(bottom,std::int64_t(0),image_height);
        clipped_={l,t,r-l,b-t};
        const auto vx=x_.overlap_visits(),vy=y_.overlap_visits();
        if(vx>(1ULL<<28)/vy) throw std::length_error("scientific grid exceeds overlap work limit");
        visits_=vx*vy;
        const auto rows=std::uint64_t(y_.stop_index()-y_.first_index());
        const auto columns=std::uint64_t(x_.stop_index()-x_.first_index());
        if(columns>std::numeric_limits<std::uint32_t>::max()/rows) throw std::length_error("scientific grid exceeds label index range");
        constexpr std::uint64_t reader_limit=256ULL*1024*1024;
        if(clipped_.height && std::uint64_t(clipped_.width)>reader_limit/4/std::uint64_t(clipped_.height))
            throw std::length_error("scientific grid exceeds source buffer limit");
        const auto source_bytes=std::uint64_t(clipped_.width)*std::uint64_t(clipped_.height)*4;
        const auto output_samples=rows*columns;
        if(source_bytes>work_bytes || output_samples>(work_bytes-source_bytes)/25)
            throw std::length_error("scientific grid exceeds sampling buffer budget");
        sampling_bytes_=source_bytes+25*output_samples;
    }
    const ScientificGridAxis& x() const {return x_;}
    const ScientificGridAxis& y() const {return y_;}
    GridReadBounds read_bounds() const {return read_;}
    GridReadBounds clipped_read_bounds() const {return clipped_;}
    std::uint64_t overlap_visits() const {return visits_;}
    // RGB float64 + validity byte + one RGBA8 source window. Detector/kernel/
    // contour work is additional and must be admitted by the future request.
    std::uint64_t sampling_buffer_bytes() const {return sampling_bytes_;}
};
}
