/*
  ==============================================================================

    TimbreSpaceTriangulation.cpp
    Created: 2 Jul 2025 5:52:14pm
    Author:  Nicholas Solem

  ==============================================================================
*/

#include "TimbreSpaceTriangulation.h"


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

    return Timbre2DPoint(a[0] + t * ab[0], a[1] + t * ab[1]);
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

}	// namespace nvs::timbrespace
