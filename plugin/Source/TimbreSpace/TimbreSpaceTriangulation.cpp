/*
  ==============================================================================

    TimbreSpaceTriangulation.cpp
    Created: 2 Jul 2025 5:52:14pm
    Author:  Nicholas Solem

  ==============================================================================
*/

#include "TimbreSpaceTriangulation.h"


namespace nvs::timbrespace {

std::vector<double> make2dCoordinates(juce::Array<Timbre5DPoint> points){
	std::vector<double> coords;
	coords.reserve(points.size() * 2);
	for (auto const & p : points){
		coords.push_back(p[0]);
		coords.push_back(p[1]);
	}
	assert(coords.size() == static_cast<size_t>(points.size() * 2));
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

// Compute barycentric coordinates for interpolation weights
// Compute barycentric coordinates for interpolation weights
std::array<double, 3> computeBarycentricWeights(const Timbre2DPoint& p,
                                     const Timbre2DPoint& a,
                                     const Timbre2DPoint& b,
                                     const Timbre2DPoint& c) {

    Timbre2DPoint v0 = c - a;
    Timbre2DPoint v1 = b - a;
    Timbre2DPoint v2 = p - a;

    double dot00 = v0.dot(v0);
    double dot01 = v0.dot(v1);
    double dot02 = v0.dot(v2);
    double dot11 = v1.dot(v1);
    double dot12 = v1.dot(v2);

    double invDenom = 1.0 / (dot00 * dot11 - dot01 * dot01);
    double u = (dot11 * dot02 - dot01 * dot12) * invDenom;
    double v = (dot00 * dot12 - dot01 * dot02) * invDenom;
    double w = 1.0 - u - v;

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
