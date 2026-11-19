// SPDX-License-Identifier: AGPL-3.0-only
#include "format_plugin_profile.hpp"
#include "../format-common/wkb_geometry.hpp"
#include "../format-common/json_tree.hpp"
#include "../../cpp/include/qupathxx/detail/affine_numeric.hpp"
#include <sqlite3.h>
#include <array>
#include <charconv>
#include <limits>

namespace qpx_gpkg {
using namespace qpx_wkb;
namespace json = qpx_native_json;
constexpr std::size_t sqlite_heap_bytes=4u*1024*1024;
constexpr std::size_t private_work=sqlite_heap_bytes+128u*1024;

struct Options {
    std::string layer,name;
    std::int32_t epsg=0;
    std::array<double,6> affine{};
};
Options selection(qpx_reader_string_v1 options,qpx_reader_cancel_v1 cancel) {
    require(options.data && options.size && options.size<=4096,"GeoPackage requires explicit layer and coordinate options");
    Arena scratch(65536);
    json::Tree tree({options.data,std::size_t(options.size)},&scratch.memory,cancel,5,12,2);
    const auto& root=tree.root();
    tree.keys(root,{"layer","name_column","coordinate_space","source_crs","source_to_pixels"});
    const auto space=json::string(tree.get(root,"coordinate_space"));
    require(space=="image-pixels" || space=="projected-xy","GeoPackage coordinate_space must be image-pixels or projected-xy");
    const auto layer=json::string(tree.get(root,"layer"));
    const auto* option=tree.find(root,"name_column");
    const auto name=option?json::string(*option):std::string_view{};
    require(!layer.empty() && layer.size()<=256 && (!option || (!name.empty() && name.size()<=256)),
            "Invalid GeoPackage layer or name column");
    utf8(layer,cancel);
    utf8(name,cancel);
    Options selected{std::string(layer),std::string(name)};
    const auto* crs=tree.find(root,"source_crs");
    const auto* transform=tree.find(root,"source_to_pixels");
    if(space=="image-pixels") {
        require(!crs && !transform,"Image-pixel GeoPackage cannot specify a source CRS or registration");
        return selected;
    }
    require(crs && transform,"Projected GeoPackage requires source_crs and source_to_pixels");
    const auto identifier=json::string(*crs);
    require(identifier.size()>5 && identifier.size()<=14 && identifier.substr(0,5)=="EPSG:" &&
            identifier[5]>='1' && identifier[5]<='9',"Expected a positive canonical EPSG code");
    const auto parsed=std::from_chars(identifier.data()+5,identifier.data()+identifier.size(),selected.epsg);
    require(parsed.ec==std::errc{} && parsed.ptr==identifier.data()+identifier.size(),
            "Expected a positive canonical EPSG code");
    const auto& values=tree.array(*transform);
    require(values.size()==selected.affine.size(),"GeoPackage affine requires six row-major coefficients");
    for(std::size_t i=0;i<values.size();++i)selected.affine[i]=json::real(tree.nodes[values[i]]);
    try {
        qpx::detail::affine_inverse_linear(selected.affine);
    } catch(const std::invalid_argument&) {
        fail("GeoPackage affine must be invertible and well-conditioned",QPX_READER_INVALID_REQUEST);
    }
    return selected;
}

struct Database {
    qpx_reader_cancel_v1 cancel;
    std::unique_ptr<sqlite3,decltype(&sqlite3_close)> db{nullptr,sqlite3_close};
    static int progress(void* context) {
        const auto c=*static_cast<qpx_reader_cancel_v1*>(context);
        return c.is_cancelled && c.is_cancelled(c.context);
    }
    Database(Bytes snapshot,qpx_reader_cancel_v1 c):cancel(c) {
        valid(snapshot.size>=100 && snapshot.sub(0,16).view()==std::string_view("SQLite format 3\0",16) &&
              snapshot.be32(68)==0x47504b47,"Not a GeoPackage SQLite database");
        const auto version=snapshot.be32(60);
        valid(version>=10200 && version<=10400,"Unsupported GeoPackage specification version");
        valid(snapshot.data[18]==1 && snapshot.data[19]==1,"GeoPackage requires a closed rollback-mode database");
        sqlite3_hard_heap_limit64(sqlite_heap_bytes);
        sqlite3* opened=nullptr;
        const auto status=sqlite3_open_v2(":memory:",&opened,
            SQLITE_OPEN_READWRITE|SQLITE_OPEN_CREATE|SQLITE_OPEN_PRIVATECACHE|SQLITE_OPEN_NOMUTEX,nullptr);
        db.reset(opened);
        valid(status==SQLITE_OK,"Cannot open private GeoPackage decoder");
        valid(sqlite3_db_config(db.get(),SQLITE_DBCONFIG_TRUSTED_SCHEMA,0,nullptr)==SQLITE_OK &&
              sqlite3_db_config(db.get(),SQLITE_DBCONFIG_DEFENSIVE,1,nullptr)==SQLITE_OK &&
              sqlite3_db_config(db.get(),SQLITE_DBCONFIG_LOOKASIDE,nullptr,0,0)==SQLITE_OK,
              "Cannot constrain GeoPackage decoder");
        sqlite3_progress_handler(db.get(),4096,progress,&cancel);
        sqlite3_limit(db.get(),SQLITE_LIMIT_LENGTH,2*1024*1024);
        sqlite3_limit(db.get(),SQLITE_LIMIT_SQL_LENGTH,16384);
        sqlite3_limit(db.get(),SQLITE_LIMIT_ATTACHED,0);
        valid(sqlite3_deserialize(db.get(),"main",const_cast<unsigned char*>(snapshot.data),
                  sqlite3_int64(snapshot.size),sqlite3_int64(snapshot.size),SQLITE_DESERIALIZE_READONLY)==SQLITE_OK,
              "Cannot attach exact read-only GeoPackage image");
        valid(sqlite3_exec(db.get(),"PRAGMA query_only=ON; PRAGMA temp_store=MEMORY; PRAGMA cache_size=-256;",
                          nullptr,nullptr,nullptr)==SQLITE_OK,"Cannot bound GeoPackage database work");
    }
};

struct Query {
    Database& db;
    std::unique_ptr<sqlite3_stmt,decltype(&sqlite3_finalize)> sql{nullptr,sqlite3_finalize};
    Query(Database& database,const std::string& command):db(database) {
        sqlite3_stmt* statement=nullptr;
        const auto result=sqlite3_prepare_v2(db.db.get(),command.c_str(),int(command.size()),&statement,nullptr);
        sql.reset(statement);
        poll(db.cancel);
        valid(result==SQLITE_OK && bool(sql),"GeoPackage SQL schema or selected column is invalid");
    }
    void bind(int index,std::string_view value) {
        valid(sqlite3_bind_text(sql.get(),index,value.data(),int(value.size()),SQLITE_TRANSIENT)==SQLITE_OK,
              "Cannot bind GeoPackage table selection");
    }
    void bind(int index,std::int64_t value) {
        valid(sqlite3_bind_int64(sql.get(),index,value)==SQLITE_OK,"Cannot bind GeoPackage SRS selection");
    }
    bool next() {
        poll(db.cancel);
        const auto result=sqlite3_step(sql.get());
        poll(db.cancel);
        valid(result==SQLITE_DONE || result==SQLITE_ROW,"GeoPackage SQL read failed or exceeded limits");
        return result==SQLITE_ROW;
    }
    std::string_view text(int i,std::size_t maximum=256) const {
        valid(sqlite3_column_type(sql.get(),i)==SQLITE_TEXT,"GeoPackage selected text column has another type");
        const auto* data=reinterpret_cast<const char*>(sqlite3_column_text(sql.get(),i));
        const auto n=std::size_t(sqlite3_column_bytes(sql.get(),i));
        valid(data && n<=maximum,"GeoPackage selected text exceeds limits");
        std::string_view result(data,n);
        utf8(result,db.cancel);
        return result;
    }
    std::int64_t integer(int i) const {
        valid(sqlite3_column_type(sql.get(),i)==SQLITE_INTEGER,"GeoPackage selected integer has another type");
        return sqlite3_column_int64(sql.get(),i);
    }
    Bytes blob(int i) const {
        valid(sqlite3_column_type(sql.get(),i)==SQLITE_BLOB,"GeoPackage feature geometry must be a nonempty SQL BLOB");
        const auto* data=static_cast<const std::uint8_t*>(sqlite3_column_blob(sql.get(),i));
        const auto n=std::size_t(sqlite3_column_bytes(sql.get(),i));
        valid(data && n<=1024*1024,"GeoPackage geometry BLOB exceeds one MiB");
        return {data,n};
    }
};

std::string quote_identifier(std::string_view field) {
    std::string name="\"";
    for (auto ch : field) {
        if (ch=='"') name.push_back('"');
        name.push_back(ch);
    }
    name.push_back('"');
    return name;
}

void table(Database& db,std::string_view name) {
    Query query(db,"SELECT type FROM pragma_table_list WHERE schema='main' AND name=?1 COLLATE BINARY");
    query.bind(1,name);
    valid(query.next() && query.text(0)=="table" && !query.next(),"GeoPackage selection must be an ordinary main-schema table");
}

struct Layer {
    std::string geometry, fid;
    unsigned type=0;
    std::int32_t srs=-1;
};

bool projected_wkt(std::string_view text) {
    constexpr std::string_view whitespace=" \t\r\n";
    const auto first=text.find_first_not_of(whitespace);
    if(first==text.npos)return false;
    text.remove_prefix(first);
    const auto open=text.find('[');
    if(open==text.npos)return false;
    const auto keyword=text.substr(0,text.find_last_not_of(whitespace,open-1)+1);
    if(keyword!="PROJCS" && keyword!="PROJCRS" && keyword!="PROJECTEDCRS")return false;
    const auto name=text.find_first_not_of(whitespace,open+1);
    if(name==text.npos || text[name]!='"')return false;
    unsigned depth=0;
    bool quoted=false;
    bool component=false;
    for(std::size_t i=open;i<text.size();++i) {
        const auto value=text[i];
        if(value=='"')quoted=!quoted;
        if(quoted)continue;
        if(value=='[' && ++depth>32)return false;
        if(value==',' && depth==1)component=true;
        if(value==']' && --depth==0)return component && text.find_first_not_of(whitespace,i+1)==text.npos;
    }
    return false;
}

bool epsg_authority(std::string_view value) {
    return value.size()==4 && (value[0]=='E'||value[0]=='e') && (value[1]=='P'||value[1]=='p') &&
        (value[2]=='S'||value[2]=='s') && (value[3]=='G'||value[3]=='g');
}

Layer layer(Database& db,const Options& options) {
    for (const auto name : {"gpkg_geometry_columns","gpkg_spatial_ref_sys","gpkg_contents"}) table(db,name);
    table(db,options.layer);
    Query match(db,"SELECT g.column_name,g.geometry_type_name,g.srs_id,g.z,g.m,c.srs_id,c.data_type "
                   "FROM main.gpkg_geometry_columns AS g JOIN main.gpkg_contents AS c "
                   "ON g.table_name=c.table_name COLLATE BINARY WHERE g.table_name=?1 COLLATE BINARY");
    match.bind(1,options.layer);
    valid(match.next(),"GeoPackage selected layer has no feature metadata");
    const auto srs=match.integer(2);
    if(options.epsg) {
        valid(srs==match.integer(5) && srs>=INT32_MIN && srs<=INT32_MAX && srs!=-1 && srs!=0,
              "Projected GeoPackage requires matching contents and geometry-column SRS IDs");
    } else {
        valid(srs==-1 && match.integer(5)==-1,"GeoPackage layer must use undefined Cartesian SRS -1");
    }
    valid(match.integer(3)==0 && match.integer(4)==0 && match.text(6)=="features","GeoPackage layer must contain XY features without Z/M");
    Layer selected;
    selected.srs=static_cast<std::int32_t>(srs);
    selected.geometry=match.text(0);
    const auto type=match.text(1);
    if (type!="GEOMETRY") {
        const auto found=std::find(names.begin()+1,names.end(),type);
        valid(found!=names.end(),"Unsupported GeoPackage declared geometry type");
        selected.type=unsigned(found-names.begin());
    }
    valid(!match.next(),"GeoPackage selected layer has duplicate metadata");
    Query crs(db,"SELECT organization,organization_coordsys_id,definition FROM main.gpkg_spatial_ref_sys WHERE srs_id=?1");
    crs.bind(1,srs);
    if(options.epsg) {
        valid(crs.next() && epsg_authority(crs.text(0)) && crs.integer(1)==options.epsg,
              "Selected GeoPackage source_crs differs from the declared EPSG");
        valid(projected_wkt(crs.text(2,65536)) && !crs.next(),
              "GeoPackage registration requires a projected CRS declaration");
    } else {
        valid(crs.next() && crs.text(0)=="NONE" && crs.integer(1)==-1 && crs.text(2)=="undefined" && !crs.next(),
              "GeoPackage SRS -1 is not declared undefined Cartesian");
    }
    Query columns(db,"SELECT name,type,pk,hidden FROM pragma_table_xinfo(?1,'main')");
    columns.bind(1,options.layer);
    bool geometry=false,name=options.name.empty();
    while (columns.next()) {
        const auto column=columns.text(0);
        const auto pk=columns.integer(2),hidden=columns.integer(3);
        if (pk) {
            valid(pk==1 && !hidden && selected.fid.empty() && columns.text(1)=="INTEGER",
                  "GeoPackage requires a single integer primary key");
            selected.fid=column;
        }
        if (column==selected.geometry && !hidden) geometry=true;
        if (column==options.name && !hidden) name=true;
    }
    valid(geometry && name && !selected.fid.empty(),"GeoPackage geometry, name or feature ID column is absent");
    return selected;
}

Node geometry(Bytes blob,const Layer& schema,bool projected,std::pmr::memory_resource* memory,qpx_reader_cancel_v1 cancel) {
    valid(blob.size>=8 && blob.data[0]=='G' && blob.data[1]=='P' && !blob.data[2],
          "Invalid GeoPackage geometry header");
    const auto flags=blob.data[3];
    const bool little=flags&1;
    const unsigned envelope=(flags>>1)&7;
    valid(!(flags&0xf0) && envelope<=1,"Unsupported extended, empty or non-XY GeoPackage geometry");
    valid(blob.number(4,4,!little)==static_cast<std::uint32_t>(schema.srs),
          projected?"GeoPackage feature SRS differs from the selected layer":"GeoPackage feature SRS must be -1");
    const auto start=envelope?40u:8u;
    valid(blob.size>start,"Truncated GeoPackage geometry");
    Wkb reader{blob.sub(start,blob.size-start),0,memory,{0,0,cancel,projected}};
    auto node=reader.value(0,schema.type);
    valid(reader.at==reader.bytes.size,"Trailing GeoPackage WKB bytes");
    if (envelope) {
        Bounds bounds;
        bounds.include(node,cancel);
        Wkb box{blob.sub(8,32),0,memory,{0,0,cancel}};
        for (const auto expected : {bounds.x_min,bounds.x_max,bounds.y_min,bounds.y_max})
            valid(box.real(little)==expected,"GeoPackage XY envelope differs from its WKB");
    }
    return node;
}

struct Definition {
    static constexpr const char *id="org.qupathxx.geopackage",*format="gpkg",
        *name="GeoPackage pixel annotations",*contract="gpkg-cartesian-image-pixel/2",
        *profile=QPX_FORMAT_PLUGIN_PROFILE_ID;
    static constexpr char version[]=QPX_FORMAT_PLUGIN_PROVIDER_VERSION;
    static void validate_options(qpx_reader_string_v1 options) {
        require(options.size && options.size<=4096,"GeoPackage requires explicit layer and coordinate options");
    }
    static void decode(Handle& h,Bytes snapshot,const qpx_reader_open_v1& request) {
        const std::string path(request.path.data,std::size_t(request.path.size));
        for (const auto suffix : {"-journal","-wal","-shm"}) {
            std::error_code error;
            const auto status=std::filesystem::symlink_status(std::filesystem::u8path(path+suffix),error);
            valid((!error || error==std::errc::no_such_file_or_directory) && status.type()==std::filesystem::file_type::not_found,
                  "Close or checkpoint GeoPackage before import; SQLite sidecar present");
        }
        const auto selected=selection(request.options,request.cancel);
        Database database(snapshot,request.cancel);
        const auto schema=layer(database,selected);
        const auto fid=quote_identifier(schema.fid);
        const auto label=selected.name.empty()?"NULL":quote_identifier(selected.name);
        Query rows(database,"SELECT "+fid+","+quote_identifier(schema.geometry)+","+label+
                            " FROM main."+quote_identifier(selected.layer)+" ORDER BY "+fid);
        auto* memory=&h.arena.memory;
        while (rows.next()) {
            const auto feature=std::to_string(rows.integer(0));
            auto node=geometry(rows.blob(1),schema,selected.epsg!=0,memory,request.cancel);
            if(selected.epsg) {
                auto project=[&](Point& source) {
                    const auto xy=qpx::detail::affine_map_xy(selected.affine,source.x,source.y);
                    source={xy[0],xy[1]};
                };
                try {
                    visit_points(node,request.cancel,project);
                } catch(const std::invalid_argument&) {
                    fail("GeoPackage affine coordinate overflow");
                }
            }
            const auto title=sqlite3_column_type(rows.sql.get(),2)==SQLITE_NULL?std::string_view{}:rows.text(2,4096);
            Facts facts(memory);
            fact(facts,"gpkg.layer",selected.layer);
            fact(facts,"gpkg.fid",feature);
            fact(facts,"gpkg.srs_id",std::to_string(schema.srs));
            if(selected.epsg)fact(facts,"gpkg.source_crs","EPSG:"+std::to_string(selected.epsg));
            emit(h,node,"",facts,title,request.cancel,"gpkg-fid-"+feature);
        }
        valid(!h.rows.empty(),"GeoPackage selected layer has no features");
    }
};
} // namespace qpx_gpkg

extern "C" QPX_ANNOTATION_METADATA_EXPORT qpx_reader_status_v1 QPX_FORMAT_CALL
qpx_annotation_format_entry_v3(std::uint32_t abi,qpx_annotation_format_api_v3* out,qpx_reader_text_v1* error) {
    using Base=qpx_annotation_source::Plugin<qpx_gpkg::Definition,3,
        qpx_gpkg::Definition::version,qpx_gpkg::private_work>;
    return Base::entry(abi,out,error);
}
