#include "nucls_csv.hpp"
#include <algorithm>
#include <charconv>
#include <cmath>
#include <cstdlib>
#include <locale.h>
#include <stdexcept>
namespace qpx::detail {
std::uint32_t nucls_codepoint(const std::string& text,std::size_t& at) {
    if(at==text.size())throw std::invalid_argument("Truncated CSV UTF-8");
    const auto first=static_cast<unsigned char>(text[at++]);if(first<128)return first;
    unsigned count=0;std::uint32_t result=0,minimum=0;
    if(first>=0xc2 && first<=0xdf){count=1;result=first&31;minimum=128;}
    else if(first>=0xe0 && first<=0xef){count=2;result=first&15;minimum=2048;}
    else if(first>=0xf0 && first<=0xf4){count=3;result=first&7;minimum=65536;}
    else throw std::invalid_argument("CSV requires UTF-8");
    if(count>text.size()-at)throw std::invalid_argument("Truncated CSV UTF-8");
    while(count--){const auto c=static_cast<unsigned char>(text[at++]);if((c&0xc0)!=0x80)throw std::invalid_argument("CSV requires UTF-8 continuations");result=(result<<6)|(c&63);}
    if(result<minimum || result>0x10ffff || (result>=0xd800 && result<=0xdfff))throw std::invalid_argument("Invalid CSV UTF-8 code point");return result;
}
bool nucls_space(std::uint32_t cp) {
    return (cp>=9 && cp<=13) || (cp>=0x1c && cp<=0x20) || cp==0x85 || cp==0xa0 || cp==0x1680 ||
        (cp>=0x2000 && cp<=0x200a) || cp==0x2028 || cp==0x2029 || cp==0x202f || cp==0x205f || cp==0x3000;
}
bool nucls_padded(const std::string& text) {
    if(text.empty())return false;std::size_t at=0;auto first=nucls_codepoint(text,at),last=first;
    while(at<text.size())last=nucls_codepoint(text,at);return nucls_space(first) || nucls_space(last);
}
namespace {
bool ascii_space(char c){return c==' ' || (c>='\t' && c<='\r');}
struct NumericLocale {
#ifdef _WIN32
    _locale_t value=_create_locale(LC_NUMERIC,"C");
    ~NumericLocale(){if(value)_free_locale(value);}
#else
    locale_t value=newlocale(LC_NUMERIC_MASK,"C",nullptr);
    ~NumericLocale(){if(value)freelocale(value);}
#endif
};
}
double nucls_number(const std::string& text) {
    // Unicode 15 decimal-zero ranges retain the legacy Unicode numeric contract.
    constexpr std::uint32_t zeros[]={0x30,0x660,0x6f0,0x7c0,0x966,0x9e6,0xa66,0xae6,0xb66,0xbe6,0xc66,0xce6,0xd66,0xde6,0xe50,0xed0,0xf20,0x1040,0x1090,0x17e0,0x1810,0x1946,0x19d0,0x1a80,0x1a90,0x1b50,0x1bb0,0x1c40,0x1c50,0xa620,0xa8d0,0xa900,0xa9d0,0xa9f0,0xaa50,0xabf0,0xff10,0x104a0,0x10d30,0x11066,0x110f0,0x11136,0x111d0,0x112f0,0x11450,0x114d0,0x11650,0x116c0,0x11730,0x118e0,0x11950,0x11c50,0x11d50,0x11da0,0x11f50,0x16a60,0x16ac0,0x16b50,0x1d7ce,0x1d7d8,0x1d7e2,0x1d7ec,0x1d7f6,0x1e140,0x1e2f0,0x1e4f0,0x1e950,0x1fbf0};
    std::string value;value.reserve(text.size());
    for(std::size_t at=0;at<text.size();) {
        auto cp=nucls_codepoint(text,at);
        if(cp>=128){if(nucls_space(cp))cp=' ';else {const auto digit=std::upper_bound(std::begin(zeros),std::end(zeros),cp);if(digit==std::begin(zeros) || cp-*(digit-1)>9)throw std::invalid_argument("Coordinate requires a decimal number");cp='0'+cp-*(digit-1);}}
        value+=char(cp);
    }
    const auto begin=std::find_if_not(value.begin(),value.end(),ascii_space),end=std::find_if_not(value.rbegin(),value.rend(),ascii_space).base();
    if(begin>=end)throw std::invalid_argument("Coordinate requires a finite number");value=std::string(begin,end);
    std::string number;number.reserve(value.size());
    const auto digit=[](char c){return c>='0' && c<='9';};
    for(std::size_t i=0;i<value.size();++i){if(value[i]=='_'){if(!i || i+1==value.size() || !digit(value[i-1]) || !digit(value[i+1]))throw std::invalid_argument("Invalid numeric underscore");}else number+=value[i];}
    std::size_t pos=0;if(number[pos]=='+' || number[pos]=='-')++pos;std::size_t digits=0;
    while(pos<number.size() && digit(number[pos])){++pos;++digits;}
    if(pos<number.size() && number[pos]=='.'){++pos;while(pos<number.size() && digit(number[pos])){++pos;++digits;}}
    if(!digits)throw std::invalid_argument("Coordinate requires a finite number");
    if(pos<number.size() && (number[pos]=='e' || number[pos]=='E')){++pos;if(pos<number.size() && (number[pos]=='+' || number[pos]=='-'))++pos;const auto start=pos;while(pos<number.size() && digit(number[pos]))++pos;if(start==pos)throw std::invalid_argument("Invalid coordinate exponent");}
    if(pos!=number.size())throw std::invalid_argument("Coordinate requires a finite decimal number");
    const auto* start=number.data();if(*start=='+')++start;double result=0;
    const auto converted=std::from_chars(start,number.data()+number.size(),result,std::chars_format::general);
    if(converted.ec==std::errc::result_out_of_range) {
        static const NumericLocale locale;if(!locale.value)throw std::runtime_error("Cannot create native numeric locale");char* stop=nullptr;
#ifdef _WIN32
        result=_strtod_l(number.c_str(),&stop,locale.value);
#else
        result=strtod_l(number.c_str(),&stop,locale.value);
#endif
        if(stop!=number.c_str()+number.size())throw std::invalid_argument("Invalid coordinate number");
    }else if(converted.ec!=std::errc{} || converted.ptr!=number.data()+number.size())throw std::invalid_argument("Invalid coordinate number");
    if(!std::isfinite(result))throw std::invalid_argument("Coordinate must be finite");return result;
}
NuclsCsvReader::NuclsCsvReader(const std::string& data):data_(data) {
    for(std::size_t at=0;at<data.size();)nucls_codepoint(data,at);
    if(data.size()>=3 && data.compare(0,3,"\xef\xbb\xbf")==0)at_=3;
}
bool NuclsCsvReader::next(std::vector<std::string>& fields) {
    fields.clear();if(at_==data_.size())return false;
    enum State {Start,Plain,Quoted,Closed};State state=Start;bool started=false;std::string value;std::size_t count=0,bytes=0;
    const auto append=[&](char c){if((static_cast<unsigned char>(c)&0xc0)!=0x80 && ++count>131072)throw std::length_error("CSV field exceeds131072 characters");value+=c;};
    const auto finish=[&]{if(value.size()+sizeof(std::string)>64u*1024*1024-bytes)throw std::length_error("CSV row exceeds native capture budget");bytes+=value.size()+sizeof(std::string);fields.push_back(std::move(value));value.clear();count=0;};
    while(at_<data_.size()) {
        const auto c=data_[at_++];
        if(c=='\r' || c=='\n') {
            const bool crlf=c=='\r' && at_<data_.size() && data_[at_]=='\n';if(crlf)++at_;++line_;
            if(state==Quoted){append(c);if(crlf)append('\n');continue;}
            if(started || !fields.empty())finish();return true;
        }
        started=true;
        if(state==Quoted){if(c=='"')state=Closed;else append(c);}
        else if(state==Closed){if(c=='"'){append(c);state=Quoted;}else if(c==','){finish();state=Start;}else throw std::invalid_argument("CSV delimiter expected after quote");}
        else if(c==','){finish();state=Start;}
        else if(state==Start && c=='"')state=Quoted;
        else {append(c);state=Plain;}
    }
    ++line_;if(state==Quoted)throw std::invalid_argument("CSV has an unterminated quoted field");finish();return true;
}
}
