// SPDX-License-Identifier: AGPL-3.0-only
#pragma once
#include "atomic_export.hpp"
#include "qupathxx/raw_image.hpp"
#include <cstring>

namespace qpx::detail {
inline std::string numpy_sample_descriptor(SampleType type) {
    const auto size=sample_bytes(type);const std::uint16_t one=1;const bool little=*reinterpret_cast<const unsigned char*>(&one)==1;
    const bool floating=type==SampleType::Float32 || type==SampleType::Float64;
    const bool signed_value=type==SampleType::Int8 || type==SampleType::Int16 || type==SampleType::Int32 || type==SampleType::Int64;
    std::string descriptor(1,size==1?'|':little?'<':'>');descriptor+=floating?'f':signed_value?'i':'u';descriptor+=std::to_string(size);
    return descriptor;
}
template <class Output>
inline void write_numpy_header(Output& file, const std::string& descriptor,
                               const std::string& shape) {
    std::string header="{'descr': "+descriptor+", 'fortran_order': False, 'shape': ("+shape+"), }";
    header.append((64-(10+header.size()+1)%64)%64,' ');header+='\n';
    const unsigned char magic[]={0x93,'N','U','M','P','Y',1,0,static_cast<unsigned char>(header.size()),static_cast<unsigned char>(header.size()>>8)};
    file.write(reinterpret_cast<const char*>(magic),sizeof(magic));file.write(header.data(),header.size());
}
template <class Output>
inline void write_raw_numpy(Output& file,const RawPixelBuffer& buffer) {
    write_numpy_header(file, "'"+numpy_sample_descriptor(buffer.sample_type())+"'",
        std::to_string(buffer.height())+", "+std::to_string(buffer.width())+", "+
        std::to_string(buffer.channels()));
    std::visit([&](const auto& values){const auto* p=reinterpret_cast<const char*>(values.data());auto n=values.size()*sizeof(typename std::decay_t<decltype(values)>::value_type);while(n) {const auto step=std::min<std::size_t>(n,65536);file.write(p,step);p+=step;n-=step;}},buffer.storage());
}

template <class Output>
inline void write_raw_numpy_with_validity(Output& file, const RawPixelBuffer& buffer) {
    const auto& valid = buffer.valid();
    const auto value_bytes = std::size_t(buffer.channels()) * sample_bytes(buffer.sample_type());
    const auto record_bytes = value_bytes + 1;
    const auto batch_pixels = std::max<std::size_t>(1, 65536 / record_bytes);
    std::vector<char> scratch(batch_pixels * record_bytes);
    const auto descriptor = "[('values', '" + numpy_sample_descriptor(buffer.sample_type()) +
                            "', (" + std::to_string(buffer.channels()) + ",)), ('valid', '|b1')]";
    write_numpy_header(file, descriptor,
                       std::to_string(buffer.height()) + ", " + std::to_string(buffer.width()));
    std::visit([&](const auto& values) {
        const auto* source = reinterpret_cast<const char*>(values.data());
        for (std::size_t offset = 0; offset < valid.size();) {
            const auto count = std::min(batch_pixels, valid.size() - offset);
            auto* target = scratch.data();
            for (std::size_t i = 0; i < count; ++i) {
                std::memcpy(target, source, value_bytes);
                target[value_bytes] = valid[offset + i] == 1;
                target += record_bytes;
                source += value_bytes;
            }
            file.write(scratch.data(), count * record_bytes);
            offset += count;
        }
    }, buffer.storage());
}
}
