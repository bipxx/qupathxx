#pragma once
#include <algorithm>
#include <atomic>
#include <cstdio>
#include <filesystem>
#include <random>
#include <stdexcept>
#include <string>
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <io.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <share.h>
#else
#include <fcntl.h>
#include <unistd.h>
#endif
namespace qpx::detail {
inline void export_checkpoint(const std::atomic<bool>* cancelled) {if(cancelled && cancelled->load(std::memory_order_relaxed)) throw std::runtime_error("export cancelled");}
inline std::string export_path(const std::string& path) {
    if(path.empty() || path.size()>32768 || path.find('\0')!=std::string::npos) throw std::invalid_argument("invalid export path");
    auto result=std::filesystem::absolute(std::filesystem::u8path(path)).lexically_normal().u8string();
    if(result.size()>32768) throw std::invalid_argument("export path is too long");return result;
}
class AtomicExportFile {
    std::filesystem::path destination_,temporary_;
    int fd_=-1;std::size_t limit_,bytes_=0;bool committed_=false;
    const std::atomic<bool>* cancelled_;
    bool close() {
        if(fd_<0) return true;
#ifdef _WIN32
        const auto code=_close(fd_);
#else
        const auto code=::close(fd_);
#endif
        fd_=-1;return code==0;
    }
public:
    AtomicExportFile(const std::string& path,std::size_t limit,const std::atomic<bool>* cancelled=nullptr):destination_(std::filesystem::u8path(export_path(path))),limit_(limit),cancelled_(cancelled) {
        export_checkpoint(cancelled_);temporary_=destination_;std::random_device random;constexpr char hex[]="0123456789abcdef";std::string suffix=".qpx-tmp-";
        for(int i=0;i<24;++i) suffix.push_back(hex[random()&15]);temporary_+=std::filesystem::u8path(suffix);
#ifdef _WIN32
        if(_wsopen_s(&fd_,temporary_.c_str(),_O_WRONLY|_O_CREAT|_O_EXCL|_O_BINARY,_SH_DENYRW,_S_IREAD|_S_IWRITE)!=0) fd_=-1;
#else
        fd_=::open(temporary_.c_str(),O_WRONLY|O_CREAT|O_EXCL,0600);
#endif
        if(fd_<0) throw std::runtime_error("cannot create export temporary file");
    }
    ~AtomicExportFile() {close();if(!committed_) {std::error_code error;std::filesystem::remove(temporary_,error);}}
    AtomicExportFile(const AtomicExportFile&)=delete;
    std::size_t bytes() const {return bytes_;}
    void write(const char* data,std::size_t size) {
        if(size>limit_-bytes_) throw std::length_error("export exceeds byte limit");
        for(std::size_t at=0;at<size;) {
            export_checkpoint(cancelled_);const auto count=std::min<std::size_t>(size-at,65536);
#ifdef _WIN32
            const auto written=_write(fd_,data+at,static_cast<unsigned>(count));
#else
            const auto written=::write(fd_,data+at,count);
#endif
            if(written<=0) throw std::runtime_error("export write failed");at+=static_cast<std::size_t>(written);bytes_+=static_cast<std::size_t>(written);
        }
    }
    void commit() {
        export_checkpoint(cancelled_);
#ifdef _WIN32
        if(_commit(fd_)!=0) throw std::runtime_error("export flush failed");
#else
        if(::fsync(fd_)!=0) throw std::runtime_error("export flush failed");
#endif
        if(!close()) throw std::runtime_error("export close failed");export_checkpoint(cancelled_);
#ifdef _WIN32
        if(!MoveFileExW(temporary_.c_str(),destination_.c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH)) throw std::runtime_error("export replacement failed");
#else
        if(::rename(temporary_.c_str(),destination_.c_str())!=0) throw std::runtime_error("export replacement failed");
#endif
        committed_=true;
    }
};
}
