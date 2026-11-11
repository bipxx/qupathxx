#pragma once
#include "qupathxx/core.hpp"
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace qpx_test {
// Independent bytes/errors freeze the old P6 contract before the core and
// example module begin sharing the extracted decoder implementation.
inline void reader_plugin_ppm_oracle(const std::filesystem::path& root) {
    const auto write=[&](const char* name,const std::string& bytes) {
        const auto path=root/name;std::ofstream file(path,std::ios::binary);
        file.write(bytes.data(),static_cast<std::streamsize>(bytes.size()));file.close();
        if(!file)throw std::runtime_error("Cannot write owned P6 oracle fixture");return path;
    };
    const auto good=[&](const char* name,const std::string& encoded,int width,int height,const std::vector<std::uint8_t>& expected) {
        const auto path=write(name,encoded);const auto image=qpx::Image::read_ppm(path.u8string());
        if(image.width()!=width || image.height()!=height || image.pixels()!=expected)throw std::runtime_error("Independent P6 raster oracle changed: "+std::string(name));
    };
    good("leading-space.ppm","P6\n1 1\n255\n \n\t",1,1,{32,10,9});
    good("comments-crlf.ppm","# before magic\r\nP6\r\n# before dimensions\r\n2 # between dimensions\r\n1\r\n255\r\nABCDEF",2,1,{65,66,67,68,69,70});
    good("trailing.ppm","P6\n1 1\n255\nABCignored",1,1,{65,66,67});
    good("tab-separator.ppm","P6\n1 1\n255\tABC",1,1,{65,66,67});
    const auto invalid=[&](const char* name,const std::string& encoded,const char* expected) {
        const auto path=write(name,encoded);try {qpx::Image::read_ppm(path.u8string());}
        catch(const std::invalid_argument& error) {if(std::string(error.what())==expected)return;throw;}
        throw std::runtime_error("Missing original invalid P6 refusal: "+std::string(name));
    };
    const auto broken=[&](const char* name,const std::string& encoded,const char* expected) {
        const auto path=write(name,encoded);try {qpx::Image::read_ppm(path.u8string());}
        catch(const std::runtime_error& error) {if(std::string(error.what())==expected)return;throw;}
        throw std::runtime_error("Missing original incomplete P6 refusal: "+std::string(name));
    };
    invalid("wrong-magic.ppm","P3\n1 1\n255\nABC","only binary P6 PPM is supported");
    invalid("wrong-max.ppm","P6\n1 1\n256\nABC","PPM maximum must be 255");
    invalid("zero-width.ppm","P6\n0 1\n255\nABC","invalid image dimensions");
    invalid("negative-height.ppm","P6\n1 -1\n255\nABC","invalid image dimensions");
    invalid("integer-suffix.ppm","P6\n1x 1\n255\nABC","invalid PPM integer");
    invalid("missing-separator.ppm","P6\n1 1\n255","missing PPM raster separator");
    broken("empty.ppm","","incomplete PPM header");
    broken("incomplete.ppm","P6\n1","incomplete PPM header");
    broken("truncated.ppm","P6\n1 1\n255\nAB","truncated PPM raster");
    const auto missing=root/"nonexistent.ppm";try {qpx::Image::read_ppm(missing.u8string());}
    catch(const std::runtime_error& error) {if(std::string(error.what())=="cannot open image: "+missing.u8string())return;throw;}
    throw std::runtime_error("Missing original unreadable P6 refusal");
}
}
