#pragma once
#include "qupathxx/ome_zarr.hpp"
#include "sha256.hpp"
#include <nlohmann/json.hpp>
#include <algorithm>
namespace qpx::detail {
struct ZarrLink {
    OmeZarrOpenOptions options;
    std::string expected;
};
inline std::shared_ptr<const ZarrLink> capture_zarr_link(const ImageServer& server){
    if(server.reader_id()!="ome-zarr")return {};
    // Capture immutable observed facts without filesystem work on the owner
    // thread. Reopening/analysis/review validates the expected directory.
    const auto source=server.reader_source();const auto options=server.ome_zarr_options();
    if(!source || source->scope!="directory_store" || !options || !server.raw_view())throw std::invalid_argument("Zarr project link requires native observed source/view");
    source->validate();auto link=std::make_shared<ZarrLink>();link->options=*options;link->options.view=*server.raw_view();link->expected=source->directory_sha256;return link;
}
inline std::string encode_zarr_link(const ZarrLink& link){
    link.options.validate();if(link.expected.size()!=64 || !std::all_of(link.expected.begin(),link.expected.end(),[](char c){return (c>='0' && c<='9') || (c>='a' && c<='f');}))throw std::invalid_argument("Invalid expected Zarr directory identity");
    const auto& o=link.options;const auto& s=o.source;
    nlohmann::json j={{"expected",link.expected},{"view",ome_zarr_view_encode(o.view)},{"work",o.work_bytes},{"metadata",s.metadata_bytes},{"chunk",s.chunk_bytes},{"encoded",s.encoded_bytes},{"inventory",s.inventory_bytes},{"files",s.max_files}};
    const auto body=std::string("qpx.ome-zarr-link/1\n")+j.dump()+"\n";Sha256 hash;hash.update(body.data(),body.size());return body+hash.finish();
}
inline std::shared_ptr<const ZarrLink> decode_zarr_link(const std::string& text){
    constexpr auto prefix="qpx.ome-zarr-link/1\n";if(text.size()>4096 || text.size()<100 || text.rfind(prefix,0))throw std::invalid_argument("Invalid Zarr project link payload");
    const auto n=text.size()-64;Sha256 hash;hash.update(text.data(),n);if(hash.finish()!=text.substr(n))throw std::invalid_argument("Zarr project link checksum mismatch");
    const auto j=nlohmann::json::parse(text.substr(std::char_traits<char>::length(prefix),n-std::char_traits<char>::length(prefix)));
    auto link=std::make_shared<ZarrLink>();link->expected=j.at("expected").get<std::string>();auto& o=link->options;auto& s=o.source;o.view=ome_zarr_view_decode(j.at("view").get<std::string>());
    const auto bounded=[&](const char* key){const auto& v=j.at(key);if(!v.is_number_integer() || v<=0 || v>1024ull*1024*1024)throw std::invalid_argument("Invalid Zarr link resource bound");return v.get<std::size_t>();};
    o.work_bytes=bounded("work");s.metadata_bytes=bounded("metadata");s.chunk_bytes=bounded("chunk");s.encoded_bytes=bounded("encoded");s.inventory_bytes=bounded("inventory");s.max_files=bounded("files");
    if(encode_zarr_link(*link)!=text)throw std::invalid_argument("Noncanonical Zarr project link");return link;
}
inline std::shared_ptr<const ZarrLink> unbound_zarr_link(std::shared_ptr<const ZarrLink> link){if(!link)return {};auto next=std::make_shared<ZarrLink>(*link);next->expected.clear();return next;}
}
