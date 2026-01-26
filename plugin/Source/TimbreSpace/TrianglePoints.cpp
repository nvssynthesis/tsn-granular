#include "TrianglePoints.h"
#include "TimbreSpaceTriangulation.h"

namespace nvs::timbrespace {


std::optional<TrianglePoints> TrianglePoints::create(const delaunator::Delaunator& d, const size_t triangleIdx)
{
    const size_t t0 = triangleIdx * 3;
    const size_t t1 = t0 + 1;
    const size_t t2 = t0 + 2;

    if (t0 >= d.triangles.size()) {
        return std::nullopt;
    }

    const size_t v0 = d.triangles[t0];
    const size_t v1 = d.triangles[t1];
    const size_t v2 = d.triangles[t2];

    TrianglePoints points {
        .p0 = getPointFromVertex(d, v0),
        .p1 = getPointFromVertex(d, v1),
        .p2 = getPointFromVertex(d, v2),

        .halfedge0 = t0,
        .halfedge1 = t1,
        .halfedge2 = t2
    };
    return points;
}

}
