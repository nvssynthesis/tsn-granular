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
		coords.push_back(p.get2D().getX());
		coords.push_back(p.get2D().getY());
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
bool pointInTriangle(const timbre2DPoint& p, const timbre2DPoint& a, const timbre2DPoint& b, const timbre2DPoint& c) {
	// Compute vectors
	double v0x = c.x - a.x, v0y = c.y - a.y;
	double v1x = b.x - a.x, v1y = b.y - a.y;
	double v2x = p.x - a.x, v2y = p.y - a.y;
	
	// Compute dot products
	double dot00 = v0x * v0x + v0y * v0y;
	double dot01 = v0x * v1x + v0y * v1y;
	double dot02 = v0x * v2x + v0y * v2y;
	double dot11 = v1x * v1x + v1y * v1y;
	double dot12 = v1x * v2x + v1y * v2y;
	
	// Compute barycentric coordinates
	double invDenom = 1.0 / (dot00 * dot11 - dot01 * dot01);
	double u = (dot11 * dot02 - dot01 * dot12) * invDenom;
	double v = (dot00 * dot12 - dot01 * dot02) * invDenom;
	
	// Check if point is in triangle
	return (u >= 0) && (v >= 0) && (u + v <= 1);
}

// Find triangle containing target point using Delaunator results
std::optional<std::array<size_t, 3>> findContainingTriangle(const delaunator::Delaunator& d,
															const timbre2DPoint& target)
{
	
	// Iterate through all triangles
	for (size_t i = 0; i < d.triangles.size(); i += 3) {
		// Get triangle vertex indices
		size_t idx0 = d.triangles[i];
		size_t idx1 = d.triangles[i + 1];
		size_t idx2 = d.triangles[i + 2];
		
		// Get triangle vertex coordinates
		timbre2DPoint a(d.coords[2 * idx0], d.coords[2 * idx0 + 1]);
		timbre2DPoint b(d.coords[2 * idx1], d.coords[2 * idx1 + 1]);
		timbre2DPoint c(d.coords[2 * idx2], d.coords[2 * idx2 + 1]);
		
		// Test if target is inside this triangle
		if (pointInTriangle(target, a, b, c)) {
			return std::array<size_t, 3>{idx0, idx1, idx2};
		}
	}
	
	// No triangle contains the point
	return std::nullopt;
}

// Compute barycentric coordinates for interpolation weights
std::array<double, 3> computeBarycentricWeights(const timbre2DPoint& p,
												const timbre2DPoint& a,
												const timbre2DPoint& b,
												const timbre2DPoint& c) {
	
	double v0x = c.x - a.x, v0y = c.y - a.y;
	double v1x = b.x - a.x, v1y = b.y - a.y;
	double v2x = p.x - a.x, v2y = p.y - a.y;
	
	double dot00 = v0x * v0x + v0y * v0y;
	double dot01 = v0x * v1x + v0y * v1y;
	double dot02 = v0x * v2x + v0y * v2y;
	double dot11 = v1x * v1x + v1y * v1y;
	double dot12 = v1x * v2x + v1y * v2y;
	
	double invDenom = 1.0 / (dot00 * dot11 - dot01 * dot01);
	double u = (dot11 * dot02 - dot01 * dot12) * invDenom;
	double v = (dot00 * dot12 - dot01 * dot02) * invDenom;
	double w = 1.0 - u - v;
	
	return {w, v, u}; // weights for vertices a, b, c respectively
}

// Usage example:
/*
 Delaunator d(coordinates);
 timbre2DPoint target(x, y);
 
 auto triangleOpt = findContainingTriangle(d, target);
 if (triangleOpt) {
 auto [idx0, idx1, idx2] = *triangleOpt;
 
 // Get the actual triangle points
 timbre2DPoint a(d.coords[2 * idx0], d.coords[2 * idx0 + 1]);
 timbre2DPoint b(d.coords[2 * idx1], d.coords[2 * idx1 + 1]);
 timbre2DPoint c(d.coords[2 * idx2], d.coords[2 * idx2 + 1]);
 
 // Compute interpolation weights
 auto weights = computeBarycentricWeights(target, a, b, c);
 
 // Use idx0, idx1, idx2 as your timbre point indices
 // Use weights[0], weights[1], weights[2] as amplitudes
 }
*/

class Foo {
public:
	void foo(){
		std::vector<double> coords = {-1, 1, 1, 1, 1, -1, -1, -1};
		
		//triangulation happens here
		delaunator::Delaunator d(coords);
		
		for(std::size_t i = 0; i < d.triangles.size(); i+=3) {
			printf(
				   "Triangle points: [[%f, %f], [%f, %f], [%f, %f]]\n",
				   d.coords[2 * d.triangles[i]],        //tx0
				   d.coords[2 * d.triangles[i] + 1],    //ty0
				   d.coords[2 * d.triangles[i + 1]],    //tx1
				   d.coords[2 * d.triangles[i + 1] + 1],//ty1
				   d.coords[2 * d.triangles[i + 2]],    //tx2
				   d.coords[2 * d.triangles[i + 2] + 1] //ty2
				   );
		}
	}
};

}	// namespace nvs::timbrespace
