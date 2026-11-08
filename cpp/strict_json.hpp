#pragma once
#include <nlohmann/json.hpp>
#include <set>
#include <vector>
#include <string>
#include <stdexcept>
namespace qpx::detail {
struct StrictJson : nlohmann::json_sax<nlohmann::json> {
    struct Frame { bool object; std::set<std::string> keys; };
    std::vector<Frame> frames;
    std::string label;
    explicit StrictJson(std::string context="GeoJSON") : label(std::move(context)) {}
    bool null() override { return true; }
    bool boolean(bool) override { return true; }
    bool number_integer(number_integer_t) override { return true; }
    bool number_unsigned(number_unsigned_t) override { return true; }
    bool number_float(number_float_t,const string_t&) override { return true; }
    bool string(string_t&) override { return true; }
    bool binary(binary_t&) override { return true; }
    bool start(bool object) {
        if(frames.size()>=128) throw std::length_error(label+" nesting exceeds 128 levels");
        frames.push_back({object,{}}); return true;
    }
    bool start_object(std::size_t) override { return start(true); }
    bool start_array(std::size_t) override { return start(false); }
    bool key(string_t& key) override {
        if(!frames.back().keys.insert(key).second) throw std::invalid_argument("duplicate JSON member");
        return true;
    }
    bool end_object() override { frames.pop_back(); return true; }
    bool end_array() override { frames.pop_back(); return true; }
    bool parse_error(std::size_t,const std::string&,const nlohmann::detail::exception& error) override {
        throw std::invalid_argument("invalid "+label+": "+error.what());
    }
};
}
