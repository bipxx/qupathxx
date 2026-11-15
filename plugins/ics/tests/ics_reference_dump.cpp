// Development-only public libics API consumer. Never linked into the reader.
#include "libics.h"
#include <array>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

namespace fs=std::filesystem;
namespace {
void check(bool value,const std::string& message){if(!value)throw std::runtime_error(message);}
void ok(Ics_Error value,const char* call){if(value!=IcsErr_Ok){const auto* text=IcsGetErrorText(value);throw std::runtime_error(std::string(call)+": "+(text?text:"unknown reference error"));}}
std::string quote(const char* text){
    std::string result="\"";for(const unsigned char c:std::string(text?text:"")){
        check(c<128,"Reference report expected ASCII metadata");
        if(c=='\\'||c=='\"'){result+='\\';result+=static_cast<char>(c);}
        else if(c=='\n')result+="\\n";else if(c=='\r')result+="\\r";else if(c=='\t')result+="\\t";
        else{check(c>=32,"Unexpected control character in reference metadata");result+=static_cast<char>(c);}
    }return result+'\"';
}
const char* type_name(Ics_DataType type){switch(type){case Ics_uint8:return "uint8";case Ics_uint16:return "uint16";case Ics_sint16:return "int16";case Ics_real32:return "float32";default:throw std::runtime_error("Unexpected reference scalar type");}}
struct Handle{ICS* value=nullptr;~Handle(){if(value)IcsClose(value);}};
}
int main(int argc,char** argv){try{
    check(argc==3,"Usage: ics_reference_dump HEADER_RELATIVE OUTPUT_PREFIX_RELATIVE");
    for(int i=1;i<3;++i){check(!fs::path(argv[i]).is_absolute(),"Use ASCII relative paths for the pinned reference API");for(const unsigned char c:std::string(argv[i]))check(c>0&&c<128,"Reference argv must be ASCII; the product is checked separately with UTF8 paths");}
    Handle handle;ok(IcsOpen(&handle.value,argv[1],"r"),"IcsOpen");
    Ics_DataType type;int count=0;std::array<std::size_t,ICS_MAXDIM> dimensions{};ok(IcsGetLayout(handle.value,&type,&count,dimensions.data()),"IcsGetLayout");check(count>0&&count<=ICS_MAXDIM,"Invalid reference dimension count");
    const auto size=IcsGetDataSize(handle.value),sample=IcsGetImelSize(handle.value);check(size>0&&size<=256ull*1024*1024,"Reference data exceeds development limit");
    std::vector<std::uint8_t> data(size);ok(IcsGetData(handle.value,data.data(),data.size()),"IcsGetData");
    const std::string prefix=argv[2];std::ofstream raw(prefix+".raw",std::ios::binary);raw.write(reinterpret_cast<const char*>(data.data()),static_cast<std::streamsize>(data.size()));check(bool(raw),"Cannot write reference bytes");
    std::ofstream meta(prefix+".json");meta<<std::setprecision(17)<<"{\n  \"schema\":\"qpx.ics-native-reference/1\",\n  \"type\":"<<quote(type_name(type))<<",\n  \"sample_bytes\":"<<sample<<",\n  \"data_bytes\":"<<size<<",\n  \"dimensions\":[";for(int d=0;d<count;++d){if(d)meta<<',';meta<<dimensions[d];}meta<<"],\n  \"axes\":[";
    for(int d=0;d<count;++d){const char *order=nullptr,*label=nullptr,*unit=nullptr;double origin=0,scale=0;ok(IcsGetOrderF(handle.value,d,&order,&label),"IcsGetOrderF");ok(IcsGetPositionF(handle.value,d,&origin,&scale,&unit),"IcsGetPositionF");if(d)meta<<',';meta<<"{\"order\":"<<quote(order)<<",\"label\":"<<quote(label)<<",\"origin\":"<<origin<<",\"scale\":"<<scale<<",\"unit\":"<<quote(unit)<<'}';}
    std::size_t bits=0;ok(IcsGetSignificantBits(handle.value,&bits),"IcsGetSignificantBits");double origin=0,scale=0;const char* unit=nullptr;ok(IcsGetImelUnitsF(handle.value,&origin,&scale,&unit),"IcsGetImelUnitsF");std::array<char,ICS_STRLEN_TOKEN> coordinates{};ok(IcsGetCoordinateSystem(handle.value,coordinates.data()),"IcsGetCoordinateSystem");
    meta<<"],\n  \"significant_bits\":"<<bits<<",\n  \"coordinates\":"<<quote(coordinates.data())<<",\n  \"scalar_origin\":"<<origin<<",\n  \"scalar_scale\":"<<scale<<",\n  \"scalar_units\":"<<quote(unit)<<"\n}\n";check(bool(meta),"Cannot write reference metadata");
    auto* closing=handle.value;handle.value=nullptr;ok(IcsClose(closing),"IcsClose");std::cout<<"Read exact reference bytes and axis metadata through libics public APIs\n";return 0;
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}}
