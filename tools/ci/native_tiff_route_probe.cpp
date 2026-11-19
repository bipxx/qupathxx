// SPDX-License-Identifier: AGPL-3.0-only
#include <qupathxx/object_properties.hpp>
#include <qupathxx/project.hpp>
#include <qupathxx/raw_image.hpp>
#include <qupathxx/rgb_preview.hpp>
#include <qupathxx/spatial_import.hpp>
#include <filesystem>
#include <iostream>
#include <stdexcept>

namespace {
void check(bool condition,const char* message) {
    if(!condition)throw std::runtime_error(message);
}
int probe(const std::filesystem::path& root) {
    using namespace qpx;
    const RgbNominalRequest window{1,0,2,2};
    for(const auto* name:{"pixels.ome.BTF","pixels.ome.tf2","pixels.ome.tf8"}) {
        const auto path=(root/name).u8string();
        const auto automatic=open_rgb_nominal_source(path);
        const auto explicit_tiff=open_rgb_nominal_source(path,"tiff");
        check(automatic->reader_id()=="tiff" && automatic->metadata().pixel_width_um==.5 &&
              automatic->metadata().pixel_height_um==2,"Automatic BigTIFF source or calibration differs");
        check(read_rgb_nominal_region(*automatic,window).bytes()==read_rgb_nominal_region(*explicit_tiff,window).bytes(),
              "BigTIFF RGB preview differs from explicit TIFF selection");
    }
    SpatialImportRequest request;
    request.image_path=(root/"pixels.ome.tf8").u8string();request.image_id=1;
    request.output_project=(root/"spatial.qpx").u8string();
    request.matrix=(root/"counts.mtx").u8string();request.features=(root/"features.tsv").u8string();
    request.positions=(root/"positions.tsv").u8string();request.options.features={"G"};
    request.options.position_columns={"observation_id","x_um","y_um"};
    request.position_unit=CoordinateUnit::Micrometre;
    const auto preview=preview_spatial_import(request);
    check(preview->image_server()->reader_id()=="tiff","Spatial import did not resolve native TIFF");
    publish_spatial_import(*preview);
    const auto document=Project::open(request.output_project)->document(1);
    const auto point=document->annotation(1);
    const auto position=point.geometry.centroid();
    check(position.x==1 && position.y==1 && point.properties->measurements().at("omics.count:G")==7,
          "Saved BigTIFF spatial point or count differs");
    RawRegionRequest raw;raw.region={1,1,0,1,1};
    check(std::get<std::vector<std::uint16_t>>(document->server()->read_raw(raw).storage())==std::vector<std::uint16_t>{4000},
          "Saved BigTIFF raw sample differs");
    return 0;
}
}

int wmain(int argc,wchar_t** argv) {
    try {
        check(argc==2,"Expected the BigTIFF fixture directory");
        return probe(argv[1]);
    }catch(const std::exception& error){
        std::cerr<<error.what()<<'\n';return 1;
    }
}
