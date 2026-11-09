#pragma once
#include "expression_matrix_internal.hpp"
#include <functional>
#ifdef QUPATHXX_HAS_PROJECT
namespace qpx::detail::matrix {
class Writer {
public:
    Writer(const std::string&,ExpressionMatrixMetadata,ExpressionMatrixImportOptions,
        std::size_t reserved,const std::atomic<bool>*);
    ~Writer();Writer(const Writer&)=delete;
    void feature_row(std::uint64_t,const std::string& id,const std::string& name,const std::string& type);
    void observation_row(std::uint64_t,const std::string& id,std::optional<Point> = {});
    void count(std::uint64_t feature,std::uint64_t observation,std::uint64_t);
    void continuous(std::uint64_t feature,std::uint64_t observation,double);
    ExpressionMatrixImportResult finish(std::array<std::string,3>,std::uint64_t input_bytes,const std::function<void()>& verify);
private:
    struct Data;std::unique_ptr<Data> data_;
};
}
#endif
