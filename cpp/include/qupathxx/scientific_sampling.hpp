#pragma once
#include "scientific_grid.hpp"
#include "image_server.hpp"

namespace qpx {
// Opaque RGB8 area means in float64; invalid/missing support is canonical zero.
// Context is included in the arrays. Validity is source support, not ROI or
// core-grid eligibility. This does not enable a new scientific detector recipe.
class ScientificRgbGrid {
    ScientificGrid grid_;
    std::vector<double> rgb_;
    std::vector<std::uint8_t> valid_;
    std::string decoded_source_sha256_;
    explicit ScientificRgbGrid(ScientificGrid grid):grid_(grid),
        rgb_(static_cast<std::size_t>(grid.width()*grid.height())*3,0.),
        valid_(static_cast<std::size_t>(grid.width()*grid.height()),0) {}
    friend ScientificRgbGrid read_scientific_rgb(const ImageServer&,const ScientificGrid&,const std::atomic<bool>*);
public:
    const ScientificGrid& grid() const {return grid_;}
    const std::vector<double>& values() const {return rgb_;}
    const std::vector<std::uint8_t>& valid() const {return valid_;}
    const std::string& decoded_source_sha256() const {return decoded_source_sha256_;}
    std::int64_t width() const {return grid_.width();}
    std::int64_t height() const {return grid_.height();}
};
// Explicit natural level zero, one bounded clipped reader window. The captured
// image dimensions must match the server and level zero must have unit scale.
ScientificRgbGrid read_scientific_rgb(const ImageServer&,const ScientificGrid&,
    const std::atomic<bool>* cancelled=nullptr);
}
