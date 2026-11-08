#pragma once
#include <cstddef>
#include <cstdint>
#include <optional>
#include <map>
#include <memory>
#include <string>
namespace qpx {
// Imported/user-supplied historical facts, not recomputed analysis results.
// Names/values: bounded UTF-8, no NUL; <=4096 entries total and <=1 MiB payload.
class ObjectProperties : public std::enable_shared_from_this<ObjectProperties> {
public:
    ObjectProperties(std::map<std::string,std::string> metadata={},std::map<std::string,double> measurements={},bool locked=false,std::optional<std::uint32_t> color_rgb=std::nullopt,std::optional<double> class_probability=std::nullopt);
    ObjectProperties(const ObjectProperties&)=default;
    ObjectProperties& operator=(const ObjectProperties&)=delete;
    const std::map<std::string,std::string>& metadata() const;
    const std::map<std::string,double>& measurements() const;
    bool locked() const {return locked_;}
    std::optional<std::uint32_t> color_rgb() const {return color_;}
    std::optional<double> class_probability() const {return probability_;}
    std::shared_ptr<const ObjectProperties> with_edit_state(bool locked=false,std::optional<std::uint32_t> color_rgb=std::nullopt,std::optional<double> class_probability=std::nullopt) const;
    std::size_t payload_bytes() const {return bytes_;}
    bool equals(const ObjectProperties&) const;
private:
    struct Facts;
    std::shared_ptr<const Facts> facts_;
    bool locked_=false;
    std::optional<std::uint32_t> color_;
    std::optional<double> probability_;
    std::size_t bytes_=0;
};
}
