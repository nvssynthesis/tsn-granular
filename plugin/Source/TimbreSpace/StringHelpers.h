#pragma once
#include "TimbrePointTypes.h"
#include "TimbreSpaceTriangulation.h"
#include "TrianglePoints.h"

#ifndef WALK_STRING_DEBUGGING
#define WALK_STRING_DEBUGGING 0
#endif

namespace nvs::timbrespace {

// some of these may seem a bit pointless, but they help with the use of an external script that can parse and then
// visualize triangles, vertices, points, and lines.

std::ostream &operator<<(std::ostream &out, const Timbre2DPoint &p);
std::string str(const Timbre2DPoint &p);
std::string str(const TrianglePoints &tri);
void printTriangles(const delaunator::Delaunator& d);
void printTriangleIdx(size_t idx);
void printLine(const Timbre2DPoint &p, const Timbre2DPoint &q);

} // namespace nvs::timbrespace