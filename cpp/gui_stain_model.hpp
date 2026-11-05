#pragma once
#include "qupathxx/stain_estimation.hpp"
#include "qupathxx/stain_model_io.hpp"
#include "stain_model_io_internal.hpp"
#include "reader_file_guard.hpp"
#include "atomic_export.hpp"
#include <chrono>
#include <future>
#include <string>
#include <vector>
#include <memory>
#include <utility>
namespace qpx::detail {
// File workers own only strings/native transforms, never a Document or reader.
class GuiStainModel {
    struct Result {std::shared_ptr<const StainTransform> model;std::string error,digest;bool save=false;};
    std::future<Result> future_;
    std::string state_="idle",error_,digest_;
    template<class F> bool launch(const char* state,F work) {
        if(busy()) return false;
        state_=state;error_.clear();digest_.clear();
        try {future_=std::async(std::launch::async,std::move(work));return true;}
        catch(const std::exception& e) {fail(e.what());return false;}
    }
public:
    bool busy() const {return future_.valid();}
    bool finished() const {return !busy() && state_!="idle";}
    const std::string& state() const {return state_;}
    const std::string& error() const {return error_;}
    const std::string& digest() const {return digest_;}
    void fail(std::string error,const char* state="failed") {state_=state;error_=std::move(error);digest_.clear();}
    bool load(std::string path) {
        return launch("loading",[path=std::move(path)] {
            Result result;
            try {result.model=std::make_shared<const StainTransform>(load_stain_model(path));result.digest=stain_model_sha256(*result.model);}
            catch(const std::exception& e) {result.error=e.what();}
            return result;
        });
    }
    bool save(StainTransform model,std::string path,std::vector<std::pair<std::string,std::string>> sources) {
        return launch("saving",[model=std::move(model),path=std::move(path),sources=std::move(sources)] {
            Result result;result.save=true;
            try {
                const auto output=export_path(path);
                const auto guard=[&] {
                    for(const auto& source:sources) {
                        try {guard_file_output(source.first,source.second,output);}
                        catch(const std::invalid_argument&) {throw std::invalid_argument("Choose a different model path; this path aliases an open image or project file.");}
                    }
                };
                save_stain_model_guarded(model,output,guard);result.digest=stain_model_sha256(model);
            } catch(const std::exception& e) {result.error=e.what();}
            return result;
        });
    }
    std::shared_ptr<const StainTransform> poll() {
        if(!future_.valid() || future_.wait_for(std::chrono::seconds(0))!=std::future_status::ready) return {};
        try {
            auto result=future_.get();
            if(!result.error.empty()) {fail(std::move(result.error));return {};}
            state_=result.save?"saved":"loaded";digest_=std::move(result.digest);return std::move(result.model);
        } catch(const std::exception& e) {fail(e.what());return {};}
    }
};
}
