/*
  ==============================================================================

    TimbreSpaceTriangulation.h
    Created: 2 Jul 2025 5:52:14pm
    Author:  Nicholas Solem

  ==============================================================================
*/

#pragma once

#include <random>
#include <ranges>

#include "../../delaunator-cpp/include/delaunator.hpp"
#include "../../slicer_granular/Source/IndexTypes.h"
#include "TimbrePointTypes.h"

namespace nvs::timbrespace {

std::vector<double> make2dCoordinates(const std::vector<Timbre5DPoint> &points);

void getUniqueEdges(const delaunator::Delaunator& d);

// Test if point p is inside triangle formed by vertices a, b, c
bool pointInTriangle(const Timbre2DPoint& p, const Timbre2DPoint& a, const Timbre2DPoint& b, const Timbre2DPoint& c) ;

// Find triangle containing target point using Delaunator results
std::optional<std::array<size_t, 3>> findContainingTriangle(const delaunator::Delaunator& d,
															const Timbre2DPoint& target);

// Compute barycentric coordinates for interpolation weights
std::array<double, 3> computeBarycentricWeights(const Timbre2DPoint& p,
												const Timbre2DPoint& a,
												const Timbre2DPoint& b,
												const Timbre2DPoint& c) ;

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
    const std::vector<Timbre5DPoint>& database, const delaunator::Delaunator &d);

// Helper function for when target is outside convex hull
std::vector<WeightedIdx> findNearestTrianglePoints(const Timbre5DPoint& target,
														 const std::vector<Timbre5DPoint>& database,
														 const delaunator::Delaunator& d);

//=============================================================================================================================

size_t findHalfedge(const delaunator::Delaunator& d, size_t triangleIdx, size_t v1, size_t v2);
size_t getThirdVertex(const delaunator::Delaunator& d, size_t triangleIdx, size_t v1, size_t v2);
size_t getVertexIndex(const delaunator::Delaunator& d, const Timbre2DPoint& point);
Timbre2DPoint getPointFromVertex(const delaunator::Delaunator& d, size_t vertexIdx);

struct TrianglePoints {
    Timbre2DPoint p0, p1, p2;
    size_t t0, t1, t2;
    static TrianglePoints create(const delaunator::Delaunator& d, size_t triangleIdx);
};

//=============================================================================================================================
#if 0
std::vector<WeightedIdx> toWeightedIndices(std::vector<DistanceIdx> const &dv, double sharpness, double contrastPower = 2.0);


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
    float                              higher3Dweight);

#endif

}	// namespace nvs::timbrespace
