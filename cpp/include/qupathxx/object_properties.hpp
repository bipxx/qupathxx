#pragma once
#include <cstddef>
#include <map>
#include <memory>
#include <string>
namespace qpx {
// Imported/user-supplied historical facts, not recomputed analysis results.
// Names/values: bounded UTF-8, no NUL; <=4096 entries total and <=1 MiB payload.
class ObjectProperties {
public:
    ObjectProperties(std::map<std::string,std::string> metadata={},std::map<std::string,double> measurements={});
    ObjectProperties(const ObjectProperties&)=default;
    ObjectProperties& operator=(const ObjectProperties&)=delete;
    const std::map<std::string,std::string>& metadata() const {return metadata_;}
    const std::map<std::string,double>& measurements() const {return measurements_;}
    std::size_t payload_bytes() const {return bytes_;}
    bool equals(const ObjectProperties&) const;
private:
    std::map<std::string,std::string> metadata_;
    std::map<std::string,double> measurements_;
    std::size_t bytes_=0;
};
}
