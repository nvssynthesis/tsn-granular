/*
  ==============================================================================

    TimbreSpaceTriangulation.cpp
    Created: 2 Jul 2025 5:52:14pm
    Author:  Nicholas Solem

  ==============================================================================
*/

#include "TimbreSpaceTriangulation.h"
#include <set>
#include <fmt/core.h>
#include <cassert>
#include "StringHelpers.h"
#include "../../slicer_granular/JUCE/modules/juce_core/system/juce_PlatformDefs.h"

#ifndef DBG
#include <iostream>
#include "StringHelpers.h"
#define DBG(x) (std::cerr << x << std::endl, (void)0)
#endif

#ifndef jassert
#define jassert(x) assert(x)
#endif



namespace nvs::timbrespace {

std::vector<double> make2dCoordinates(const std::vector<Timbre5DPoint> &points){
	std::vector<double> coords;
	coords.reserve(points.size() * 2);
    const auto p0x = points[0][0];
    const auto p0y = points[0][1];
    bool allXSame = true;
    bool allYSame = true;
	for (auto const & p : points){
        const auto x = p[0];
	    const auto y = p[1];
	    if (x != p0x) {
	        allXSame = false;
	    }
	    if (y != p0y) {
	        allYSame = false;
	    }
	    coords.push_back(x);
		coords.push_back(y);
	}
	assert(coords.size() == points.size() * 2);
    if(allXSame || allYSame) {
        DBG("At least 1 dimension has no variance; these coordinates will cause an invalid triangulation\n");
    }
	return coords;
}

using Point2D = Timbre2DPoint;

// Test if point p is inside triangle formed by vertices a, b, c
bool pointInTriangle(const Point2D& p, const Point2D& a, const Point2D& b, const Point2D& c) {
    // Compute vectors
    const Point2D v0 = c - a;
    const Point2D v1 = b - a;
    const Point2D v2 = p - a;

    const double dot00 = v0.dot(v0);
    const double dot01 = v0.dot(v1);
    const double dot02 = v0.dot(v2);
    const double dot11 = v1.dot(v1);
    const double dot12 = v1.dot(v2);

    // Compute barycentric coordinates
    const double invDenom = 1.0 / (dot00 * dot11 - dot01 * dot01);
    const double u = (dot11 * dot02 - dot01 * dot12) * invDenom;
    const double v = (dot00 * dot12 - dot01 * dot02) * invDenom;

    // Check if point is in triangle
    return (u >= 0) && (v >= 0) && (u + v <= 1);
}

std::optional<std::array<size_t, 3>> findContainingTriangle(const delaunator::Delaunator& d,
                                              const Point2D& target, const size_t startTriangle)
{
#if 1
    const auto containingTriangle = straightWalk(d, target, startTriangle);

    size_t idx0{0}, idx1{1}, idx2{2};
    if (containingTriangle.has_value()) {
        const size_t i = containingTriangle.value() / 3;
        idx0 = d.triangles[i];
        idx1 = d.triangles[i + 1];
        idx2 = d.triangles[i + 2];
    }
#else
    // Iterate through all triangles
    for (size_t i = 0; i < d.triangles.size(); i += 3) {
        // Get triangle vertex indices
        size_t idx0 = d.triangles[i];
        size_t idx1 = d.triangles[i + 1];
        size_t idx2 = d.triangles[i + 2];
#endif

        // Get triangle vertex coordinates
        Point2D a(d.coords[2 * idx0], d.coords[2 * idx0 + 1]);
        Point2D b(d.coords[2 * idx1], d.coords[2 * idx1 + 1]);
        Point2D c(d.coords[2 * idx2], d.coords[2 * idx2 + 1]);

        // Test if target is inside this triangle
        if (pointInTriangle(target, a, b, c)) {
            return std::array<size_t, 3>{idx0, idx1, idx2};
        }
    return std::nullopt;
}

std::array<double, 3> computeDistanceWeights(const Point2D& p,
                                 const Point2D& a,
                                 const Point2D& b,
                                 const Point2D& c)
{
    double d0 = (p - a).norm() + 1e-10;
    double d1 = (p - b).norm() + 1e-10;
    double d2 = (p - c).norm() + 1e-10;

    double inv_d0 = 1.0 / d0;
    double inv_d1 = 1.0 / d1;
    double inv_d2 = 1.0 / d2;
    double sum = inv_d0 + inv_d1 + inv_d2;

    return {inv_d0 / sum, inv_d1 / sum, inv_d2 / sum};
}
// project point onto line segment ab
Point2D projectPointOntoSegment(const Point2D& p,
                                      const Point2D& a,
                                      const Point2D& b) {
    assert(std::isfinite(a[0]) && std::isfinite(a[1]));
    assert(std::isfinite(b[0]) && std::isfinite(b[1]));
    assert(std::isfinite(p[0]) && std::isfinite(p[1]));

    Point2D ab = b - a;
    Point2D ap = p - a;

    double ab_squared = ab.dot(ab);
    if (ab_squared < 1e-10) return a; // degenerate segment

    double t = ap.dot(ab) / ab_squared;
    t = std::clamp(t, 0.0, 1.0); // stay on segment by clamp

    return {a[0] + t * ab[0], a[1] + t * ab[1]};
}

// find which edge is closest to p and project onto it
Point2D clampToTriangle(const Point2D& p,
                               const Point2D& a,
                               const Point2D& b,
                               const Point2D& c) {
    // project onto each edge
    Point2D proj_ab = projectPointOntoSegment(p, a, b);
    Point2D proj_bc = projectPointOntoSegment(p, b, c);
    Point2D proj_ca = projectPointOntoSegment(p, c, a);

    // closest projection
    double dist_ab = (p - proj_ab).norm();
    double dist_bc = (p - proj_bc).norm();
    double dist_ca = (p - proj_ca).norm();

    if (dist_ab <= dist_bc && dist_ab <= dist_ca) return proj_ab;
    if (dist_bc <= dist_ca) return proj_bc;
    return proj_ca;
}

// compute barycentric coordinates for interpolation weights
std::array<double, 3> computeBarycentricWeights(const Point2D& p,
                                     const Point2D& a,
                                     const Point2D& b,
                                     const Point2D& c) {

    const Point2D v0 = c - a;
    const Point2D v1 = b - a;
    const Point2D v2 = p - a;

    const double dot00 = v0.dot(v0);
    const double dot01 = v0.dot(v1);
    const double dot02 = v0.dot(v2);
    const double dot11 = v1.dot(v1);
    const double dot12 = v1.dot(v2);

    const double denom = dot00 * dot11 - dot01 * dot01;

    if (std::abs(denom) < 1e-10) {  // degenerate triangle (collinear points)
        // fallback to distance weighting
        return computeDistanceWeights(p, a, b, c);
    }

    const double invDenom = 1.0 / denom;
    const double u = (dot11 * dot02 - dot01 * dot12) * invDenom;
    const double v = (dot00 * dot12 - dot01 * dot02) * invDenom;
    const double w = 1.0 - u - v;

    if (w < 0 || v < 0 || u < 0) {
        const Point2D clamped = clampToTriangle(p, a, b, c);

        // recompute with clamped point
        const Point2D v0_c = c - a;
        const Point2D v1_c = b - a;
        const Point2D v2_c = clamped - a;

        const double dot00_c = v0_c.dot(v0_c);
        const double dot01_c = v0_c.dot(v1_c);
        const double dot02_c = v0_c.dot(v2_c);
        const double dot11_c = v1_c.dot(v1_c);
        const double dot12_c = v1_c.dot(v2_c);

        const double denom_c = dot00_c * dot11_c - dot01_c * dot01_c;

        if (std::abs(denom_c) < 1e-10) {
            return computeDistanceWeights(clamped, a, b, c);
        }

        const double invDenom_c = 1.0 / denom_c;
        const double u_c = (dot11_c * dot02_c - dot01_c * dot12_c) * invDenom_c;
        const double v_c = (dot00_c * dot12_c - dot01_c * dot02_c) * invDenom_c;
        const double w_c = 1.0 - u_c - v_c;

        // clamped point should be on/in triangle, but clamp weights for safety
        return {std::max(0.0, w_c), std::max(0.0, v_c), std::max(0.0, u_c)};
    }

    return {w, v, u}; // weights for vertices a, b, c respectively
}

// Usage example:
/*
 Delaunator d(coordinates);
 Point2D target(x, y);
 
 auto triangleOpt = findContainingTriangle(d, target);
 if (triangleOpt) {
 auto [idx0, idx1, idx2] = *triangleOpt;
 
 // Get the actual triangle points
 Point2D a(d.coords[2 * idx0], d.coords[2 * idx0 + 1]);
 Point2D b(d.coords[2 * idx1], d.coords[2 * idx1 + 1]);
 Point2D c(d.coords[2 * idx2], d.coords[2 * idx2 + 1]);
 
 // Compute interpolation weights
 auto weights = computeBarycentricWeights(target, a, b, c);
 
 // Use idx0, idx1, idx2 as your timbre point indices
 // Use weights[0], weights[1], weights[2] as amplitudes
 }
*/

// triangulation-based point selection function
std::vector<WeightedIdx> findPointsTriangulationBased(const Timbre5DPoint& target,
    const std::vector<Timbre5DPoint>& database, const delaunator::Delaunator &d, size_t* startTriangle)
{
	if (database.empty()) { return {{},{},{}}; }
    const auto dbSizeAtStart = database.size();

	// Need at least 3 points for triangulation
	jassert (database.size() >= 3);

	// Find triangle containing the target point
	const Point2D targetPoint = get2D(target);

    const size_t _startTri = startTriangle != nullptr ? *startTriangle : 0;
    const auto triangleOpt = findContainingTriangle(d, targetPoint, _startTri);
    if (startTriangle != nullptr && triangleOpt != std::nullopt) {
        *startTriangle = triangleOpt.value()[0];
    }

	if (!triangleOpt.has_value()) {
		// Target point is outside the convex hull
		// Fall back to distance-based method or handle as edge case
		const auto result = findNearestTrianglePoints(target, database, d);
	    jassert (result.size() == 3);
	    return result;
	}

	// Get the triangle vertices
	const auto triangle = triangleOpt.value();
	const size_t idx0 = triangle[0];
	const size_t idx1 = triangle[1];
	const size_t idx2 = triangle[2];
	{
	    const auto dbSize = database.size();
	    jassert (dbSize == dbSizeAtStart);
	    jassert (idx0 < dbSize);
	    jassert (idx1 < dbSize);
	    jassert (idx2 < dbSize);
	}

	// Get the 2D points for barycentric calculation
	const Point2D p0 = get2D(database[idx0]);
	const Point2D p1 = get2D(database[idx1]);
	const Point2D p2 = get2D(database[idx2]);

	// Compute barycentric weights
	const auto weights = computeBarycentricWeights(targetPoint, p0, p1, p2);
    for (auto & w : weights) {
        jassert (w >= 0.0);
    }

	// Create result vector
	std::vector<WeightedIdx> result;
	result.reserve(3);
	result.emplace_back(idx0, weights[0]);
	result.emplace_back(idx1, weights[1]);
	result.emplace_back(idx2, weights[2]);

    jassert (result.size() == 3);
	return result;
}

// Helper function for when target is outside convex hull
std::vector<WeightedIdx> findNearestTrianglePoints(const Timbre5DPoint& target,
														 const std::vector<Timbre5DPoint>& database,
														 const delaunator::Delaunator& d)
{
	// Find the nearest edge or vertex of the convex hull
	// This is a simplified approach - you might want to be more sophisticated

	Point2D targetPoint = get2D(target);
	double minDistance = std::numeric_limits<double>::max();
	std::array<size_t, 3> bestTriangle {0, 1, 2};

	// Check all triangles and find the one with minimum distance to target
    std::array<Point2D, 3> bestPoints;
	for (size_t i = 0; i < d.triangles.size(); i += 3) {
		const size_t idx0 = d.triangles[i];
		const size_t idx1 = d.triangles[i + 1];
		const size_t idx2 = d.triangles[i + 2];

		Point2D p0 = get2D(database[idx0]);
		Point2D p1 = get2D(database[idx1]);
		Point2D p2 = get2D(database[idx2]);

		// Calculate centroid of triangle
		Point2D centroid = (p0 + p1 + p2) / 3.0;

        if (const double distance = (targetPoint - centroid).squaredNorm();
            distance < minDistance)
        {
			minDistance = distance;
			bestTriangle = {idx0, idx1, idx2};
            bestPoints = {p0, p1, p2};
		}
	}

    const auto weights = computeBarycentricWeights(targetPoint, bestPoints[0], bestPoints[1], bestPoints[2]);
	// Use the closest triangle and project the point onto it
	std::vector<WeightedIdx> result;
	result.reserve(3);
	result.emplace_back(bestTriangle[0], weights[0]);
	result.emplace_back(bestTriangle[1], weights[1]);
	result.emplace_back(bestTriangle[2], weights[2]);

	return result;
}


//=============================================================================================================================


// Find the halfedge index in triangle that goes from vertex v1 to vertex v2
// Returns SIZE_MAX if not found
size_t findHalfedge(const delaunator::Delaunator& d, size_t triangleIdx, size_t v1, size_t v2) {
    size_t t0 = triangleIdx * 3;

    for (size_t i = 0; i < 3; ++i) {
        size_t halfedge = t0 + i;
        size_t nextHalfedge = (i == 2) ? t0 : halfedge + 1;

        if (d.triangles[halfedge] == v1 && d.triangles[nextHalfedge] == v2) {
            return halfedge;
        }
    }
    return SIZE_MAX;
}

// Check if two triangles are neighbors (share an edge)
bool isNeighbor(const delaunator::Delaunator& d, size_t triangle1, size_t triangle2) {
    size_t baseIdx = triangle1 * 3;

    // Check all three edges of triangle1
    for (size_t i = 0; i < 3; ++i) {
        size_t halfedge = baseIdx + i;
        size_t oppositeHalfedge = d.halfedges[halfedge];

        if (oppositeHalfedge != delaunator::INVALID_INDEX) {
            size_t neighborTri = oppositeHalfedge / 3;
            if (neighborTri == triangle2) {
                return true;
            }
        }
    }
    return false;
}

// Get the neighbor triangle across the edge from v1 to v2
// Returns SIZE_MAX if no neighbor exists (hull edge) or edge not found
size_t neighbor(const delaunator::Delaunator& d,
                size_t triangle, size_t v1, size_t v2)
{
    size_t baseIdx = triangle * 3;

    for (size_t i = 0; i < 3; ++i) {
        size_t halfedge = baseIdx + i;
        size_t currVertex = d.triangles[halfedge];
        size_t nextVertex = d.triangles[baseIdx + (i + 1) % 3];

        // check if this edge connects v1 and v2 in either direction
        if ((currVertex == v1 && nextVertex == v2) ||
            (currVertex == v2 && nextVertex == v1)) {

            size_t oppositeHalfedge = d.halfedges[halfedge];

            if (oppositeHalfedge == delaunator::INVALID_INDEX) {
                return SIZE_MAX; // hull edge, no neighbor
            }

            return oppositeHalfedge / 3;
            }
    }

    return SIZE_MAX; // edge not found
}

float cross(const Point2D &A, const Point2D &B) {
    return A.x() * B.y() - A.y() * B.x();
}

float orientation(const Point2D& A,
                          const Point2D& B,
                          const Point2D& C)
{
    const auto crossProd = cross(B - A, C - A);

    return crossProd;
}

float orientation(const TrianglePoints &points) {
    return orientation(points.p0, points.p1, points.p2);
}

bool lexLess(const Point2D& u, const Point2D& v) {
    return (u.x() < v.x()) || (u.x() == v.x() && u.y() < v.y());
}

bool inHalfOpen(const Point2D& a, const Point2D& b, const Point2D& p)

{
    const Point2D& lo = lexLess(a,b) ? a : b;
    const Point2D& hi = lexLess(a,b) ? b : a;

    bool cond1 = !lexLess(p, lo);
    bool cond2 = lexLess(p, hi);
    return cond1 && cond2;
}

bool horizontalRayIntersectsEdge(const Point2D& A,
                                 const Point2D& B,
                                 const Point2D& P)
{
    // Ignore horizontal edges
    if (A.y() == B.y())
        return false;

    // Check if P.y is in the half-open vertical span of AB
    if (!((A.y() > P.y()) != (B.y() > P.y())))
        return false;

    // Compute x-coordinate of intersection
    double x = A.x() + (P.y() - A.y()) * (B.x() - A.x()) / (B.y() - A.y());

    // True if intersection is strictly to the right
    return x > P.x();
}

// Check if point q is on the "other side" of edge e relative to triangle t
// Edge e is defined by the halfedge index in the triangle
bool pointOnOtherSide(const delaunator::Delaunator& d,
                      size_t triangle,
                      size_t edgeIdx,  // 0, 1, or 2 for which edge of the triangle
                      const Point2D& q) {
    const auto triPoints = TrianglePoints::create(d, triangle);
    assert(triPoints != std::nullopt);

    const auto edge = getEdgeVertices(d, triangle, edgeIdx);
    const auto innerVertex = getOppositeVertex(d, triangle, edgeIdx);

    const Point2D A = getPointFromVertex(d, edge.first);
    const Point2D B = getPointFromVertex(d, edge.second);
    const Point2D C = getPointFromVertex(d, innerVertex);
    const Point2D &P = q;

    const auto s1 = cross(B-A,P-A);
    const auto s2 = cross(B-A, C-A);

    const auto mult = s1 * s2;
    if (mult <= 0) {
        return true;
    }
    // if (mult > 0) {
    return false;
    // }
    // assert(mult == 0.f); was used for point on edge, but in current use case–walking through to get to containing triangle–it shouldn't matter
    // return inHalfOpen(A, B, P);
}

// Get the third vertex of a triangle given two known vertices
// Returns SIZE_MAX if v1 or v2 are not in the triangle
size_t getThirdVertex(const delaunator::Delaunator& d, size_t triangleIdx, size_t v1, size_t v2) {
    size_t t0 = triangleIdx * 3;

    std::array<size_t, 3> vertices = {
        d.triangles[t0],
        d.triangles[t0 + 1],
        d.triangles[t0 + 2]
    };

    // First check that both v1 and v2 are actually in the triangle
    bool hasV1 = false;
    bool hasV2 = false;

    for (size_t v : vertices) {
        if (v == v1) hasV1 = true;
        if (v == v2) hasV2 = true;
    }

    if (!hasV1 || !hasV2) {
        return SIZE_MAX;
    }

    // Now find the third vertex
    for (size_t v : vertices) {
        if (v != v1 && v != v2) {
            return v;
        }
    }

    return SIZE_MAX;
}

// Get vertex index from point (assumes point exactly matches a vertex in coords)
[[deprecated("uses linear search")]]
size_t getVertexIndex(const delaunator::Delaunator& d, const Point2D& point) {
    const float EPSILON = 1e-6f;

    for (size_t i = 0; i < d.coords.size() / 2; ++i) {
        const float vx = static_cast<float>(d.coords[2 * i]);
        const float vy = static_cast<float>(d.coords[2 * i + 1]);

        if (std::abs(vx - point.x()) < EPSILON && std::abs(vy - point.y()) < EPSILON) {
            return i;
        }
    }
    return SIZE_MAX;
}

Point2D getPointFromVertex(const delaunator::Delaunator& d, const size_t vertexIdx) {
    assert (vertexIdx < d.coords.size());
    return {
        static_cast<float>(d.coords[2 * vertexIdx + 0]),
        static_cast<float>(d.coords[2 * vertexIdx + 1])
    };
}

std::pair<size_t, size_t> getEdgeVertices(const delaunator::Delaunator& d,
                                          size_t triangle,
                                          size_t edgeIdx) {
    size_t baseIdx = triangle * 3;
    size_t halfedge = baseIdx + edgeIdx;
    size_t nextHalfedge = (edgeIdx == 2) ? baseIdx : halfedge + 1;

    size_t v1 = d.triangles[halfedge];
    size_t v2 = d.triangles[nextHalfedge];

    return {v1, v2};
}
size_t getOppositeVertex(const delaunator::Delaunator& d,
                         size_t triangle,
                         size_t edgeIdx)
{
    size_t base = triangle * 3;
    return d.triangles[base + (edgeIdx + 2) % 3];
}

class _Vertex {
    // just used for keeping invariants together, NOT a general purpose vertex class!
public:
    _Vertex(const delaunator::Delaunator &d, const char *label)
    :   _d(d)

    , _label(label)

    {}
    void set(size_t idx) {
        _id = idx;
        _p = getPointFromVertex(_d, _id);

        printPoint();

        }
    void set(const Point2D &p) {
        _p = p;

        printPoint();

    }
    [[nodiscard]] const Point2D &point() const {
        return _p;
    }
    [[nodiscard]] size_t id() const {
        return _id;
    }

    void printPoint() {
#ifndef WALK_STRING_DEBUGGING
        const auto s = fmt::format("{}_{}: {}\n", _label, _count++, str(_p));
        fmt::print("{}", s);
#endif
    };

private:
    size_t _id {SIZE_MAX};
    size_t _count {0};
    Point2D _p;
    const delaunator::Delaunator &_d;

    const char *_label;

};

// Remembering stochastic walk - refines the triangle location
std::optional<size_t> rememberingStochasticWalk(const delaunator::Delaunator& d,
                                                 const Point2D& p,
                                                 size_t startTri) {
#ifndef WALK_STRING_DEBUGGING
    fmt::print("rememberingStochasticWalk called with:\n startTri={}\n target_p=({}, {})\n", startTri, p.x(), p.y());
#endif
    size_t t = startTri;
    size_t previous = t;

    bool end = false;

    auto neighborThroughEdge_ifShouldWalkHere = [&d, &p](const size_t currentTriangle, const size_t previousTriangle, const size_t edgeIdx) -> std::optional<size_t> {
        const auto [edge_v0, edge_v1] = getEdgeVertices(d, currentTriangle, edgeIdx);
        const auto neighborThroughE = neighbor(d, currentTriangle, edge_v0, edge_v1);

        {
            const auto v0 = getPointFromVertex(d, edge_v0);
            const auto v1 = getPointFromVertex(d, edge_v1);
            const auto currentTrianglePoints = TrianglePoints::create(d, currentTriangle);
            if (currentTrianglePoints == std::nullopt) {
#ifndef WALK_STRING_DEBUGGING
                fmt::print("\t\tcurrent triangle not valid... not sure what to do quite yet.\n");
#endif
                jassertfalse;
            }
            else {
#ifndef WALK_STRING_DEBUGGING
                fmt::print("\t\tcurrent triangle points: {}\n", str(*currentTrianglePoints));
                fmt::print("\t\tedge considered e: {}, {}\n", str(v0), str(v1));
#endif
                const auto neighborPoints = TrianglePoints::create(d, neighborThroughE);
                if (neighborPoints == std::nullopt) {
#ifndef WALK_STRING_DEBUGGING
                    fmt::print("\t\tneighbor through e not valid... not sure what to do yet.\n");
#endif
                }
                else {
#ifndef WALK_STRING_DEBUGGING
                    fmt::print("\t\tneighbor through e: {}\n", str(*neighborPoints));
#endif
                }
            }
        }

        if (neighborThroughE != delaunator::INVALID_INDEX &&
                neighborThroughE != previousTriangle &&
                    pointOnOtherSide(d, currentTriangle, edgeIdx, p)) {
            {
#ifndef WALK_STRING_DEBUGGING
                fmt::print("\t\tYES the point is on that side (and it wasn't previously checked and not invalid)\n\n");
#endif
            }
            return neighborThroughE;
        }
#ifndef WALK_STRING_DEBUGGING
        fmt::print("\t\tNO the point NOT is on that side.\n\n");
#endif
        return std::nullopt;
    };

    while (!end) {
        size_t e = rand() % 3;

        if (auto neighbor = neighborThroughEdge_ifShouldWalkHere(t, previous, e); neighbor != std::nullopt)
        {
            previous = t;
            t = neighbor.value();
        }
        else
        {
            e += 1; e %= 3; // next edge
            if (neighbor = neighborThroughEdge_ifShouldWalkHere(t, previous, e); neighbor != std::nullopt)
            {
                previous = t;
                t = neighbor.value();
            }
            else {
                e += 1; e %= 3; // next edge
                if (neighbor = neighborThroughEdge_ifShouldWalkHere(t, previous, e); neighbor != std::nullopt)
                {
                    previous = t;
                    t = neighbor.value();
                }
                else {
                    end=true;
                }
            }
        }
    }
    const auto trianglePoints = TrianglePoints::create(d, t);
    if (pointInTriangle(p, trianglePoints->p0, trianglePoints->p1, trianglePoints->p2)) {
        return t;
    }
    return std::nullopt;
}

size_t getVertexFromHalfedge(const delaunator::Delaunator& d, size_t halfedgeIdx) {
    return d.triangles[halfedgeIdx];
}

std::optional<size_t> straightWalk(const delaunator::Delaunator &d, const Point2D &_p, size_t startTri) {
    // traverses the triangulation T, following the line segment from q to p.

    printTriangles(d);

    _Vertex p(d, "p");
    p.set(_p);

    _Vertex q(d, "q");
    _Vertex r(d, "r");
    _Vertex l(d, "l");
    _Vertex s(d, "s");

    startTri = std::min(startTri, d.triangles.size() / 3); // prevent lookup error int triangle points::create
    const auto qrlOpt = TrianglePoints::create(d, startTri);
    assert(qrlOpt != std::nullopt);

    auto t = startTri; printTriangleIdx(t);

    q.set(getVertexFromHalfedge(d, qrlOpt->halfedge0));
    r.set(getVertexFromHalfedge(d, qrlOpt->halfedge2));
    l.set(getVertexFromHalfedge(d, qrlOpt->halfedge1));

    if (p.point() == q.point() || p.point() == r.point() || p.point() == l.point()) {
        return t;
    }

    auto neighborValidityCheck = [&d](size_t t, const _Vertex &l, const _Vertex &r) {
        auto a = d.triangles[t*3 + 0];
        auto b = d.triangles[t*3 + 1];
        auto c = d.triangles[t*3 + 2];

        bool l_in = (l.id() == a || l.id() == b || l.id() == c);
        bool r_in = (r.id() == a || r.id() == b || r.id() == c);

        assert(l_in && r_in);
    };

    if (orientation(r.point(), q.point(), p.point()) < 0) {
        while (orientation(l.point(), q.point(), p.point()) < 0) {
            r.set(l.id());
            neighborValidityCheck(t, l, r);
            t = neighbor(d, t, q.id(), l.id());
            if (t == SIZE_MAX) {
                return std::nullopt;
            }
            printTriangleIdx(t);
            l.set(getThirdVertex(d, t, q.id(), r.id()));
        }
    }
    else {
        do {
            l.set(r.id());
            neighborValidityCheck(t, l, r);
            t = neighbor(d, t, q.id(), r.id());
            if (t == SIZE_MAX) {
                return std::nullopt;
            }
            printTriangleIdx(t);
            r.set(getThirdVertex(d, t, q.id(), l.id()));
        } while (orientation(r.point(), q.point(), p.point()) > 0);
    }
    if (pointInTriangle(p.point(), q.point(), r.point(), l.point())) {
        return t;
    }
    // end of initialization step.
    assert(orientation(p.point(), r.point(), l.point()) <= 0 && "Initialization failed to bracket qp");
    // now qp has r on its right and l on its left.
    while (orientation(p.point(), r.point(), l.point()) < 0) {
        neighborValidityCheck(t, l, r);
        t = neighbor(d, t, r.id(), l.id());
        if (t == SIZE_MAX) {
            return std::nullopt;
        }
        printTriangleIdx(t);
        s.set(getThirdVertex(d, t, r.id(), l.id()));
        if (orientation(s.point(), q.point(), p.point()) < 0) {
            r.set(s.id());
            l.printPoint();
        } else {
            l.set(s.id());
            r.printPoint();
        }
    }
    return t;
}
[[deprecated("not working, just use straight walk")]]
std::optional<size_t> hybridWalk(const delaunator::Delaunator &d, const Point2D &_q, const size_t startTri_α)
{
    printTriangles(d);

    _Vertex q(d, "q");
    q.set(_q);

    auto τ = startTri_α;
    printTriangleIdx(τ);

    const auto lrsOpt = TrianglePoints::create(d, startTri_α);
    assert(lrsOpt.has_value());
    // FROM PAPER: "we assume that the vertices of the triangles are in a CCW order."
    // THIS is NOT the case with Delaunator!!! so we access vertices in reverse order
    _Vertex l(d, "l"); l.set(getVertexFromHalfedge(d, lrsOpt->halfedge0));
    _Vertex r(d, "r"); r.set(getVertexFromHalfedge(d, lrsOpt->halfedge2));
    _Vertex s(d, "s"); s.set(getVertexFromHalfedge(d, lrsOpt->halfedge1));

    _Vertex p(d, "p");
    p.set(s.id());
    printLine(p.point(), q.point());

    if (p.point() == q.point()) {
        return τ; // already found
    }

    const auto a = q.point() - p.point();
    // k = ||a||
    const float k = a.norm();
    const float kcos = a.x() / k;
    const float ksin = a.y() / k;
    Point2D q_prime(
      q.point().x() * kcos - q.point().y() * ksin,
      q.point().x() * ksin + q.point().y() * kcos
    );

    if (double r_prime_y = r.point().x() * ksin + r.point().y() * kcos;
        r_prime_y > q_prime.y())
    {
      // r is above line pq
      double l_prime_y = l.point().x() * ksin + l.point().y() * kcos;
      while (l_prime_y > q_prime.y()) { // IN PAPER this MIIIGHT be a do while loop instead...
        assert(orientation(p.point(), r.point(), l.point()) <= 0);
        τ = neighbor(d, τ, p.id(), l.id());
        if (τ == SIZE_MAX) {
          return std::nullopt;
        }
        printTriangleIdx(τ);
        r.set(l.id());
        l.set(getThirdVertex(d, τ, p.id(), r.id()));
        l_prime_y = l.point().x() * ksin + l.point().y() * kcos;
      }
      s.set(l.id());
      l.set(r.id());
      r.set(p.id());
    }
    else {
      // r is below line pq
      while (true) {
          assert(orientation(p.point(), r.point(), l.point()) <= 0);
          // from paper: "repeat: "
          τ = neighbor(d, τ, p.id(), r.id()); // τ = neighbor of τ trough ε_pr;
          if (τ == SIZE_MAX) {
              return std::nullopt;
          }
          printTriangleIdx(τ);
          l.set(r.id());
          r.set(getThirdVertex(d, τ, p.id(), l.id()));
          r_prime_y = r.point().x() * ksin + r.point().y() * kcos;
          if (r_prime_y > q_prime.y()) { break; }
      }
      s.set(r.id());
      r.set(l.id());
      l.set(p.id());
    }
    // now line pq intersects τ
    // walk step - following the line segment pq
    Point2D s_prime(
      s.point().x() * kcos - s.point().y() * ksin,
      std::numeric_limits<float>::quiet_NaN() // unassigned yet
    );
    while (s_prime.x() < q_prime.x()) {
      s_prime.y() = s.point().x() * ksin + s.point().y() * kcos;
      if (s_prime.y() < q_prime.y()){
        r.set(s.id()); // only used for getting third vertex; no need to update actual point, just index
      } else {
        l.set(s.id()); // only used for getting third vertex; no need to update actual point, just index
      }
      τ = neighbor(d, τ, l.id(), r.id()); // τ = neighbor of τ trough ε_pr;
      if (τ == SIZE_MAX) {
        return std::nullopt;
      }
      printTriangleIdx(τ);
      s.set(getThirdVertex(d, τ, r.id(), l.id()));
      s_prime.x() = s.point().x() * kcos - s.point().y() * ksin;
    }
#ifdef DISABLE_STOCHASTIC_REFINEMENT
     return τ;
#endif
    return rememberingStochasticWalk(d, q.point(), τ);
}
//=============================================================================================================================
#if 0
std::vector<WeightedIdx> toWeightedIndices(
    const std::vector<DistanceIdx> &dv,
    const double sharpness, const double contrastPower)
{
    if (dv.empty()) {
        return {};
    }

    // 1) find maximum distance for normalization
    double dmax = 0.0;
    for (const auto& di : dv) {
        dmax = std::max(dmax, di.distance);
    }

    // 2) Convert distances to initial weights using softmax
    std::vector<double> weights;
    weights.reserve(dv.size());

    if (dmax <= 0.0) {
        // All distances are zero - uniform weights
        const double uniformWeight = 1.0 / static_cast<double>(dv.size());
        weights.assign(dv.size(), uniformWeight);
    } else {
        // Apply softmax: exp(-sharpness * (distance/dmax))
        for (const auto& di : dv) {
            const double normalizedDist = di.distance / dmax;
            weights.push_back(std::exp(-sharpness * normalizedDist));
        }

        // Normalize to sum = 1
        const double sum = std::accumulate(weights.begin(), weights.end(), 0.0);
        for (auto& w : weights) {
            w /= sum;
        }
    }

    // 3) apply contrast power scaling
    if (contrastPower != 1.0) {
        for (auto& w : weights) {
            w = std::pow(w, contrastPower);
        }

        // re-normalize after power scaling
        if (const double sum = std::accumulate(weights.begin(), weights.end(), 0.0);
            sum > 0.0)
        {
            for (auto& w : weights) {
                w /= sum;
            }
        }
    }

    // 4) build result vector with WeightedIdx structs
    std::vector<WeightedIdx> result;
    result.reserve(dv.size());

    for (size_t i = 0; i < dv.size(); ++i) {
        result.emplace_back(dv[i].idx, weights[i]);
    }

    return result;
}
/**
 * Finds the K nearest points to `target` in `database`,
 * builds softmax-style weights over those K,
 * then either returns them all, or samples `numToPick` **without replacement**.
 *
 * @param  target          the target point from which to find nearest points
 * @param  database        your full set of Timbre5DPoints
 * @param  K               how many nearest neighbors to consider (caps at database.size())
 * @param  numToPick       if <=0, we return all K weighted indices;
 *                         otherwise we pick exactly numToPick **without** replacement.
 * @param  sharpness       0=flat, >0 biases toward closer points
 * @param  higher3Dweight  extra weighting on the 3D portion of your distance metric
 */
std::vector<WeightedIdx> findPointsDistanceBased(
    const Timbre5DPoint&               target,
    const juce::Array<Timbre5DPoint>&  database,
    int                                K,
    const int                          numToPick,
    double                             sharpness,
    float                              higher3Dweight)
{
    if (database.isEmpty()) { return {}; }

    std::vector<WeightedIdx> weightedIndices = [&database, target, higher3Dweight, &K, sharpness]()
    {
       // compute squared distances
       std::vector<DistanceIdx> distIdx;
       distIdx.reserve (database.size());
       for (int i = 0; i < database.size(); ++i) {
          auto const& p = database.getReference(i);

          // Extract 2D portions and compute difference
          Point2D diff2D = get2D(p) - get2D(target);

          // Extract 3D portions and compute difference
          Timbre3DPoint diff3D = get3D(p) - get3D(target);

          // Compute squared distance
          double d2 = diff2D.squaredNorm() + higher3Dweight * diff3D.squaredNorm();

          distIdx.emplace_back (i, d2);
       }

       // keep only the K nearest
       {
          K = std::min<int> (K, (int) distIdx.size());
          std::ranges::nth_element (distIdx, distIdx.begin() + K
                                    ,
                                    [](auto& a, auto& b){ return a.distance < b.distance; });
          distIdx.resize (K);
          assert(distIdx.size() == static_cast<size_t>(K));
       }
       return toWeightedIndices(distIdx, sharpness);
    }();

    /** if caller just wants the full distribution, return it: */
    if (numToPick <= 0 || numToPick >= K) { return weightedIndices; }

    /** otherwise sample numToPick *without replacement* by repeatedly drawing
     from the discrete distribution then zeroing out that weight and re-normalizing. */
    std::vector<WeightedIdx> picked;
    picked.reserve (numToPick);

    // extract weights into their own vector using views
    auto weightView = weightedIndices | std::views::transform([](const auto& wi) { return wi.weight; });
    std::vector<double> weightArr(weightView.begin(), weightView.end());

    std::mt19937 rng { std::random_device{}() };
    for (int pick = 0; pick < numToPick; ++pick) {
       // construct the discrete_distribution over the weights
       std::discrete_distribution<int> dist(weightArr.begin(), weightArr.end());
       int choice = dist(rng);
       picked.push_back(weightedIndices[choice]);
       // zero out that weight so it's never picked again
       weightArr[choice] = 0.0;

       // Check if any weights remain (optional optimization)
       if (std::ranges::all_of(weightArr, [](const double w) { return w == 0.0; })) {
          break;
       }
    }
    return picked;
}
#endif

}	// namespace nvs::timbrespace

