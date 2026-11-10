#include "expression_matrix_internal.hpp"
#include "h5ad_identity.hpp"
#ifdef QUPATHXX_HAS_PROJECT
#include <nlohmann/json.hpp>
namespace qpx::detail::matrix {
namespace {
std::string frame_text(const CoordinateFrame& f){
    f.validate();nlohmann::json j={{"id",f.id},{"source_id",f.source_id},{"unit",f.unit==CoordinateUnit::Pixel?"pixel":"micrometre"},{"c",f.plane.c},{"z",f.plane.z},{"t",f.plane.t},{"width",f.width},{"height",f.height},{"image_level_zero",f.image_level_zero},{"micrometres_per_pixel",nullptr}};
    if(f.micrometres_per_pixel)j["micrometres_per_pixel"]=*f.micrometres_per_pixel;const auto s=j.dump();bounded_utf8(s,4096);return s;
}
CoordinateFrame frame_value(const std::string& s){
    const auto j=nlohmann::json::parse(s);if(!j.is_object() || j.size()!=10)throw std::invalid_argument("Expression coordinate frame shape differs");
    CoordinateFrame f;f.id=j.at("id").get<std::string>();f.source_id=j.at("source_id").get<std::string>();const auto unit=j.at("unit").get<std::string>();if(unit!="pixel" && unit!="micrometre")throw std::invalid_argument("Expression coordinate unit unsupported");f.unit=unit=="pixel"?CoordinateUnit::Pixel:CoordinateUnit::Micrometre;
    const auto integer=[&](const char* key,std::int64_t minimum,std::int64_t maximum){const auto& v=j.at(key);if(!v.is_number_integer() || (v.is_number_unsigned() && v.get<std::uint64_t>()>std::uint64_t(maximum)))throw std::invalid_argument("Expression frame integer type/range differs");const auto n=v.get<std::int64_t>();if(n<minimum || n>maximum)throw std::invalid_argument("Expression frame integer out of range");return n;};
    f.plane=ObjectPlane(std::int32_t(integer("c",-1,65535)),std::uint32_t(integer("z",0,65535)),std::uint32_t(integer("t",0,65535)));
    f.width=integer("width",0,9007199254740992LL);f.height=integer("height",0,9007199254740992LL);f.image_level_zero=j.at("image_level_zero").get<bool>();const auto& pitch=j.at("micrometres_per_pixel");if(!pitch.is_null()){if(!pitch.is_array() || pitch.size()!=2)throw std::invalid_argument("Expression calibration requires two axes");f.micrometres_per_pixel=pitch.get<std::array<double,2>>();}f.validate();return f;
}
}
void store_meta(Database& db,const ExpressionMatrixMetadata& m){
    const bool old=legacy(m);std::map<std::string,std::string> values{{"schema",old?"qpx.expression-matrix/1":"qpx.expression-matrix/3"},{"domain",m.value_domain},{"format",m.input_profile},{"features",std::to_string(m.features)},{"observations",std::to_string(m.observations)},{"entries",std::to_string(m.entries)},{"matrix_sha256",m.input_sha256[0]},{"features_sha256",m.input_sha256[1]},{"observations_sha256",m.input_sha256[2]},{"source_sha256",m.source_sha256},{"complete","1"}};
    if(!old){if(!m.spatial_frame)throw std::invalid_argument("Schema2 matrix needs explicit coordinates/frame");values.emplace("value_kind",m.domain==ExpressionValueDomain::Counts?"counts":"continuous");values.emplace("coordinate_frame",frame_text(*m.spatial_frame));values.emplace("coordinate_sha256",m.spatial_sha256);values.emplace("input_matrix",m.input_matrix);values.emplace("input_spatial",m.input_spatial);}
    Statement insert(db,"INSERT INTO metadata VALUES(?,?)");for(const auto& [key,value]:values){insert.text(1,key);insert.text(2,value);insert.step();insert.reset();}
}
ExpressionMatrixMetadata load_meta(Database& db){
    Statement application(db,"PRAGMA application_id");if(!application.step() || application.number(0)!=0x51505845)throw std::invalid_argument("Not a qupathxx expression sidecar");
    Statement schema(db,"PRAGMA user_version");if(!schema.step())throw std::invalid_argument("Expression sidecar version absent");const auto version=schema.number(0);if(version!=1 && version!=2 && version!=3)throw std::invalid_argument("Unsupported expression sidecar version");
    Statement tables(db,"SELECT name,type,wr,strict FROM pragma_table_list WHERE schema='main' AND name IN ('metadata','features','observations','counts','coordinates')");std::size_t count=0;
    while(tables.step()){const auto name=tables.text(0,32);if(tables.text(1,16)!="table" || tables.number(3)!=1 || tables.number(2)!=(name=="counts" || name=="metadata"?1u:0u))throw std::invalid_argument("Expression sidecar table layout differs");++count;}if(count!=(version==1?4u:5u))throw std::invalid_argument("Expression sidecar tables missing");
    {Statement columns(db,"PRAGMA table_info(counts)");unsigned col=0;const char* names[]={"feature","observation","value"};while(columns.step()){if(col>=3 || columns.text(1,32)!=names[col] || columns.text(2,16)!=(col==2?"BLOB":"INTEGER") || columns.number(3)!=1 || columns.number(5)!=(col==2?0u:col+1))throw std::invalid_argument("Expression sparse index layout differs");++col;}if(col!=3)throw std::invalid_argument("Expression sparse index incomplete");}
    if(version>=2){Statement columns(db,"PRAGMA table_info(coordinates)");unsigned col=0;const char* names[]={"observation","x","y","sha256"};while(columns.step()){if(col>=4 || columns.text(1,32)!=names[col] || columns.text(2,16)!=(col==0?"INTEGER":col==3?"TEXT":"BLOB") || columns.number(3)!=1 || columns.number(5)!=(col==0?1u:0u))throw std::invalid_argument("Expression coordinate index layout differs");++col;}if(col!=4)throw std::invalid_argument("Expression coordinate index incomplete");}
    Statement rows(db,"SELECT key,value FROM metadata");std::map<std::string,std::string> values;const std::size_t fields=version==1?11:version==2?14:16;while(rows.step()){if(values.size()==fields || !values.emplace(rows.text(0,64),rows.text(1,4096)).second)throw std::invalid_argument("Expression metadata shape differs");}
    if(values.size()!=fields || values.at("schema")!=(version==1?"qpx.expression-matrix/1":version==2?"qpx.expression-matrix/2":"qpx.expression-matrix/3") || values.at("complete")!="1")throw std::invalid_argument("Incomplete/unsupported expression sidecar");
    ExpressionMatrixMetadata m;m.features=spatial_unsigned(values.at("features"));m.observations=spatial_unsigned(values.at("observations"));m.entries=spatial_unsigned(values.at("entries"));
    if(!m.features || m.features>1000000 || !m.observations || m.observations>1000000 || m.entries>1000000000)throw std::invalid_argument("Expression metadata dimensions exceed contract");m.input_profile=values.at("format");m.value_domain=values.at("domain");
    if(version==1){if(!legacy(m) || m.value_domain!="raw-counts")throw std::invalid_argument("Unsupported legacy expression format/domain");}
    else {
        if(m.input_profile!="h5ad-array/1" && m.input_profile!="h5ad-csr_matrix/1" && m.input_profile!="h5ad-csc_matrix/1")throw std::invalid_argument("Unsupported schema2 expression format");
        const auto& kind=values.at("value_kind");if(kind!="counts" && kind!="continuous")throw std::invalid_argument("Unsupported expression value kind");m.domain=kind=="counts"?ExpressionValueDomain::Counts:ExpressionValueDomain::Continuous;bounded_utf8(m.value_domain,128);
        if(m.value_domain.empty() || (m.domain==ExpressionValueDomain::Counts?m.value_domain!="raw-counts":m.value_domain=="raw-counts"))throw std::invalid_argument("Expression domain label differs from stored kind");
        m.spatial_frame=frame_value(values.at("coordinate_frame"));m.spatial_sha256=values.at("coordinate_sha256");digest(m.spatial_sha256);if(version==3){m.input_matrix=values.at("input_matrix");m.input_spatial=values.at("input_spatial");}
    }
    m.input_sha256={values.at("matrix_sha256"),values.at("features_sha256"),values.at("observations_sha256")};for(const auto& s:m.input_sha256)digest(s);
    if(version==3){H5adOptions p;p.matrix=m.input_matrix;p.spatial=m.input_spatial;p.domain=m.domain;p.value_domain=m.value_domain;p.frame=*m.spatial_frame;p.validate();if(h5ad_input_identity(m.input_sha256[0],p)!=m.input_sha256[1])throw std::invalid_argument("Stored H5AD selection/frame/domain recipe identity differs");}
    m.source_sha256=values.at("source_sha256");digest(m.source_sha256);if(source_hash(m)!=m.source_sha256)throw std::invalid_argument("Expression metadata identity differs");return m;
}
}
#endif
