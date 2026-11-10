#pragma once
#include "qupathxx/spatial_import.hpp"
#include "qupathxx/spatial_import_base.hpp"
namespace qpx {
class Project;
namespace detail::spatial_import {
void new_output(const std::string&);
void digest_argument(const std::string&);
std::string real_text(double);
SpatialImportBaseRequest base_request(const SpatialImportRequest&);
std::shared_ptr<Project> open_base(const SpatialImportBaseRequest&,const std::atomic<bool>*);
inline std::shared_ptr<Project> open_base(const SpatialImportRequest& request,const std::atomic<bool>* cancelled) {
    return open_base(base_request(request),cancelled);
}
void validate_positions(const Document&,const std::vector<Point>&,ObjectPlane,
    std::size_t work_bytes,std::size_t bytes_per_object,const std::atomic<bool>*);
}
}
