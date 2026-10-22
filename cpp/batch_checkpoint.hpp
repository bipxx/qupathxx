#pragma once
#include <atomic>
#include <cstdint>
#include <string>
struct sqlite3;
namespace qpx::detail {
struct BatchCheckpoint {
    std::string parameters_sha256,input_sha256,pixel_sha256,source_identity,image_id,image_name,parameters_json,csv,data_sha256,source_locator,source_file_identity;
    std::uint64_t source_generation=0,source_revision=0,objects=0,generated_bytes=0,max_file_bytes=1024ULL*1024*1024;
};
void write_batch_checkpoint(sqlite3*,const BatchCheckpoint&,const std::atomic<bool>*);
BatchCheckpoint read_batch_checkpoint(const std::string& path,const std::atomic<bool>* cancelled=nullptr,bool include_csv=false,std::size_t csv_limit=256*1024*1024);
std::string batch_json_quote(const std::string&);
}
