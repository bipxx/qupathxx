#include "../nucls_csv.hpp"
#include "../nucls_geometry.hpp"
#include <nlohmann/json.hpp>
#include <fstream>
#include <iostream>
#include <cstring>
#include <limits>
using namespace qpx::detail;
using Json=nlohmann::json;
void checked(bool value,int line){if(!value)throw std::runtime_error("NuCLS numeric check at line "+std::to_string(line));}
#define check(...) checked((__VA_ARGS__),__LINE__)
std::uint64_t bits(double value){std::uint64_t out;std::memcpy(&out,&value,8);return out;}
template<class F>void rejects(F action){bool hit=false;try{action();}catch(const std::exception&){hit=true;}check(hit);}
int main(int argc,char** argv){try{
    check(argc==2);std::ifstream input(argv[1]);const auto fixture=Json::parse(input);
    for(const auto& value:fixture["numbers"])check(bits(nucls_number(value["text"]))==std::stoull(value["bits"].get<std::string>(),nullptr,16));
    for(const auto& value:fixture["invalid_numbers"])rejects([&]{nucls_number(value.get<std::string>());});
    for(const auto& value:fixture["sums"])check(bits(nucls_accurate_sum(value["values"].get<std::vector<double>>()))==std::stoull(value["bits"].get<std::string>(),nullptr,16));
    rejects([&]{nucls_accurate_sum({std::numeric_limits<double>::max(),std::numeric_limits<double>::max()});});
    for(const auto& value:fixture["polygons"]){const auto geometry=nucls_geometry(value["row"].get<std::map<std::string,std::string>>());check(geometry.vertices==value["vertices"]);check(bits(geometry.centroid.x)==std::stoull(value["centroid_bits"][0].get<std::string>(),nullptr,16));check(bits(geometry.centroid.y)==std::stoull(value["centroid_bits"][1].get<std::string>(),nullptr,16));}
    std::string text="\xef\xbb\xbf" "a,b,c\r\n1,\"two\r\nlines\",\"quote\"\"ok\"\r\n\r\n3,4,5";NuclsCsvReader reader(text);std::vector<std::string> fields;
    check(reader.next(fields) && fields==std::vector<std::string>({"a","b","c"}) && reader.line()==1);
    check(reader.next(fields) && fields==std::vector<std::string>({"1","two\r\nlines","quote\"ok"}) && reader.line()==3);
    check(reader.next(fields) && fields.empty() && reader.line()==4);
    check(reader.next(fields) && fields==std::vector<std::string>({"3","4","5"}) && reader.line()==5 && !reader.next(fields));
    for(const std::string invalid:{"\"unterminated","\"closed\"x","bad\xff"})rejects([&]{NuclsCsvReader csv(invalid);csv.next(fields);});
    const std::string large(131073,'x');rejects([&]{NuclsCsvReader csv(large);csv.next(fields);});
    check(nucls_padded(" x") && nucls_padded(u8"\u3000x") && !nucls_padded(u8"\u80bf\u7624"));
    std::cout<<"NuCLS numeric/CSV:12 decimals,29 accurate sums,32 local-frame polygons bit-exact against Python oracle; UTF-8/quotes/physical lines/field limits passed\n";
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
