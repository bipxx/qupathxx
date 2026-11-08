#include "density_formats.hpp"
#include "json_string.hpp"
namespace qpx::detail {
using Json=nlohmann::json;
Json density_engine(const BuildEnvironment& e) {
    return {{"source_tree_sha256",e.source_tree_sha256},{"compiler",e.compiler},{"compiler_flags",e.compiler_flags},
        {"configuration",e.configuration},{"platform",e.platform},{"geos_version",e.geos_version}};
}
const char* density_checkpoint_schema(DensityFormat format) {
    return format==DensityFormat::GuiV1?"qpx.gui-density-checkpoint/1":"qpx.project-density-checkpoint/1";
}
Json density_receipt(const ProjectDensityReport& report,const ProjectDensityImage& row,const SpatialDensityResult& result,DensityFormat format) {
    Json value;
    if(format==DensityFormat::ProjectV1 && !row.receipt_json.empty())value=Json::parse(row.receipt_json);
    else value={{"schema",density_checkpoint_schema(format)},{"project_id",report.project_id},{"image_id",row.id},
        {"engine",density_engine(*result.provenance()->environment)},{"checkpoint",row.checkpoint},{"checkpoint_image_id",row.checkpoint_image},
        {"checkpoint_sha256",row.checkpoint_sha256},{"record_id",row.record},{"result_sha256",result.sha256()},{"result_input_sha256",result.input_sha256()}};
    if(format==DensityFormat::ProjectV1){value["reuse"]=row.reuse;value["request_input_sha256"]=row.input_sha256;}
    return value;
}
Json project_image(const ProjectDensityImage& row) {
    if(row.state=="saved" || row.state=="reused") {
        auto value=Json::parse(row.receipt_json);value["state"]="complete";value["rows"]=row.rows;value["image_name"]=row.name;return value;
    }
    return {{"image_id",row.id},{"state",row.state},{"rows",row.rows},{"error",row.error}};
}
std::string density_image_json(const ProjectDensityImage& row,DensityFormat format) {
    if(format==DensityFormat::ProjectV1)return project_image(row).dump();
    return Json({{"image_id",row.id},{"image_name",row.name},{"state",row.state},{"error",row.error},
        {"record_id",row.record},{"reuse",row.reuse},{"rows",row.rows}}).dump();
}
std::string gui_report_json(const ProjectDensityReport& report) {
    std::string out="{\"schema\":\"qpx.gui-project-density/1\",\"state\":"+json_string(report.state)+",\"error\":"+json_string(report.error)+
        ",\"source\":"+json_string(report.source)+",\"source_sha256\":"+json_string(report.source_sha256)+",\"project_id\":"+json_string(report.project_id)+
        ",\"completed\":"+std::to_string(report.completed)+",\"computed\":"+std::to_string(report.computed)+",\"reused\":"+std::to_string(report.reused)+",\"failed\":"+std::to_string(report.failed)+",\"rows\":"+std::to_string(report.rows)+",\"images\":[";
    bool first=true;for(const auto& row:report.images) {
        if(!first)out+=',';first=false;
        out+="{\"image_id\":"+std::to_string(row.id)+",\"image_name\":"+json_string(row.name)+",\"state\":"+json_string(row.state)+",\"error\":"+json_string(row.error)+
            ",\"checkpoint\":"+json_string(row.checkpoint)+",\"checkpoint_image_id\":"+std::to_string(row.checkpoint_image)+",\"record_id\":"+std::to_string(row.record)+
            ",\"input_sha256\":"+json_string(row.input_sha256)+",\"result_sha256\":"+json_string(row.result_sha256)+
            ",\"reuse\":"+json_string(row.reuse)+",\"reuse_reason\":"+json_string(row.reuse_reason)+",\"checkpoint_sha256\":"+json_string(row.checkpoint_sha256)+",\"result_input_sha256\":"+json_string(row.result_input_sha256)+"}";
    }return out+"]}\n";
}

std::string density_report_json(const ProjectDensityReport& report,DensityFormat format) {
    if(format==DensityFormat::GuiV1)return gui_report_json(report);
    Json images=Json::array();for(const auto& row:report.images)if(row.state!="not_run")images.push_back(project_image(row));
    return Json({{"schema","qpx.project-density-report/1"},{"state",report.state},{"project_id",report.project_id},
        {"project",report.source},{"project_sha256",report.source_sha256},{"manifest",report.manifest},{"manifest_sha256",report.manifest_sha256},
        {"engine",density_engine(*build_environment())},{"selected",report.images.size()},{"images",images},{"rows",report.rows},{"error",report.error},
        {"computed",report.computed},{"reused",report.reused},{"failed",report.failed},{"not_run",report.not_run},
        {"csv_path",report.csv},{"report_path",report.report}}).dump(2,' ',true)+"\n";
}
}
