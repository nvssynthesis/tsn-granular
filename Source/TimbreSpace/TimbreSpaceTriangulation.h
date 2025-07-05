/*
  ==============================================================================

    TimbreSpaceTriangulation.h
    Created: 2 Jul 2025 5:52:14pm
    Author:  Nicholas Solem

  ==============================================================================
*/

#pragma once

#include "../../delaunator-cpp/include/delaunator.hpp"
#include "TimbrePointTypes.h"

namespace nvs::timbrespace {

std::vector<double> make2dCoordinates(juce::Array<Timbre5DPoint> points);

void getUniqueEdges(const delaunator::Delaunator& d);

// Test if point p is inside triangle formed by vertices a, b, c
bool pointInTriangle(const timbre2DPoint& p, const timbre2DPoint& a, const timbre2DPoint& b, const timbre2DPoint& c) ;

// Find triangle containing target point using Delaunator results
std::optional<std::array<size_t, 3>> findContainingTriangle(const delaunator::Delaunator& d,
															const timbre2DPoint& target);

// Compute barycentric coordinates for interpolation weights
std::array<double, 3> computeBarycentricWeights(const timbre2DPoint& p,
												const timbre2DPoint& a,
												const timbre2DPoint& b,
												const timbre2DPoint& c) ;

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

}	// namespace nvs::timbrespace
