#pragma once
#include <cstdint>
#include <cstddef>
#include <map>
#include <memory>
#include <string>
#include <variant>
#include <vector>
namespace qpx {
struct ImageDataValue {
    using List=std::vector<ImageDataValue>;
    using Map=std::map<std::string,ImageDataValue>;
    using Value=std::variant<std::monostate,bool,std::int64_t,double,std::string,List,Map>;
    Value value{};
};
// Immutable native ownership; no Python objects, callbacks or live image state.
class ImageDataProperties : public std::enable_shared_from_this<ImageDataProperties> {
public:
    static constexpr std::size_t max_entries=4096,max_nodes=16384,max_depth=16,max_payload=1024*1024;
    explicit ImageDataProperties(ImageDataValue::Map values={});
    ImageDataProperties(const ImageDataProperties&)=default;
    ImageDataProperties& operator=(const ImageDataProperties&)=delete;
    const ImageDataValue::Map& values() const {return values_;}
    bool empty() const {return values_.empty();}
    std::size_t payload_bytes() const {return bytes_;}
    bool equals(const ImageDataProperties&) const;
private:
    ImageDataValue::Map values_;
    std::size_t bytes_=sizeof(ImageDataProperties);
};
}
