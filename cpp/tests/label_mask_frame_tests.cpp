#include "qupathxx/label_mask_frame.hpp"
#include <cmath>
#include <initializer_list>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>
#include <utility>

using namespace qpx;
namespace {
void check(bool value, const char* message) {
    if (!value)
        throw std::runtime_error(message);
}
template <class F> void refuses(const char* name, F f) {
    try {
        f();
    } catch (const std::invalid_argument&) {
        return;
    }
    throw std::runtime_error(std::string("Invalid label-mask frame accepted: ") + name);
}
LabelMaskFrame xy(std::uint64_t width, std::uint64_t height) {
    return {{{"y", "", height, 1, 0}, {"x", "", width, 1, 0}}};
}
LabelMaskFrame prefix(LabelMaskFrame frame, std::initializer_list<LabelMaskAxisFrame> axes) {
    frame.axes.insert(frame.axes.begin(), axes);
    return frame;
}
LabelMaskTargetFrame target(std::int64_t width, std::int64_t height, RegionRequest crop) {
    LabelMaskTargetFrame result;
    result.width = width;
    result.height = height;
    result.crop = crop;
    return result;
}
void expected(const LabelMaskCropLink& link, std::int64_t x, std::int64_t y,
               std::int64_t width, std::int64_t height, std::int64_t origin_x,
               std::int64_t origin_y) {
    check(link.source_crop.x == x && link.source_crop.y == y && link.source_crop.level == 0 &&
              link.source_crop.width == width && link.source_crop.height == height &&
              link.label_origin_x_in_reference == origin_x &&
              link.label_origin_y_in_reference == origin_y,
          "Label-mask link differs from the literal expected integer crop/origin");
}
void spatial_links() {
    const auto reference = xy(10, 8);
    expected(validate_link_label_mask_frames(reference, reference, {},
                                               target(10, 8, {0, 0, 0, 10, 8})),
             0, 0, 10, 8, 0, 0);
    auto shifted = xy(5, 4);
    shifted.axes[0].translation = 2;
    shifted.axes[1].translation = 3;
    expected(validate_link_label_mask_frames(shifted, reference, {},
                                               target(10, 8, {4, 3, 0, 3, 2})),
             1, 1, 3, 2, 3, 2);
    auto negative = xy(20, 15);
    negative.axes[0].translation = -3;
    negative.axes[1].translation = -4;
    expected(validate_link_label_mask_frames(negative, reference, {},
                                               target(10, 8, {1, 2, 0, 5, 4})),
             5, 5, 5, 4, -4, -3);
    expected(validate_link_label_mask_frames(negative, reference, {},
                                               target(10, 8, {0, 0, 0, 10, 8})),
             4, 3, 10, 8, -4, -3);
    const LabelMaskFrame physical_reference{{{"y", "millimeter", 8, .002, .020},
                                             {"x", "micrometer", 10, .25, 10}}};
    const LabelMaskFrame physical_label{{{"y", "nanometer", 20, 2000, 18000},
                                         {"x", "nanometer", 20, 250, 10250}}};
    auto calibrated = target(10, 8, {2, 1, 0, 4, 3});
    calibrated.pixel_width_um = .25;
    calibrated.pixel_height_um = 2;
    expected(validate_link_label_mask_frames(physical_label, physical_reference, {}, calibrated),
             1, 2, 4, 3, 1, -1);
    check(physical_label.axes[0].scale == 2000 &&
              physical_label.axes[1].translation == 10250 &&
              physical_reference.axes[0].translation == .020,
          "Frame linking altered original unit/transform metadata");
    auto common_origin = reference;
    common_origin.axes[0].translation = 1e20;
    common_origin.axes[1].translation = -1e20;
    expected(validate_link_label_mask_frames(common_origin, common_origin, {},
                                               target(10, 8, {1, 2, 0, 3, 4})),
             1, 2, 3, 4, 0, 0);
    // Physical calibration remains explicit per axis; no unit is inferred on Y.
    auto partial = reference;
    partial.axes[1].unit = "micrometer";
    partial.axes[1].scale = .25;
    auto partial_target = target(10, 8, {0, 0, 0, 2, 2});
    partial_target.pixel_width_um = .25;
    expected(validate_link_label_mask_frames(partial, partial, {}, partial_target),
             0, 0, 2, 2, 0, 0);
}
void declared_tolerances() {
    check(label_mask_frame_relative_tolerance == 1e-12 &&
              label_mask_frame_index_tolerance == 1e-9 &&
              label_mask_frame_origin_limit == 4503599627370496LL,
          "Declared geometry tolerance/domain changed");
    auto reference = xy(10, 8);
    reference.axes[1].unit = "micrometer";
    auto labels = reference;
    labels.axes[1].scale = 1 + 5e-13;
    labels.axes[1].translation = 1 + 5e-10;
    auto declared = target(10, 8, {2, 0, 0, 2, 2});
    declared.pixel_width_um = 1 + 5e-13;
    expected(validate_link_label_mask_frames(labels, reference, {}, declared),
             1, 0, 2, 2, 1, 0);
    auto bad_scale = labels;
    bad_scale.axes[1].scale = 1 + 2e-12;
    refuses("scale beyond relative tolerance", [&] {
        validate_link_label_mask_frames(bad_scale, reference, {}, declared);
    });
    auto bad_origin = labels;
    bad_origin.axes[1].translation = 1 + 2e-9;
    refuses("origin beyond index tolerance", [&] {
        validate_link_label_mask_frames(bad_origin, reference, {}, declared);
    });
    auto bad_calibration = declared;
    bad_calibration.pixel_width_um = 1 + 2e-12;
    refuses("calibration beyond relative tolerance", [&] {
        validate_link_label_mask_frames(labels, reference, {}, bad_calibration);
    });
}
void plane_links() {
    const auto reference = prefix(xy(10, 8), {{"t", "second", 8, 2, 10},
                                              {"c", "", 4, 1, 0},
                                              {"z", "micrometer", 10, .5, 3}});
    const auto labels = prefix(xy(10, 8), {{"t", "millisecond", 4, 4000, 10000},
                                           {"c", "", 1, 1, 0},
                                           {"z", "nanometer", 6, 1000, 3000}});
    auto selected = target(10, 8, {1, 2, 0, 3, 4});
    selected.z = selected.t = 4;
    selected.z_spacing_um = .5;
    selected.time_interval_seconds = 2;
    expected(validate_link_label_mask_frames(labels, reference, {2, 2, 4, 4}, selected),
             1, 2, 3, 4, 0, 0);
    // Singleton label planes have explicit coordinates; sampling steps may differ.
    const auto singleton_labels = prefix(xy(10, 8), {{"t", "second", 1, 2, 18},
                                                     {"z", "micrometer", 1, 5, 5}});
    expected(validate_link_label_mask_frames(singleton_labels, reference, {0, 0, 4, 4}, selected),
             1, 2, 3, 4, 0, 0);
    // A2D mask explicitly attaches to the declared reference/target plane.
    expected(validate_link_label_mask_frames(xy(10, 8), reference, {0, 0, 4, 4}, selected),
             1, 2, 3, 4, 0, 0);
    // Absent reference axes are coherent unitless singleton coordinates at zero.
    const auto label_to_singleton = prefix(xy(10, 8), {{"t", "", 5, 3, -9},
                                                       {"z", "", 4, 2, -4}});
    expected(validate_link_label_mask_frames(label_to_singleton, xy(10, 8), {2, 3, 0, 0},
                                               target(10, 8, {1, 2, 0, 3, 4})),
             1, 2, 3, 4, 0, 0);
    auto bad_plane = labels;
    bad_plane.axes[2].translation = 3250; // Z shifts by half a reference plane.
    refuses("half-plane physical Z mismatch", [&] {
        validate_link_label_mask_frames(bad_plane, reference, {2, 2, 4, 4}, selected);
    });
    bad_plane = labels;
    bad_plane.axes[0].translation = 11000;
    refuses("half-index time mismatch", [&] {
        validate_link_label_mask_frames(bad_plane, reference, {2, 2, 4, 4}, selected);
    });
    refuses("label Z out of range", [&] {
        validate_link_label_mask_frames(labels, reference, {6, 2, 4, 4}, selected);
    });
    refuses("label T out of range", [&] {
        validate_link_label_mask_frames(labels, reference, {2, 4, 4, 4}, selected);
    });
    auto outside = selected;
    outside.z = 10;
    refuses("reference Z out of range", [&] {
        validate_link_label_mask_frames(labels, reference, {2, 2, 10, 4}, outside);
    });
    outside = selected;
    outside.t = 8;
    refuses("reference T out of range", [&] {
        validate_link_label_mask_frames(labels, reference, {2, 2, 4, 8}, outside);
    });
    refuses("target/reference Z disagreement", [&] {
        validate_link_label_mask_frames(labels, reference, {2, 2, 3, 4}, selected);
    });
    refuses("target/reference T disagreement", [&] {
        validate_link_label_mask_frames(labels, reference, {2, 2, 4, 3}, selected);
    });
    refuses("missing label Z with nonzero source index", [&] {
        validate_link_label_mask_frames(xy(10, 8), reference, {1, 0, 4, 4}, selected);
    });
    refuses("missing label T with nonzero source index", [&] {
        validate_link_label_mask_frames(xy(10, 8), reference, {0, 1, 4, 4}, selected);
    });
    auto missing_reference_target = target(10, 8, {0, 0, 0, 1, 1});
    missing_reference_target.z = 1;
    refuses("missing reference Z has only index zero", [&] {
        validate_link_label_mask_frames(xy(10, 8), xy(10, 8), {0, 0, 1, 0},
                                       missing_reference_target);
    });
    missing_reference_target.z = 0;
    missing_reference_target.t = 1;
    refuses("missing reference T has only index zero", [&] {
        validate_link_label_mask_frames(xy(10, 8), xy(10, 8), {0, 0, 0, 1},
                                       missing_reference_target);
    });
    auto physical_singleton = prefix(xy(10, 8), {{"z", "micrometer", 1, 1, 0}});
    refuses("missing reference axis does not infer physical units", [&] {
        validate_link_label_mask_frames(physical_singleton, xy(10, 8), {},
                                       target(10, 8, {0, 0, 0, 1, 1}));
    });
    auto wrong_singleton = label_to_singleton;
    wrong_singleton.axes[1].translation = -3;
    refuses("present label plane must coincide with missing-reference singleton", [&] {
        validate_link_label_mask_frames(wrong_singleton, xy(10, 8), {2, 3, 0, 0},
                                       target(10, 8, {0, 0, 0, 1, 1}));
    });
    const auto tolerant_reference = prefix(xy(2, 2), {{"t", "", 2, 4, 1},
                                                       {"z", "", 2, 2, 1}});
    auto tolerant_labels = tolerant_reference;
    tolerant_labels.axes[0].translation = 1 + 2e-9;
    tolerant_labels.axes[1].translation = 1 + 1e-9;
    auto tolerant_target = target(2, 2, {0, 0, 0, 1, 1});
    tolerant_target.z = tolerant_target.t = 1;
    expected(validate_link_label_mask_frames(tolerant_labels, tolerant_reference,
                                               {1, 1, 1, 1}, tolerant_target),
             0, 0, 1, 1, 0, 0);
    tolerant_labels.axes[1].translation = 1 + 4e-9;
    refuses("Z coordinate exceeds reference-index tolerance", [&] {
        validate_link_label_mask_frames(tolerant_labels, tolerant_reference,
                                       {1, 1, 1, 1}, tolerant_target);
    });
    tolerant_labels = tolerant_reference;
    tolerant_labels.axes[0].translation = 1 + 8e-9;
    refuses("T coordinate exceeds reference-index tolerance", [&] {
        validate_link_label_mask_frames(tolerant_labels, tolerant_reference,
                                       {1, 1, 1, 1}, tolerant_target);
    });
    // All UInt32 indices remain exact in binary64; plane counts do not wrap.
    const auto many = prefix(xy(2, 2), {{"z", "", UINT32_MAX, 1, 0}});
    auto last_plane = target(2, 2, {0, 0, 0, 1, 1});
    last_plane.z = UINT32_MAX - 1;
    expected(validate_link_label_mask_frames(many, many,
                                               {UINT32_MAX - 1, 0, UINT32_MAX - 1, 0}, last_plane),
             0, 0, 1, 1, 0, 0);
}
void unit_vocabulary() {
    const LabelMaskFrame reference{{{"y", "micrometer", 4, 1, 0},
                                     {"x", "micrometer", 4, 1, 0}}};
    auto calibrated = target(4, 4, {0, 0, 0, 2, 2});
    calibrated.pixel_width_um = calibrated.pixel_height_um = 1;
    for (const auto& entry : {std::pair<const char*, double>{"micrometer", 1},
                              {"nanometer", 1000}, {"millimeter", .001},
                              {"centimeter", .0001}, {"meter", .000001}}) {
        const LabelMaskFrame labels{{{"y", entry.first, 4, entry.second, 0},
                                      {"x", entry.first, 4, entry.second, 0}}};
        expected(validate_link_label_mask_frames(labels, reference, {}, calibrated),
                 0, 0, 2, 2, 0, 0);
    }
    const auto timed = prefix(xy(4, 4), {{"t", "second", 3, 60, 0}});
    auto selected = target(4, 4, {0, 0, 0, 2, 2});
    selected.t = 1;
    selected.time_interval_seconds = 60;
    for (const auto& entry : {std::pair<const char*, double>{"second", 60},
                              {"millisecond", 60000}, {"microsecond", 60000000},
                              {"minute", 1}, {"hour", 1. / 60}}) {
        const auto labels = prefix(xy(4, 4), {{"t", entry.first, 3, entry.second, 0}});
        expected(validate_link_label_mask_frames(labels, timed, {0, 1, 0, 1}, selected),
                 0, 0, 2, 2, 0, 0);
    }
}
void reference_plane_calibration() {
    const auto calibrated = prefix(xy(4, 4), {{"t", "millisecond", 3, 2000, 0},
                                               {"z", "nanometer", 3, 500, 0}});
    auto selected = target(4, 4, {0, 0, 0, 2, 2});
    selected.z = selected.t = 1;
    selected.z_spacing_um = .5;
    selected.time_interval_seconds = 2;
    expected(validate_link_label_mask_frames(calibrated, calibrated, {1, 1, 1, 1}, selected),
             0, 0, 2, 2, 0, 0);
    auto mismatch = selected;
    mismatch.z_spacing_um = .25;
    refuses("reference Z step differs from native target calibration", [&] {
        validate_link_label_mask_frames(calibrated, calibrated, {1, 1, 1, 1}, mismatch);
    });
    mismatch = selected;
    mismatch.time_interval_seconds = 4;
    refuses("reference time step differs from native target calibration", [&] {
        validate_link_label_mask_frames(calibrated, calibrated, {1, 1, 1, 1}, mismatch);
    });
    mismatch = selected;
    mismatch.z_spacing_um.reset();
    refuses("reference physical Z cannot invent target Z calibration", [&] {
        validate_link_label_mask_frames(calibrated, calibrated, {1, 1, 1, 1}, mismatch);
    });
    mismatch = selected;
    mismatch.time_interval_seconds.reset();
    refuses("reference physical T cannot invent target time calibration", [&] {
        validate_link_label_mask_frames(calibrated, calibrated, {1, 1, 1, 1}, mismatch);
    });
    auto unitless = calibrated;
    unitless.axes[0].unit.clear();
    unitless.axes[1].unit.clear();
    refuses("unitless reference Z/T cannot infer physical target units", [&] {
        validate_link_label_mask_frames(unitless, unitless, {1, 1, 1, 1}, selected);
    });
    auto unknown = calibrated;
    unknown.axes[1].unit = "um";
    refuses("unknown reference Z unit", [&] {
        validate_link_label_mask_frames(calibrated, unknown, {1, 1, 1, 1}, selected);
    });
    unknown = calibrated;
    unknown.axes[0].unit = "seconds";
    refuses("unknown reference time unit", [&] {
        validate_link_label_mask_frames(calibrated, unknown, {1, 1, 1, 1}, selected);
    });
    auto near = selected;
    near.z_spacing_um = .5 * (1 + 5e-13);
    near.time_interval_seconds = 2 * (1 + 5e-13);
    expected(validate_link_label_mask_frames(calibrated, calibrated, {1, 1, 1, 1}, near),
             0, 0, 2, 2, 0, 0);
    near.time_interval_seconds = 2 * (1 + 2e-12);
    refuses("native time calibration beyond declared tolerance", [&] {
        validate_link_label_mask_frames(calibrated, calibrated, {1, 1, 1, 1}, near);
    });
    // A native singleton image may have calibration even without a reference Z/T
    // axis. The explicit target/reference indices remain zero in that case.
    auto singleton = target(4, 4, {0, 0, 0, 2, 2});
    singleton.z_spacing_um = 7;
    singleton.time_interval_seconds = 3;
    expected(validate_link_label_mask_frames(xy(4, 4), xy(4, 4), {}, singleton),
             0, 0, 2, 2, 0, 0);
    singleton.z_spacing_um = NAN;
    refuses("invalid native singleton Z calibration", [&] {
        validate_link_label_mask_frames(xy(4, 4), xy(4, 4), {}, singleton);
    });
    singleton.z_spacing_um = 7;
    singleton.time_interval_seconds = 0;
    refuses("invalid native singleton time calibration", [&] {
        validate_link_label_mask_frames(xy(4, 4), xy(4, 4), {}, singleton);
    });
}
void invalid_descriptors() {
    const auto good = xy(10, 8);
    const auto declared = target(10, 8, {0, 0, 0, 2, 2});
    const auto bad_label = [&](const char* name, auto modify) {
        auto bad = good;
        modify(bad);
        refuses(name, [&] { validate_link_label_mask_frames(bad, good, {}, declared); });
    };
    bad_label("too few axes", [](auto& f) { f.axes.pop_back(); });
    bad_label("too many axes", [](auto& f) { f.axes.resize(6); });
    bad_label("reversed XY", [](auto& f) { std::swap(f.axes[0], f.axes[1]); });
    bad_label("duplicate axis", [](auto& f) { f.axes[0].name = "x"; });
    bad_label("unknown axis", [](auto& f) { f.axes[0].name = "q"; });
    bad_label("missing Y", [](auto& f) { f.axes[0].name = "z"; });
    bad_label("zero extent", [](auto& f) { f.axes[0].extent = 0; });
    bad_label("UInt64 spatial extent", [](auto& f) { f.axes[1].extent = UINT64_MAX; });
    bad_label("zero scale", [](auto& f) { f.axes[0].scale = 0; });
    bad_label("negative scale", [](auto& f) { f.axes[0].scale = -1; });
    bad_label("NaN scale", [](auto& f) { f.axes[0].scale = NAN; });
    bad_label("infinite scale", [](auto& f) { f.axes[0].scale = INFINITY; });
    bad_label("NaN translation", [](auto& f) { f.axes[0].translation = NAN; });
    bad_label("infinite translation", [](auto& f) { f.axes[0].translation = INFINITY; });
    bad_label("undeclared unit alias", [](auto& f) { f.axes[0].unit = "um"; });
    bad_label("time unit on spatial axis", [](auto& f) { f.axes[0].unit = "second"; });
    bad_label("physical/unitless mismatch", [](auto& f) { f.axes[0].unit = "micrometer"; });
    bad_label("half-pixel origin", [](auto& f) { f.axes[0].translation = .5; });
    bad_label("source crop begins before labels", [](auto& f) { f.axes[1].translation = 1; });
    bad_label("source crop extends beyond labels", [](auto& f) { f.axes[1].extent = 1; });
    auto bad = prefix(good, {{"c", "", 2, 1, 0}});
    refuses("label C is not singleton", [&] {
        validate_link_label_mask_frames(bad, good, {}, declared);
    });
    bad = prefix(good, {{"c", "", 4097, 1, 0}});
    refuses("reference C exceeds channel domain", [&] {
        validate_link_label_mask_frames(good, bad, {}, declared);
    });
    expected(validate_link_label_mask_frames(prefix(good, {{"c", "", 1, 1, 0}}),
                                               prefix(good, {{"c", "", 4096, 1, 0}}), {}, declared),
             0, 0, 2, 2, 0, 0);
    for (int mode = 0; mode < 3; ++mode) {
        bad = prefix(good, {{"c", "", 1, 1, 0}});
        if (mode == 0)
            bad.axes[0].scale = 1 + 1e-14;
        if (mode == 1)
            bad.axes[0].translation = 1e-15;
        if (mode == 2)
            bad.axes[0].unit = "micrometer";
        refuses("C transform must be exactly unitless identity", [&] {
            validate_link_label_mask_frames(bad, good, {}, declared);
        });
        refuses("reference C transform must also be identity", [&] {
            validate_link_label_mask_frames(good, bad, {}, declared);
        });
    }
    bad = prefix(good, {{"c", "", 1, 1, 0}, {"t", "", 1, 1, 0}});
    refuses("C before T order", [&] { validate_link_label_mask_frames(bad, good, {}, declared); });
    bad = prefix(good, {{"z", "", std::uint64_t(UINT32_MAX) + 1, 1, 0}});
    refuses("Z count exceeds UInt32", [&] {
        validate_link_label_mask_frames(bad, good, {}, declared);
    });
    bad = prefix(good, {{"t", "nanosecond", 1, 1, 0}});
    refuses("unadmitted time unit", [&] { validate_link_label_mask_frames(bad, good, {}, declared); });
    bad = prefix(good, {{"t", "meter", 1, 1, 0}});
    refuses("space unit on time axis", [&] { validate_link_label_mask_frames(bad, good, {}, declared); });
    auto wrong_dimensions = good;
    wrong_dimensions.axes[1].extent = 11;
    refuses("reference full-image width mismatch", [&] {
        validate_link_label_mask_frames(good, wrong_dimensions, {}, declared);
    });
    wrong_dimensions = good;
    wrong_dimensions.axes[0].extent = 9;
    refuses("reference full-image height mismatch", [&] {
        validate_link_label_mask_frames(good, wrong_dimensions, {}, declared);
    });
}
void invalid_targets() {
    const auto good = xy(10, 8);
    const auto declared = target(10, 8, {1, 1, 0, 2, 2});
    const auto invalid = [&](const char* name, auto modify) {
        auto bad = declared;
        modify(bad);
        refuses(name, [&] { validate_link_label_mask_frames(good, good, {}, bad); });
    };
    invalid("zero target width", [](auto& t) { t.width = 0; });
    invalid("negative target height", [](auto& t) { t.height = -1; });
    invalid("negative crop X", [](auto& t) { t.crop.x = -1; });
    invalid("negative crop Y", [](auto& t) { t.crop.y = -1; });
    invalid("empty crop width", [](auto& t) { t.crop.width = 0; });
    invalid("negative crop height", [](auto& t) { t.crop.height = -1; });
    invalid("outside crop X", [](auto& t) { t.crop.x = 10; });
    invalid("outside crop Y", [](auto& t) { t.crop.y = 8; });
    invalid("crop right beyond image", [](auto& t) { t.crop.width = 10; });
    invalid("crop bottom beyond image", [](auto& t) { t.crop.height = 8; });
    invalid("nonzero pyramid level", [](auto& t) { t.crop.level = 1; });
    invalid("negative pyramid level", [](auto& t) { t.crop.level = -1; });
    invalid("unitless reference cannot infer target calibration", [](auto& t) {
        t.pixel_width_um = 1;
    });
    for (const auto value : {0., -1., double(NAN), double(INFINITY)})
        invalid("invalid target calibration", [&](auto& t) { t.pixel_height_um = value; });
    auto physical = good;
    physical.axes[0].unit = "micrometer";
    refuses("physical reference requires declared target calibration", [&] {
        validate_link_label_mask_frames(physical, physical, {}, declared);
    });
    auto calibrated = declared;
    calibrated.pixel_height_um = 1;
    refuses("physical reference cannot infer label units", [&] {
        validate_link_label_mask_frames(good, physical, {}, calibrated);
    });
    const auto large = xy(1000001, 1000001);
    refuses("crop axis exceeds native LabelRaster bound", [&] {
        validate_link_label_mask_frames(large, large, {},
                                       target(1000001, 1000001, {0, 0, 0, 1000001, 1}));
    });
    refuses("crop sample product exceeds native UInt32 label bound", [&] {
        validate_link_label_mask_frames(large, large, {},
                                       target(1000001, 1000001, {0, 0, 0, 65536, 65536}));
    });
}
void integer_domains_and_extremes() {
    constexpr std::int64_t limit = 4503599627370496LL;
    auto reference = xy(std::uint64_t(limit + 20), 1);
    auto labels = xy(100, 1);
    labels.axes[1].translation = double(limit);
    expected(validate_link_label_mask_frames(labels, reference, {},
                                               target(limit + 20, 1, {limit + 7, 0, 0, 1, 1})),
             7, 0, 1, 1, limit, 0);
    labels.axes[1].translation = double(limit + 1);
    refuses("origin exceeds exact translation domain", [&] {
        validate_link_label_mask_frames(labels, reference, {},
                                       target(limit + 20, 1, {limit + 7, 0, 0, 1, 1}));
    });
    labels = xy(std::uint64_t(limit + 30), 1);
    labels.axes[1].translation = -double(limit);
    expected(validate_link_label_mask_frames(labels, xy(20, 1), {},
                                               target(20, 1, {7, 0, 0, 1, 1})),
             limit + 7, 0, 1, 1, -limit, 0);
    // Adjacent extents above2^53 must never be compared after double conversion.
    constexpr std::int64_t large_width = 9007199254740993LL;
    const auto large = xy(std::uint64_t(large_width), 1);
    const auto exact_target = target(large_width, 1, {large_width - 1, 0, 0, 1, 1});
    expected(validate_link_label_mask_frames(large, large, {}, exact_target),
             large_width - 1, 0, 1, 1, 0, 0);
    auto neighbor = large;
    --neighbor.axes[1].extent;
    refuses("adjacent huge reference dimension differs exactly", [&] {
        validate_link_label_mask_frames(large, neighbor, {}, exact_target);
    });
    const auto maximum = xy(INT64_MAX, 1);
    auto edge = target(INT64_MAX, 1, {INT64_MAX - 2, 0, 0, 1, 1});
    expected(validate_link_label_mask_frames(maximum, maximum, {}, edge),
             INT64_MAX - 2, 0, 1, 1, 0, 0);
    auto negative = maximum;
    negative.axes[1].translation = -4;
    refuses("source crop subtraction overflows Int64", [&] {
        validate_link_label_mask_frames(negative, maximum, {}, edge);
    });
    edge.crop.x = INT64_MAX - 1;
    edge.crop.width = 2;
    refuses("target crop addition would overflow", [&] {
        validate_link_label_mask_frames(maximum, maximum, {}, edge);
    });
    const auto good = xy(4, 4);
    const auto declared = target(4, 4, {0, 0, 0, 1, 1});
    auto bad = good;
    bad.axes[1].unit = "meter";
    bad.axes[1].scale = std::numeric_limits<double>::max();
    refuses("unit conversion scale overflow", [&] {
        validate_link_label_mask_frames(bad, good, {}, declared);
    });
    bad = good;
    bad.axes[1].unit = "nanometer";
    bad.axes[1].scale = std::numeric_limits<double>::denorm_min();
    refuses("unit conversion scale underflow", [&] {
        validate_link_label_mask_frames(bad, good, {}, declared);
    });
    bad.axes[1].scale = 1000;
    bad.axes[1].translation = std::numeric_limits<double>::denorm_min();
    refuses("unit conversion translation underflow", [&] {
        validate_link_label_mask_frames(bad, good, {}, declared);
    });
    auto other = good;
    bad = good;
    bad.axes[1].translation = std::numeric_limits<double>::max();
    other.axes[1].translation = -std::numeric_limits<double>::max();
    refuses("relative translation subtraction overflow", [&] {
        validate_link_label_mask_frames(bad, other, {}, declared);
    });
    bad = other = good;
    bad.axes[1].scale = other.axes[1].scale = std::numeric_limits<double>::min();
    bad.axes[1].translation = std::numeric_limits<double>::max();
    refuses("relative translation division overflow", [&] {
        validate_link_label_mask_frames(bad, other, {}, declared);
    });
    bad = other = good;
    bad.axes[1].scale = other.axes[1].scale = std::numeric_limits<double>::max();
    bad.axes[1].translation = std::numeric_limits<double>::denorm_min();
    refuses("relative translation division underflow", [&] {
        validate_link_label_mask_frames(bad, other, {}, declared);
    });
    bad = prefix(good, {{"z", "", 3, std::numeric_limits<double>::max(), 0}});
    other = prefix(good, {{"z", "", 1, 1, 0}});
    refuses("selected plane coordinate overflow", [&] {
        validate_link_label_mask_frames(bad, other, {2, 0, 0, 0}, declared);
    });
}
} // namespace
int main() {
    try {
        spatial_links();
        declared_tolerances();
        plane_links();
        unit_vocabulary();
        reference_plane_calibration();
        invalid_descriptors();
        invalid_targets();
        integer_domains_and_extremes();
        std::cout << "Native label/reference/target frame-link tests passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
