#pragma once

#include "TimbrePointTypes.h"
#include "../delaunator-cpp/include/delaunator.hpp"

namespace nvs::timbrespace {

struct TrianglePoints {
    Timbre2DPoint p0, p1, p2;
    size_t halfedge0, halfedge1, halfedge2;
    static std::optional<TrianglePoints> create(const delaunator::Delaunator& d, size_t triangleIdx);
};


} // namespace nvs