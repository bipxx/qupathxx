#include "qupathxx/scientific_grid.hpp"
#include "scientific_grid_reference.hpp"
#include <array>
#include <cstring>
#include <iostream>
#include <stdexcept>

namespace {
const char* stage="initial";
void require(bool value) {if(!value) throw std::runtime_error(std::string("scientific grid geometry assertion failed: ")+stage);}
template<class Exception,class Function> void rejects(Function function) {
    try {function();} catch(const Exception&) {return;}
    throw std::runtime_error(std::string("scientific grid admission did not reject: ")+stage);
}
std::uint64_t bits(double value) {std::uint64_t result;std::memcpy(&result,&value,sizeof(value));return result;}
void fraction(qpx::GridFraction value,std::int64_t n,std::int64_t d) {
    require(value.numerator()==n && value.denominator()==d);
}
void axis(const grid_reference::Axis& value) {
    const qpx::ScientificGridAxis grid(value.origin,value.extent,value.samples,value.context);
    fraction(grid.edge(value.index),value.ln,value.ld);
    fraction(grid.edge(value.index+1),value.rn,value.rd);
    fraction(grid.center(value.index),value.cn,value.cd);
    require(bits(grid.center(value.index).binary64())==value.bits);
}
std::array<std::int64_t,4> bounds(qpx::GridReadBounds value) {return {value.x,value.y,value.width,value.height};}
}

void check_scientific_grid_geometry() {
    using namespace qpx;
    stage="axis";
    for(const auto& value:grid_reference::axes) axis(value);
    for(const auto& value:grid_reference::probes) axis(value);
    for(const auto& value:grid_reference::fractions) {
        stage="fraction";
        const GridFraction f(value.n,value.d);
        require(bits(f.binary64())==value.bits);
        require(f.floor()<=f.ceil() && f.ceil()-f.floor()<=1);
    }
    for(const auto& value:grid_reference::weights) {
        stage="weight";
        const ScientificGridAxis grid(value.origin,value.extent,value.samples,value.context);
        fraction(grid.overlap(value.index,value.pixel),value.n,value.d);
        fraction(grid.overlap(value.index,std::numeric_limits<std::int64_t>::min()),0,1);
        fraction(grid.overlap(value.index,std::numeric_limits<std::int64_t>::max()),0,1);
    }
    for(const auto& value:grid_reference::plans) {
        stage="plan";
        const ScientificGrid grid(value.iw,value.ih,{value.x,value.w,value.nx,value.cx},{value.y,value.h,value.ny,value.cy});
        require(bounds(grid.read_bounds())==value.read);
        require(bounds(grid.clipped_read_bounds())==value.clipped);
        require(grid.overlap_visits()==value.visits);
        require(grid.sampling_buffer_bytes()==value.bytes);
        // Exact budget boundary: admission must account for both source and output.
        ScientificGrid exact(value.iw,value.ih,grid.x(),grid.y(),value.bytes);
        rejects<std::length_error>([&]{ScientificGrid short_budget(value.iw,value.ih,grid.x(),grid.y(),value.bytes-1);});
    }
    for(const auto n:{std::numeric_limits<std::int64_t>::min(),std::numeric_limits<std::int64_t>::max()}) {
        stage="invalid extrema";
        rejects<std::invalid_argument>([&]{GridFraction f(n,1);});
        rejects<std::invalid_argument>([&]{ScientificGridAxis grid(n,1,1);});
    }
    stage="invalid controls";
    rejects<std::invalid_argument>([]{GridFraction f(1,0);});
    rejects<std::invalid_argument>([]{GridFraction f(1,2000001);});
    rejects<std::invalid_argument>([]{ScientificGridAxis grid(0,0,1);});
    rejects<std::invalid_argument>([]{ScientificGridAxis grid(0,1,0);});
    rejects<std::invalid_argument>([]{ScientificGridAxis grid(0,1,1000001);});
    rejects<std::invalid_argument>([]{ScientificGridAxis grid(0,1,1,-1);});
    rejects<std::invalid_argument>([]{ScientificGridAxis grid(0,1,1,4097);});
    rejects<std::invalid_argument>([]{ScientificGridAxis grid(1000000000000LL,1,1);});
    const ScientificGridAxis one(0,1,1);
    rejects<std::out_of_range>([&]{one.edge(-1);});
    rejects<std::out_of_range>([&]{one.center(1);});
    rejects<std::out_of_range>([&]{one.overlap(std::numeric_limits<std::int64_t>::max(),0);});
    rejects<std::invalid_argument>([&]{ScientificGrid grid(0,1,one,one);});
    rejects<std::invalid_argument>([&]{ScientificGrid grid(1,1,one,one,0);});
    rejects<std::invalid_argument>([&]{ScientificGrid grid(1,1,one,one,(1ULL<<30)+1);});
    rejects<std::length_error>([]{ScientificGrid grid(1000000000000LL,1,{0,1000000000000LL,1},{0,1,1});});
    rejects<std::length_error>([]{ScientificGrid grid(10000,10000,{0,10000,1},{0,10000,1},1ULL<<30);});
    rejects<std::length_error>([]{ScientificGrid grid(1,1,{0,1,11000},{0,1,1000});});
    std::cout<<"Native scientific grid exact geometry, rounding, context and admission passed\n";
}
