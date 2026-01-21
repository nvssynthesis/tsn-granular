/*
  ==============================================================================

    TimbreSpaceTriangulation.cpp
    Created: 2 Jul 2025 5:52:14pm
    Author:  Nicholas Solem

  ==============================================================================
*/

#include "TimbreSpaceTriangulation.h"
#include <set>
#include <unordered_set>
#include <cassert>

#ifndef DBG
#include <iostream>
#include "fmt/core.h"
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

void getUniqueEdges(const delaunator::Delaunator& d)
/**
 use this to get all edges, non-redundantly, to then draw them
 */
{
	std::set<std::pair<size_t, size_t>> edges;

	// Collect all edges from all triangles
	for (size_t i = 0; i < d.triangles.size(); i += 3) {
		size_t a = d.triangles[i];
		size_t b = d.triangles[i + 1];
		size_t c = d.triangles[i + 2];
		
		// Add edges (ensure consistent ordering)
		edges.insert({std::min(a,b), std::max(a,b)});
		edges.insert({std::min(b,c), std::max(b,c)});
		edges.insert({std::min(c,a), std::max(c,a)});
	}

	// Draw all unique edges
	for (const auto& [idx1, idx2] : edges) {
//		drawLine(idx1, idx2);
		std::cout << idx1 << ", " << idx2 << "\n";
	}
}

// Test if point p is inside triangle formed by vertices a, b, c
bool pointInTriangle(const Timbre2DPoint& p, const Timbre2DPoint& a, const Timbre2DPoint& b, const Timbre2DPoint& c) {
    // Compute vectors
    const Timbre2DPoint v0 = c - a;
    const Timbre2DPoint v1 = b - a;
    const Timbre2DPoint v2 = p - a;

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

// barycentric method
bool pointInTriangle2(const Timbre2DPoint& p, const Timbre2DPoint& a, const Timbre2DPoint& b, const Timbre2DPoint& c) {
    constexpr float EPSILON = 1e-6;

    auto sign = [](const Timbre2DPoint& p1, const Timbre2DPoint& p2, const Timbre2DPoint& p3) {
        return (p1.x() - p3.x()) * (p2.y() - p3.y()) - (p2.x() - p3.x()) * (p1.y() - p3.y());
    };

    double d1 = sign(p, a, b);
    double d2 = sign(p, b, c);
    double d3 = sign(p, c, a);

    bool has_neg = (d1 < -EPSILON) || (d2 < -EPSILON) || (d3 < -EPSILON);
    bool has_pos = (d1 > EPSILON) || (d2 > EPSILON) || (d3 > EPSILON);

    return !(has_neg && has_pos);
}

// Find triangle containing target point using Delaunator results
std::optional<std::array<size_t, 3>> findContainingTriangle(const delaunator::Delaunator& d,
                                              const Timbre2DPoint& target)
{

    // Iterate through all triangles
    for (size_t i = 0; i < d.triangles.size(); i += 3) {
       // Get triangle vertex indices
       size_t idx0 = d.triangles[i];
       size_t idx1 = d.triangles[i + 1];
       size_t idx2 = d.triangles[i + 2];

       // Get triangle vertex coordinates
       Timbre2DPoint a(d.coords[2 * idx0], d.coords[2 * idx0 + 1]);
       Timbre2DPoint b(d.coords[2 * idx1], d.coords[2 * idx1 + 1]);
       Timbre2DPoint c(d.coords[2 * idx2], d.coords[2 * idx2 + 1]);

       // Test if target is inside this triangle
       if (pointInTriangle(target, a, b, c)) {
          return std::array<size_t, 3>{idx0, idx1, idx2};
       }
    }

    // No triangle contains the point
    return std::nullopt;
}

std::array<double, 3> computeDistanceWeights(const Timbre2DPoint& p,
                                 const Timbre2DPoint& a,
                                 const Timbre2DPoint& b,
                                 const Timbre2DPoint& c)
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
Timbre2DPoint projectPointOntoSegment(const Timbre2DPoint& p,
                                      const Timbre2DPoint& a,
                                      const Timbre2DPoint& b) {
    assert(std::isfinite(a[0]) && std::isfinite(a[1]));
    assert(std::isfinite(b[0]) && std::isfinite(b[1]));
    assert(std::isfinite(p[0]) && std::isfinite(p[1]));

    Timbre2DPoint ab = b - a;
    Timbre2DPoint ap = p - a;

    double ab_squared = ab.dot(ab);
    if (ab_squared < 1e-10) return a; // degenerate segment

    double t = ap.dot(ab) / ab_squared;
    t = std::clamp(t, 0.0, 1.0); // stay on segment by clamp

    return {a[0] + t * ab[0], a[1] + t * ab[1]};
}

// find which edge is closest to p and project onto it
Timbre2DPoint clampToTriangle(const Timbre2DPoint& p,
                               const Timbre2DPoint& a,
                               const Timbre2DPoint& b,
                               const Timbre2DPoint& c) {
    // project onto each edge
    Timbre2DPoint proj_ab = projectPointOntoSegment(p, a, b);
    Timbre2DPoint proj_bc = projectPointOntoSegment(p, b, c);
    Timbre2DPoint proj_ca = projectPointOntoSegment(p, c, a);

    // closest projection
    double dist_ab = (p - proj_ab).norm();
    double dist_bc = (p - proj_bc).norm();
    double dist_ca = (p - proj_ca).norm();

    if (dist_ab <= dist_bc && dist_ab <= dist_ca) return proj_ab;
    if (dist_bc <= dist_ca) return proj_bc;
    return proj_ca;
}

// compute barycentric coordinates for interpolation weights
std::array<double, 3> computeBarycentricWeights(const Timbre2DPoint& p,
                                     const Timbre2DPoint& a,
                                     const Timbre2DPoint& b,
                                     const Timbre2DPoint& c) {

    const Timbre2DPoint v0 = c - a;
    const Timbre2DPoint v1 = b - a;
    const Timbre2DPoint v2 = p - a;

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
        const Timbre2DPoint clamped = clampToTriangle(p, a, b, c);

        // recompute with clamped point
        const Timbre2DPoint v0_c = c - a;
        const Timbre2DPoint v1_c = b - a;
        const Timbre2DPoint v2_c = clamped - a;

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
 Timbre2DPoint target(x, y);
 
 auto triangleOpt = findContainingTriangle(d, target);
 if (triangleOpt) {
 auto [idx0, idx1, idx2] = *triangleOpt;
 
 // Get the actual triangle points
 Timbre2DPoint a(d.coords[2 * idx0], d.coords[2 * idx0 + 1]);
 Timbre2DPoint b(d.coords[2 * idx1], d.coords[2 * idx1 + 1]);
 Timbre2DPoint c(d.coords[2 * idx2], d.coords[2 * idx2 + 1]);
 
 // Compute interpolation weights
 auto weights = computeBarycentricWeights(target, a, b, c);
 
 // Use idx0, idx1, idx2 as your timbre point indices
 // Use weights[0], weights[1], weights[2] as amplitudes
 }
*/

// triangulation-based point selection function
std::vector<WeightedIdx> findPointsTriangulationBased(const Timbre5DPoint& target,
    const std::vector<Timbre5DPoint>& database, const delaunator::Delaunator &d)
{
	if (database.empty()) { return {}; }
    const auto dbSizeAtStart = database.size();

	// Need at least 3 points for triangulation
	jassert (database.size() >= 3);

	// Find triangle containing the target point
	const Timbre2DPoint targetPoint = get2D(target);
	const auto triangleOpt = findContainingTriangle(d, targetPoint);

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
	const Timbre2DPoint p0 = get2D(database[idx0]);
	const Timbre2DPoint p1 = get2D(database[idx1]);
	const Timbre2DPoint p2 = get2D(database[idx2]);

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

	Timbre2DPoint targetPoint = get2D(target);
	double minDistance = std::numeric_limits<double>::max();
	std::array<size_t, 3> bestTriangle {0, 1, 2};

	// Check all triangles and find the one with minimum distance to target
    std::array<Timbre2DPoint, 3> bestPoints;
	for (size_t i = 0; i < d.triangles.size(); i += 3) {
		const size_t idx0 = d.triangles[i];
		const size_t idx1 = d.triangles[i + 1];
		const size_t idx2 = d.triangles[i + 2];

		Timbre2DPoint p0 = get2D(database[idx0]);
		Timbre2DPoint p1 = get2D(database[idx1]);
		Timbre2DPoint p2 = get2D(database[idx2]);

		// Calculate centroid of triangle
		Timbre2DPoint centroid = (p0 + p1 + p2) / 3.0;

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




std::optional<TrianglePoints> TrianglePoints::create(const delaunator::Delaunator& d, size_t triangleIdx)
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
    // Find the halfedge in 'triangle' that goes from v1 to v2
    size_t baseIdx = triangle * 3;

    for (size_t i = 0; i < 3; ++i) {
        size_t halfedge = baseIdx + i;
        size_t nextHalfedge = (i == 2) ? baseIdx : halfedge + 1;

        // Check if this halfedge goes from v1 to v2
        if (d.triangles[halfedge] == v1 && d.triangles[nextHalfedge] == v2) {
            // Found the edge, get the opposite halfedge
            size_t oppositeHalfedge = d.halfedges[halfedge];

            if (oppositeHalfedge == delaunator::INVALID_INDEX) {
                return SIZE_MAX; // Hull edge, no neighbor
            }

            // Return the triangle index of the neighbor
            return oppositeHalfedge / 3;
        }
    }

    // Edge (v1, v2) not found in this triangle
    return SIZE_MAX;
}

float cross(const Timbre2DPoint &A, const Timbre2DPoint &B) {
    return A.x() * B.y() - A.y() * B.x();
}

Orientation_e orientation(const Timbre2DPoint &A, const Timbre2DPoint &B, const Timbre2DPoint &C) {
    const auto crossProd = cross(B-A, C-A);
    if (crossProd > 0){
        return Orientation_e::CCW;
    }
    if (crossProd < 0) {
        return Orientation_e::CW;
    }
    assert (crossProd == 0);
    return Orientation_e::colinear;
}
Orientation_e orientation(const TrianglePoints &points) {
    return orientation(points.p0, points.p1, points.p2);
}

bool lexLess(const Timbre2DPoint& u, const Timbre2DPoint& v) {
    return (u.x() < v.x()) || (u.x() == v.x() && u.y() < v.y());
}

bool inHalfOpen(const Timbre2DPoint& a, const Timbre2DPoint& b, const Timbre2DPoint& p)

{
    const Timbre2DPoint& lo = lexLess(a,b) ? a : b;
    const Timbre2DPoint& hi = lexLess(a,b) ? b : a;

    bool cond1 = !lexLess(p, lo);
    bool cond2 = lexLess(p, hi);
    return cond1 && cond2;
}

bool horizontalRayIntersectsEdge(const Timbre2DPoint& A,
                                 const Timbre2DPoint& B,
                                 const Timbre2DPoint& P)
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
                      const Timbre2DPoint& q) {
    const auto triPoints = TrianglePoints::create(d, triangle);
    assert(triPoints != std::nullopt);

    const auto edge = getEdgeVertices(d, triangle, edgeIdx);
    const auto innerVertex = getOppositeVertex(d, triangle, edgeIdx);

    const Timbre2DPoint A = getPointFromVertex(d, edge.first);
    const Timbre2DPoint B = getPointFromVertex(d, edge.second);
    const Timbre2DPoint C = getPointFromVertex(d, innerVertex);
    const Timbre2DPoint &P = q;

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
size_t getVertexIndex(const delaunator::Delaunator& d, const Timbre2DPoint& point) {
    const float EPSILON = 1e-6f;

    for (size_t i = 0; i < d.coords.size() / 2; ++i) {
        float vx = static_cast<float>(d.coords[2 * i]);
        float vy = static_cast<float>(d.coords[2 * i + 1]);

        if (std::abs(vx - point.x()) < EPSILON && std::abs(vy - point.y()) < EPSILON) {
            return i;
        }
    }
    return SIZE_MAX;
}

// Get point from vertex index
Timbre2DPoint getPointFromVertex(const delaunator::Delaunator& d, size_t vertexIdx) {
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
std::ostream &operator<<(std::ostream &out, const Timbre2DPoint &p)
{
    out << p.x() << "," << p.y();
    return out;
}

template <typename T>
std::string to_string_with_precision(const T a_value, const int n = 6)
{
    std::ostringstream out;
    out.precision(n);
    out << std::fixed << a_value;
    return std::move(out).str();
}

std::string str(const Timbre2DPoint &p) {
    return std::string("(") +
        to_string_with_precision(p.x(), 2) +
            std::string(", ") +
                to_string_with_precision(p.y(), 2) +
                    std::string(")");
}
std::string str(const TrianglePoints &tri) {
    return std::string("(") +
        str(tri.p0) + std::string(", ") +
            str(tri.p1) + std::string(", ") +
                str(tri.p2) +
                    std::string(")");
}

// Remembering stochastic walk - refines the triangle location
std::optional<size_t> rememberingStochasticWalk(const delaunator::Delaunator& d,
                                                 const Timbre2DPoint& p,
                                                 size_t startTri) {
    fmt::print("rememberingStochasticWalk called with:\n startTri={}\n target_p=({}, {})\n", startTri, p.x(), p.y());
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
                fmt::print("\t\tcurrent triangle not valid... not sure what to do quite yet.\n");
            }
            else {
                fmt::print("\t\tcurrent triangle points: {}\n", str(*currentTrianglePoints));
                fmt::print("\t\tedge considered e: {}, {}\n", str(v0), str(v1));
                const auto neighborPoints = TrianglePoints::create(d, neighborThroughE);
                if (neighborPoints == std::nullopt) {
                    fmt::print("\t\tneighbor through e not valid... not sure what to do yet.\n");
                }
                else {
                    fmt::print("\t\tneighbor through e: {}\n", str(*neighborPoints));
                }
            }
        }

        if (neighborThroughE != delaunator::INVALID_INDEX &&
                neighborThroughE != previousTriangle &&
                    pointOnOtherSide(d, currentTriangle, edgeIdx, p)) {
            {
                fmt::print("\t\tYES the point is on that side (and it wasn't previously checked and not invalid)\n\n");
            }
            return neighborThroughE;
        }
        fmt::print("\t\tNO the point NOT is on that side.\n\n");
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

std::optional<size_t> hybridWalk(const delaunator::Delaunator &d, const Timbre2DPoint &q, size_t startTri_α)
{
    // FROM PAPER: "we assume that the vertices of the triangles are in a CCW order."
    // THIS is NOT the case with Delaunator!!! need cw to ccw conversion?
    auto τ = startTri_α;
    auto lrsOpt = TrianglePoints::create(d, startTri_α);
    assert(lrsOpt.has_value());
    auto lIdx = getVertexFromHalfedge(d, lrsOpt->halfedge0); auto l = getPointFromVertex(d, lIdx); assert(l == lrsOpt->p0);
    auto rIdx = getVertexFromHalfedge(d, lrsOpt->halfedge1); auto r = getPointFromVertex(d, rIdx); assert(r == lrsOpt->p1);
    auto sIdx = getVertexFromHalfedge(d, lrsOpt->halfedge2); auto s = getPointFromVertex(d, sIdx); assert(s == lrsOpt->p2);

    const auto pIdx = sIdx; const auto p = s;
    if (p == q) {
        return τ; // already found
    }

    const auto a = q - p;
    // k = ||a||
    const float kcos = a.x();
    const float ksin = a.y();
    Timbre2DPoint q_prime(
      q.x() * kcos - q.y() * ksin,
      q.x() * ksin + q.y() * kcos
    );
    double r_prime_y = r.x() * ksin + r.y() * kcos;
    if (r_prime_y > q_prime.y()) {
      // r is above line pq
      double l_prime_y = l.x() * ksin + l.y() * kcos;
      while (l_prime_y > q_prime.y()) { // IN PAPER this MIIIGHT be a do while loop instead...
        τ = neighbor(d, τ, pIdx, lIdx); // τ = neighbor of τ trough ε_pl;
        r = l; rIdx = lIdx;
        lIdx = getThirdVertex(d, τ, pIdx, rIdx); l = getPointFromVertex(d, lIdx);
        l_prime_y = l.x() * ksin + l.y() * kcos;
      }
      sIdx = lIdx; s = l;
      lIdx = rIdx; l = r;
      rIdx = pIdx; r = p;
    }
    else {
      // r is below line pq
      do { // from paper: "repeat: "
        τ = neighbor(d, τ, pIdx, rIdx); // τ = neighbor of τ trough ε_pr;
        l = r; lIdx = rIdx;
        rIdx = getThirdVertex(d, τ, pIdx, lIdx); r = getPointFromVertex(d, rIdx);
        r_prime_y = r.x() * ksin + r.y() * kcos;
      } while (r_prime_y <= q_prime.y()); // until r′_y > q′_y;
      sIdx = rIdx; s = r;
      rIdx = lIdx; r = l;
      lIdx = pIdx; l = p;
    }
    // now line pq intersects τ
    // walk step - following the line segment pq
    Timbre2DPoint s_prime(
      s.x() * kcos - s.y() * ksin,
      std::numeric_limits<double>::quiet_NaN() // unassigned yet
    );
    while (s_prime.x() < q_prime.x()) {
      s_prime.y() = s.x() * ksin + s.y() * kcos;
      if (s_prime.y() < q_prime.y()){
        rIdx = sIdx; // only used for getting third vertex; no need to update actual point, just index
      } else {
        lIdx = sIdx; // only used for getting third vertex; no need to update actual point, just index
      }
      τ = neighbor(d, τ, lIdx, rIdx); // τ = neighbor of τ trough ε_pr;
      sIdx = getThirdVertex(d, τ, rIdx, lIdx); s = getPointFromVertex(d, sIdx);
      s_prime.x() = s.x() * kcos - s.y() * ksin;
    }

    return rememberingStochasticWalk(d, q, startTri_α);
}

[[deprecated]]
#pragma message("hybridWalk_old: remove me")
std::optional<size_t> hybridWalk_old(const delaunator::Delaunator& d,
                                 size_t startTriIdx,
                                 const Timbre2DPoint& q) {
    const float EPSILON = 1e-6f;

    // Get starting triangle vertices
    size_t t0 = startTriIdx * 3;
    size_t v0 = d.triangles[t0];
    size_t v1 = d.triangles[t0 + 1];
    size_t v2 = d.triangles[t0 + 2];

    // Early exit if q is at any vertex of starting triangle
    Timbre2DPoint p0 = getPointFromVertex(d, v0);
    Timbre2DPoint p1 = getPointFromVertex(d, v1);
    Timbre2DPoint p2 = getPointFromVertex(d, v2);

    if ((std::abs(p0.x() - q.x()) < EPSILON && std::abs(p0.y() - q.y()) < EPSILON) ||
        (std::abs(p1.x() - q.x()) < EPSILON && std::abs(p1.y() - q.y()) < EPSILON) ||
        (std::abs(p2.x() - q.x()) < EPSILON && std::abs(p2.y() - q.y()) < EPSILON))
    {
        return startTriIdx;
    }

    // Choose p as first vertex
    size_t vp = v0;
    size_t vr = v1;
    size_t vl = v2;
    Timbre2DPoint p = p0;

    // Compute transformation: vector a = q - p
    float ax = q.x() - p.x();
    float ay = q.y() - p.y();
    float kcos = ax;
    float ksin = ay;

    // Transform q
    float qprime_x = q.x() * kcos - q.y() * ksin;
    float qprime_y = q.x() * ksin + q.y() * kcos;

    std::cout << "p: " << p << std::endl <<
        " q: " << q << std::endl <<
            " kcos: " << kcos << std::endl <<
                " ksin: " << ksin << std::endl <<
                    " qprime_x: " << qprime_x  << std::endl <<
                        " qprime_y: " << qprime_y << std::endl;

    // Current triangle
    size_t currentTri = startTriIdx;

    // === INITIALIZATION: Find triangle intersected by pq ===

    Timbre2DPoint r = getPointFromVertex(d, vr);
    float rprime_y = r.x() * ksin + r.y() * kcos;

    // Case 1: r is above the line pq
    if (rprime_y > qprime_y) {
        Timbre2DPoint l = getPointFromVertex(d, vl);
        float lprime_y = l.x() * ksin + l.y() * kcos;

        while (lprime_y > qprime_y - EPSILON) {
            // Cross edge from p to l
            size_t edge_pl = findHalfedge(d, currentTri, vp, vl);
            if (edge_pl == SIZE_MAX || d.halfedges[edge_pl] == delaunator::INVALID_INDEX) {
                std::cout << "Returning nullopt: hit boundary in case 1 creation of edge_pl" << std::endl;
                return std::nullopt; // Hit boundary
            }

            size_t oppositeEdge = d.halfedges[edge_pl];
            currentTri = oppositeEdge / 3;

            vr = vl;
            vl = getThirdVertex(d, currentTri, vp, vr);
            if (vl == SIZE_MAX) {
                std::cout << "Returning nullopt: vl == SIZE_MAX in case 1\n";
                return std::nullopt;
            }
            l = getPointFromVertex(d, vl);
            lprime_y = l.x() * ksin + l.y() * kcos;
        }

        // Update s, l, r for walk step
        size_t vs = vl;
        vl = vr;
        vr = vp;

    } else {
        // Case 2: r is below the line pq
        std::unordered_set<size_t> visitedInInit;
        const size_t MAX_INIT_ITERATIONS = d.triangles.size();
        size_t iterations = 0;

        do {
            if (visitedInInit.contains(currentTri) || iterations++ > MAX_INIT_ITERATIONS) {
                // We're cycling or stuck - break and proceed to walk step
                break;
            }
            visitedInInit.insert(currentTri);

            // Cross edge from p to r
            size_t edge_pr = findHalfedge(d, currentTri, vp, vr);
            if (edge_pr == SIZE_MAX || d.halfedges[edge_pr] == delaunator::INVALID_INDEX) {
                std::cout << "Returning nullopt: edge_pr == SIZE_MAX || d.halfedges[edge_pr] == delaunator::INVALID_INDEX in case 2\n";
                return std::nullopt; // Hit boundary
            }

            size_t oppositeEdge = d.halfedges[edge_pr];
            currentTri = oppositeEdge / 3;

            vl = vr;
            vr = getThirdVertex(d, currentTri, vp, vl);
            if (vr == SIZE_MAX) {
                std::cout << "Returning nullopt: vr == SIZE_MAX in case 2\n";
                return std::nullopt;
            }

            r = getPointFromVertex(d, vr);
            rprime_y = r.x() * ksin + r.y() * kcos;

        } while (rprime_y <= qprime_y + EPSILON);

        // Update s, l, r for walk step
        size_t vs = vr;
        vr = vl;
        vl = vp;
    }
    // At this point, pq intersects currentTri

    std::cout << "After initialization, before walk: currentTri=" << currentTri << std::endl;
    auto debugTri = TrianglePoints::create(d, currentTri);
    assert (debugTri != std::nullopt);
    std::cout << "  Triangle vertices: "
              << debugTri->p0.x() << "," << debugTri->p0.y() << " | "
              << debugTri->p1.x() << "," << debugTri->p1.y() << " | "
              << debugTri->p2.x() << "," << debugTri->p2.y() << std::endl;
    std::cout << "  vl=" << vl << " vr=" << vr << std::endl;


    // === WALK STEP: Follow line segment pq ===

    size_t vs = getThirdVertex(d, currentTri, vl, vr);
    if (vs == SIZE_MAX) {
        std::cout << "Returning nullopt: vs == SIZE_MAX in WALK STEP\n";
        return std::nullopt;
    }

    Timbre2DPoint s = getPointFromVertex(d, vs);
    float sprime_x = s.x() * kcos - s.y() * ksin;

    while (sprime_x < qprime_x) {
        std::cout << "Walk step: sprime_x=" << sprime_x << " qprime_x=" << qprime_x << std::endl;

        float sprime_y = s.x() * ksin + s.y() * kcos;

        size_t edge_lr;
        if (sprime_y < qprime_y) {
            vr = vs;
            edge_lr = findHalfedge(d, currentTri, vl, vr);
        } else {
            vl = vs;
            edge_lr = findHalfedge(d, currentTri, vl, vr);
        }

        std::cout << "Attempting to cross edge_lr=" << edge_lr << std::endl;

        if (edge_lr == SIZE_MAX || d.halfedges[edge_lr] == delaunator::INVALID_INDEX) {
            std::cout << "Returning nullopt: in sprime_x < qprime_x while loop, hit boundary! edge_lr == SIZE_MAX || d.halfedges[edge_lr] == delaunator::INVALID_INDEX" << std::endl;
            return std::nullopt;
        }

        size_t oppositeEdge = d.halfedges[edge_lr];
        currentTri = oppositeEdge / 3;

        vs = getThirdVertex(d, currentTri, vl, vr);
        if (vs == SIZE_MAX) {
            std::cout << "Returning nullopt: in sprime_x < qprime_x while loop,vs == SIZE_MAX" << std::endl;
            return std::nullopt;
        }

        s = getPointFromVertex(d, vs);
        sprime_x = s.x() * kcos - s.y() * ksin;
    }

    std::cout << "After initialization, before walk: currentTri=" << currentTri << std::endl;
    return rememberingStochasticWalk(d, q, currentTri);
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
          Timbre2DPoint diff2D = get2D(p) - get2D(target);

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

