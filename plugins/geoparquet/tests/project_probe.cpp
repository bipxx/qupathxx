// SPDX-License-Identifier: AGPL-3.0-only
#include <qupathxx/object_properties.hpp>
#include <qupathxx/project.hpp>
#include <qupathxx/reader_plugins.hpp>
#include <iostream>
#include <map>
#include <stdexcept>
#include <string_view>

using namespace qpx;

void check(bool ok, const char* diagnostic) {
    if (!ok)
        throw std::runtime_error(diagnostic);
}

int main(int argc, char** argv) {
    try {
        check(argc == 4 || argc == 5, "Expected project, reader manifest and oracle");
        configure_reader_plugins(argv[2]);
        const auto project = Project::open_without_journal_recovery(argv[1]);
        const auto document = project->document(1);
        const std::string_view test(argv[3]);
        if (test == "seed-classes") {
            check(argc == 5, "Expected classified template path");
            document->set_class_definition(ObjectClass::definition("Tumor", 0x123456));
            project->save_new(argv[4]);
            return 0;
        }
        if (test == "compare-native" || test == "compare-registered") {
            check(argc == 5, "Expected WKB comparison project");
            const auto baseline = Project::open_without_journal_recovery(argv[4]);
            const auto reference = baseline->document(1);
            check(document->objects().size() == reference->objects().size(), "GeoArrow object count differs from WKB");
            for (const auto& [id, actual] : document->objects()) {
                const auto& expected = reference->objects().at(id);
                check(actual.geometry.equals(expected.geometry) && actual.plane == expected.plane &&
                      actual.name == expected.name && actual.class_name == expected.class_name,
                      "GeoArrow geometry, plane or labels differ from WKB");
                for (const auto* key : {"source_id", "source.geometry.type", "source.geometry.path",
                                        "source.geoparquet.source_id"})
                    check(actual.properties->metadata().at(std::string("qpx.annotation-plugin.") + key) ==
                          expected.properties->metadata().at(std::string("qpx.annotation-plugin.") + key),
                          "GeoArrow source identity or collection facts differ from WKB");
                if (test == "compare-registered") {
                    const auto& facts = actual.properties->metadata();
                    check(facts.at("qpx.annotation-plugin.source.geoparquet.crs") == "EPSG:3857" &&
                          facts.at("qpx.annotation-plugin.options").find(R"("source_to_pixels":[0.5,0.25,0,0,-0.25,500])")
                              != std::string::npos, "Registered GeoArrow CRS or affine identity differs");
                }
            }
            std::cout << "Verified exact persisted GeoArrow and WKB geometry equality\n";
            return 0;
        }
        const auto grouped = test == "collection";
        const auto classified = test == "version10";
        const auto geo_column = test == "alternate" ? "alternate" : "geometry";
        const auto version = classified ? "1.0.0" : "1.1.0";
        const auto crs = test == "registered" ? "EPSG:3857" : "null";
        std::map<std::string, const Annotation*> shapes;
        for (const auto& entry : document->objects()) {
            const auto& actual = entry.second;
            check(actual.properties && actual.plane == ObjectPlane(-1, 0, 0), "GeoParquet saved plane differs");
            const auto& facts = actual.properties->metadata();
            const std::string prefix = "qpx.annotation-plugin.";
            const auto& id = facts.at(prefix + "source_id");
            check(facts.at(prefix + "provider_id") == "org.qupathxx.geoparquet" &&
                  facts.at(prefix + "source.geoparquet.version") == version &&
                  facts.at(prefix + "source.geoparquet.geometry_column") == geo_column &&
                  facts.at(prefix + "source.geoparquet.crs") == crs,
                  "GeoParquet receipt facts differ");
            check(facts.at(prefix + "source.geoparquet.source_id") == id.substr(0, id.find('/')),
                  "GeoParquet root ID differs");
            if (test == "registered")
                check(facts.at(prefix + "options").find(R"("source_to_pixels":[0.5,0,500,0,-0.25,500])") !=
                          std::string::npos, "Saved WKB registration coefficients differ");
            check(shapes.emplace(id, &actual).second, "GeoParquet mapped IDs are not unique");
        }
        check(shapes.size() == (grouped ? 4 : 3), "Unexpected GeoParquet object count");
        const auto& point = *shapes.at(grouped ? "0001/0" : "0001");
        check(point.geometry.equals(Roi::points({{.5, .75}})), "Saved point differs");
        check(shapes.at("1")->geometry.equals(Roi::polyline({{1,.5}, {2,1.5}, {3,1}})), "Saved line differs");
        const auto& region = *shapes.at("μ");
        const auto expected_region = Roi::multipolygon({
            {{{0,0}, {4,0}, {4,3}, {0,3}}, {{{1,1}, {1,2}, {3,2}, {3,1}}}}
        });
        check(region.geometry.equals(expected_region) && region.geometry.area() == 10 &&
              !region.geometry.contains({2,1.5}), "Saved polygon or hole differs");
        check(point.name == "Spot" && shapes.at("1")->name == "1" && region.name == "Region",
              "GeoParquet optional names differ");
        if (grouped) {
            check(shapes.at("0001/1")->geometry.equals(Roi::points({{.25, .5}})) &&
                  point.properties->metadata().at("qpx.annotation-plugin.source.geometry.container.0.type") == "GEOMETRYCOLLECTION",
                  "Saved collection membership differs");
        }
        if (classified)
            check(point.class_name == "Tumor" && region.class_name.empty(), "Saved GeoParquet classes differ");
        std::cout << "Verified exact GeoParquet geometry, IDs and metadata after offline reopen\n";
        return 0;
    } catch (const std::exception& exc) {
        std::cerr << exc.what() << '\n';
        return 1;
    }
}
