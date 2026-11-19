// SPDX-License-Identifier: AGPL-3.0-only
#include <qupathxx/object_properties.hpp>
#include <qupathxx/project.hpp>
#include <qupathxx/raw_image.hpp>
#include <filesystem>
#include <iostream>
#include <stdexcept>

namespace {
void check(bool good,const char* message) {
    if(!good)throw std::runtime_error(message);
}
void compare_vectors(const std::filesystem::path& first,const std::filesystem::path& second) {
    const auto a=qpx::Project::open(first.u8string())->document(1);
    const auto b=qpx::Project::open(second.u8string())->document(1);
    const auto rows=a->objects(),other=b->objects();
    check(!rows.empty() && rows.size()==other.size(),"Vector object count changed");
    for(const auto& [id,row]:rows) {
        const auto& next=other.at(id);
        check(row.name==next.name && row.plane==next.plane && row.locked()==next.locked() &&
              row.geometry.equals(next.geometry),"Vector name, plane, lock or geometry changed");
        check(bool(row.properties) && bool(next.properties),"Missing preserved vector source facts");
        const auto& facts=row.properties->metadata();
        const auto& next_facts=next.properties->metadata();
        constexpr std::string_view prefix="qpx.annotation-plugin.source.";
        for(const auto& [key,value]:facts)
            if(key.compare(0,prefix.size(),prefix)==0)
                check(next_facts.at(key)==value,"Vector source facts changed");
    }
}
}

int wmain(int argc,wchar_t** argv) {
    try {
        if(argc==4 && std::wstring_view(argv[1])==L"--compare") {
            compare_vectors(argv[2],argv[3]);
            return 0;
        }
        if(argc==3) {
            qpx::Project project;
            check(project.add_tiff_image(std::filesystem::path(argv[1]).u8string(),{})==1,
                  "Expected one volume image");
            project.save_new(std::filesystem::path(argv[2]).u8string());
            return 0;
        }
        const bool custom=argc==4 && std::wstring_view(argv[2])==L"--coverage";
        check(argc==2 || custom,"Expected a saved native project");
        const std::wstring_view coverage=custom?argv[3]:L"";
        check(!custom || (coverage.size()==16*16 && coverage.find_first_not_of(L"01")==coverage.npos),
              "Expected a 16 by 16 binary coverage map");
        const auto project=qpx::Project::open(std::filesystem::path(argv[1]).u8string());
        const auto document=project->document(1);
        check(document->objects().size()==2,"Expected one saved mask and one vector shape");
        bool found_mask=false,found_point=false;
        for(const auto& [id,shape]:document->objects()) {
            check(bool(shape.properties),"Missing OME source facts");
            const auto& facts=shape.properties->metadata();
            const auto& source_id=facts.at("qpx.annotation-plugin.source_id");
            if(source_id=="Shape:point") {
                found_point=true;
                check(shape.geometry.equals(qpx::Roi::points({{1.25,2.5}})),"Mixed vector point changed");
                continue;
            }
            found_mask=true;
            check(source_id=="Shape:mask" && shape.locked() && shape.plane==qpx::ObjectPlane(-1,1,1) &&
                  shape.name=="Mask region","Mask source ID, name, lock or plane differs");
            const auto& parts=shape.geometry.polygon_parts();
            check(parts.size()==2 && parts[0].holes.size()+parts[1].holes.size()==1 &&
                  shape.geometry.area()==9,"Mask must retain both parts and one hole");
            for(int y=0;y<16;++y)for(int x=0;x<16;++x) {
                const bool expected=custom?coverage[y*16+x]==L'1':
                    (x>=2 && x<=4 && y>=3 && y<=5 && (x!=3 || y!=4)) || (x==6 && y==7);
                check(shape.geometry.contains({x+.5,y+.5})==expected,"Saved mask coverage differs at a pixel");
            }
            check(facts.count("qpx.annotation-plugin.source.ome.mask.bindata.attributes.0")==1,
                  "Saved mask is missing BinData attributes");
        }
        check(found_point && found_mask,"Missing one of the expected source IDs");
        std::cout<<"Verified saved mask coverage, source facts, lock and plane\n";
        return 0;
    } catch(const std::exception& error) {
        std::cerr<<error.what()<<'\n';
        return 1;
    }
}
