// SPDX-License-Identifier: AGPL-3.0-only
#include <qupathxx/object_properties.hpp>
#include <qupathxx/project.hpp>
#include <qupathxx/reader_plugins.hpp>
#include <iostream>
#include <map>
#include <stdexcept>
#include <string_view>

using namespace qpx;

void check(bool success, const char* what) {
    if (!success)
        throw std::runtime_error(what);
}

int main(int argc, char** argv) {
    try {
        check(argc == 4 || argc == 5, "Expected project, image reader manifest and case");
        const std::string_view test(argv[3]);
        configure_reader_plugins(argv[2]);
        const auto project = Project::open_without_journal_recovery(argv[1]);
        const auto document = project->document(1);
        if (test == "seed-classes") {
            check(argc == 5, "Expected classified template destination");
            document->set_class_definition(ObjectClass::definition("Tumor", 0x123456));
            document->set_class_definition(ObjectClass::definition("Stroma", 0x654321));
            project->save_new(argv[4]);
            return 0;
        }
        const std::string_view provider = argc == 5 ? argv[4] : "org.qupathxx.arrow-points";
        std::map<std::string, const Annotation*> points;
        for (const auto& [id, row] : document->objects()) {
            check(row.properties && row.plane == ObjectPlane(-1, 0, 0), "Saved plane differs");
            const auto& metadata = row.properties->metadata();
            check(metadata.at("qpx.annotation-plugin.provider_id") == provider,
                  "Saved point provider differs");
            check(points.emplace(metadata.at("qpx.annotation-plugin.source_id"), &row).second,
                  "Duplicate saved point ID");
        }
        check(points.size() == 3, "Expected three saved points");
        const auto& first = *points.at("0001");
        const auto& second = *points.at("1");
        const auto& third = *points.at("μ");
        if (test == "integer" || test == "integer-dictionaries") {
            check(first.geometry.equals(Roi::points({{0,1}})) &&
                  second.geometry.equals(Roi::points({{1,2}})) &&
                  third.geometry.equals(Roi::points({{3,0}})), "Integer coordinates differ");
            check(first.properties->metadata().at("qpx.annotation-plugin.source_class") == "tumor" &&
                  second.properties->metadata().at("qpx.annotation-plugin.source_class").empty() &&
                  third.properties->metadata().at("qpx.annotation-plugin.source_class") == "stroma",
                  "Source classes differ");
            check(first.class_name == "Tumor" && second.class_name.empty() && third.class_name == "Stroma",
                  "Native class mapping differs");
        } else {
            check(first.geometry.equals(Roi::points({{.5,.75}})) &&
                  second.geometry.equals(Roi::points({{1.25,1.25}})) &&
                  third.geometry.equals(Roi::points({{3.5,2.5}})), "Float coordinates differ");
        }
        check(first.name == "Spot A" && second.name == "1" && third.name == "Spot C", "Saved names differ");
        std::cout << "Verified three exact point annotations after offline project reopen\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
