/*
  ==============================================================================

    TimbreSpace.cpp
    Created: 2 Jul 2025 2:02:13pm
    Author:  Nicholas Solem

  ==============================================================================
*/

#include "TimbreSpace.h"
#include "../../slicer_granular/nvs_libraries/nvs_libraries/include/nvs_memoryless.h"
#include "../Analysis/ThreadedAnalyzer.h"
#include "TimbreSpaceTriangulation.h"
#include <random>
#include <ranges>
#include "fmt/core.h"

namespace nvs::timbrespace {


//===============================================TimbreSpace=================================================

TimbreSpace::TimbreSpace() = default;
TimbreSpace::~TimbreSpace() = default;

void TimbreSpace::add5DPoint(timbre2DPoint p2D, std::array<float, 3> p3D){
	using namespace nvs::util;
	assert(inRangeM1_1(p2D.getX()) && inRangeM1_1(p2D.getY())
		   && inRangeM1_1(p3D[0]) && inRangeM1_1(p3D[1]) && inRangeM1_1(p3D[2])
		   );
	timbres5D.add({
		._p2D{p2D},
		._p3D{p3D}
	});
}

void TimbreSpace::clear() {
	timbres5D.clear();
}


std::vector<util::WeightedIdx> toWeightedIndices(std::vector<util::DistanceIdx> const &dv, double sharpness, double contrastPower = 2.0);

// Modified main function with method parameter
void TimbreSpace::setProbabilisticPointFromTarget(const Timbre5DPoint& target,
												  int K_neighbors,
												  double sharpness,
												  float higher3Dweight,
												  PointSelectionMethod method)
{
	if (timbres5D.size() == 0) { return; }
	
	std::vector<util::WeightedIdx> const weightedIndices = [&target, K_neighbors, sharpness, higher3Dweight, method, this]()
	{
		switch (method) {
			case PointSelectionMethod::TRIANGULATION_BASED:
				return findPointsTriangulationBased(target, timbres5D, *_delaunator.get());
				break;
			case PointSelectionMethod::DISTANCE_BASED:
				return findPointsDistanceBased(target, timbres5D, K_neighbors, 3, sharpness, higher3Dweight);
				break;
		}
	}();
	jassert(weightedIndices.size() > 0);
	
	for (auto const &widx : weightedIndices) {
		jassert(0 <= widx.idx);
		jassert((widx.idx < timbres5D.size()) || ((timbres5D.size() == 0) && (widx.idx == 0)));
	}
	
	currentPointIndices = weightedIndices;
}

void TimbreSpace::reshape(bool verbose)
{	/** to be called when we only want to change the view of the timbre points (which will also need to happen when the timbre space itself changes) */
	
	if (eventwiseExtractedTimbrePoints.size() == 0){
//		writeToLog("drawTimbreSpacePoints: timbreSpaceNeededData empty, returning...");
		return;
	}
	std::vector<std::vector<float>> const &timbreSpaceRepr = eventwiseExtractedTimbrePoints;
	if (!(timbreSpaceRepr[0].size() == _ranges.size())){
//		writeToLog("drawTimbreSpacePoints: point size mismatch, exiting early");
		jassertfalse;
		return;
	}
	
	auto normalizer = [](float x, std::pair<float, float> range) -> float
	{
		auto r = (range.second - range.first);
		auto y01 = (x - range.first);
		if (r != 0){
			y01 /= r;
		}
		return juce::jmap(y01, -1.f, 1.f);
	};
	auto squash = [](float xNorm) -> float { return std::asinh(10.0*xNorm) / (float)(M_PI); };
	auto foo = [&](float x, std::pair<float, float> range) -> float { return squash(normalizer(x, range)); };
	
	constexpr size_t nDim {5};
	
	// clear points of timbreSpaceHolds
	clear(); // clearing to make way for points we're about to be adding
	for (size_t i = 0; i < timbreSpaceRepr.size(); ++i) {
		std::vector<float> const &timbreFrame = timbreSpaceRepr[i];
		
		jassert (timbreFrame.size() >= nDim);

		// ========================================2D========================================
		// squash normalized points within dimension range
		auto pNL = juce::Point<float>(foo(timbreFrame[0], _ranges[0]),
									   foo(timbreFrame[1], _ranges[1]));
		// histogram equalization
		float const &equalizedX  = histoEqualizedD0[i];
		float const &equalizedY  = histoEqualizedD1[i];
		
		auto pHE = juce::Point<float>( juce::jmap(equalizedX, -1.f, 1.f),
								juce::jmap(equalizedY, -1.f, 1.f));
	
		float c = settings.histogramEqualization;
		jassert (0.0 <= c && c <= 1.0);
		juce::Point<float> p = (1.f - c) * pNL + c * pHE;
		
		// ========================================3D========================================
		std::array<float, 3> const color {
			( normalizer(timbreFrame[2], _ranges[2]) ),
			( normalizer(timbreFrame[3], _ranges[3]) ),
			( normalizer(timbreFrame[4], _ranges[4]) )
		};
		// with this method, there is the gaurantee that
		// the Nth member of timbreSpaceComponent.timbres5D corresponds to
		// the Nth member of onsets.
		float const padding_scalar = 0.95f;
		add5DPoint(p * padding_scalar, color);
		if (verbose){
			fmt::print("adding the point {:.3f}, {:.3f}\n", p.x, p.y);
		}
	}
}

void TimbreSpace::valueTreePropertyChanged (juce::ValueTree &alteredTree, const juce::Identifier &) {
	std::cout << "value tree changed!!!!!\n";
	if (alteredTree.hasType("TimbreSpace")){
		std::cout << "tree changed! redrawing points...\n";
		settings.histogramEqualization = (double)alteredTree.getProperty("HistogramEqualization");
		auto xs = (juce::String)alteredTree.getProperty("xAxis");
		auto ys = ((juce::String)alteredTree.getProperty("yAxis"));
		std::cout << "x: " << xs << " y: " << ys << '\n';
		settings.dimensionWisefeatures[0] = nvs::analysis::toFeature(xs);
		settings.dimensionWisefeatures[1] = nvs::analysis::toFeature(ys);
		extract();
		updateTimbreSpacePoints();
		reshape();
	   _delaunator = std::make_unique<delaunator::Delaunator>(make2dCoordinates(timbres5D));
	}
	else {
		std::cout << "what tree?\n";
	}
}
void TimbreSpace::changeListenerCallback(juce::ChangeBroadcaster* source) {
	if (auto *a = dynamic_cast<nvs::analysis::ThreadedAnalyzer*>(source)){
		auto onsetsOpt = a->getOnsets();
		if (onsetsOpt.has_value() and onsetsOpt->size()) {
			fullTimbreSpace = a->stealTimbreSpaceRepresentation();
			extract();
			updateTimbreSpacePoints();
			reshape();
			_delaunator = std::make_unique<delaunator::Delaunator>(make2dCoordinates(timbres5D));
		}
	}
}
void TimbreSpace::extract() {
	auto const &featuresToExtract = settings.dimensionWisefeatures;
	if (!fullTimbreSpace.has_value()){
		std::cerr << "TsnGranularAudioProcessorEditor::TimbreSpaceNeededData::extract: timbre space empty, early exit\n";
		return;
	}
	eventwiseExtractedTimbrePoints.clear();
	eventwiseExtractedTimbrePoints.reserve(fullTimbreSpace->size());
	
	for (auto const &t : fullTimbreSpace.value()) {
		std::vector<float> v = nvs::analysis::extractFeatures(t, featuresToExtract, nvs::analysis::Statistic::Median);
		eventwiseExtractedTimbrePoints.push_back(v);
	}
}

namespace {
std::vector<float> getHistoEqualizationVec(std::vector<float> const &points){
	std::vector<float> allX, vecOut;
	allX.reserve (points.size());
	vecOut.reserve (points.size());
	for (auto& p : points){
		allX.push_back (p);
	}
	std::sort (allX.begin(), allX.end());
	
	for (auto const& p : points)
	{
		// find first index ≥ p.x
		auto idx = (int)(std::lower_bound (allX.begin(),
										   allX.end(),
										   p)
						 - allX.begin());
		float quantile = (float) idx / (float)(allX.size() - 1);
		
		// optional: you can still gamma‑tweak the quantile
		float gamma    = 0.8f;             // <1 stretches mid‑values
		float t        = std::pow (quantile, gamma);
		vecOut.push_back(t);
	}
	return vecOut;
}
}	// end anonymous namespace

void TimbreSpace::updateTimbreSpacePoints()
{	/** to be called when the actual analyzed timbre space changes  */
	if (eventwiseExtractedTimbrePoints.size() == 0){
		std::cout << "updateAndDrawTimbreSpacePoints: timbreSpace empty, returning...\n";
		return;
	}
	{
		auto const n_dim = eventwiseExtractedTimbrePoints[0].size();
		_ranges.clear();
		_ranges.reserve(n_dim);
		
		for (size_t i = 0; i < n_dim; ++i){
			_ranges.push_back(nvs::analysis::calculateRangeOfDimension(eventwiseExtractedTimbrePoints, i));
		}
	}
	{
		std::vector<float> allDim0, allDim1;
		allDim0.reserve(eventwiseExtractedTimbrePoints.size());
		allDim1.reserve(eventwiseExtractedTimbrePoints.size());
		for (auto const& frame : eventwiseExtractedTimbrePoints){
			allDim0.push_back(frame[0]);	// e.g. bfcc1
			allDim1.push_back(frame[1]);	// e.g. bfcc2
		}
		histoEqualizedD0 = getHistoEqualizationVec(allDim0);
		histoEqualizedD1 = getHistoEqualizationVec(allDim1);
	}
}
/**
 * Finds the K nearest points to `target` in `database`,
 * builds softmax-style weights over those K,
 * then either returns them all, or samples `numToPick` **without replacement**.
 *
 * @param  database        your full set of Timbre5DPoints
 * @param  K               how many nearest neighbors to consider (caps at database.size())
 * @param  numToPick       if <=0, we return all K weighted indices;
 *                         otherwise we pick exactly numToPick **without** replacement.
 * @param  sharpness       0=flat, >0 biases toward closer points
 * @param  higher3Dweight  extra weighting on the 3D portion of your distance metric
 */
std::vector<util::WeightedIdx> findPointsDistanceBased(
	const Timbre5DPoint&               target,
	const juce::Array<Timbre5DPoint>&  database,
	int                                K,
	int                                numToPick,
	double                             sharpness,
	float                              higher3Dweight)
{
	if (database.isEmpty()) { return {}; }
	
	std::vector<util::WeightedIdx> weightedIndices = [&database, target, higher3Dweight, &K, sharpness](){
		// compute squared distances
		std::vector<util::DistanceIdx> distIdx;
		distIdx.reserve (database.size());
		for (int i = 0; i < database.size(); ++i) {
			auto const& p = database.getReference(i);
			double dx = p.get2D().getX() - target.get2D().getX();
			double dy = p.get2D().getY() - target.get2D().getY();
			auto   u1 = p.get3D();
			auto   u2 = target.get3D();
			double du = u1[0] - u2[0],
				   dv = u1[1] - u2[1],
				   dz = u1[2] - u2[2];

			double d2 = dx*dx + dy*dy
					  + higher3Dweight * (du*du + dv*dv + dz*dz);
			distIdx.emplace_back (i, d2);
		}

		// keep only the K nearest
		{
			K = std::min<int> (K, (int) distIdx.size());
			std::nth_element (
				distIdx.begin(),
				distIdx.begin() + K,
				distIdx.end(),
				[](auto& a, auto& b){ return a.distance < b.distance; });
			distIdx.resize (K);
			assert(distIdx.size() == (size_t)K);
		}
		return toWeightedIndices(distIdx, sharpness);
	}();

	/** if caller just wants the full distribution, return it: */
	if (numToPick <= 0 || numToPick >= K) { return weightedIndices; }
	
	/** otherwise sample numToPick *without replacement* by repeatedly drawing
	 from the discrete distribution then zeroing out that weight and re-normalizing. */
	std::vector<util::WeightedIdx> picked;
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
		if (std::all_of(weightArr.begin(), weightArr.end(), [](double w) { return w == 0.0; })) {
			break;
		}
	}
	return picked;
}
std::vector<util::WeightedIdx> toWeightedIndices(std::vector<util::DistanceIdx> const &dv, double sharpness, double contrastPower)
{
	if (dv.empty()) {
		return {};
	}
	
	// 1) Find maximum distance for normalization
	double dmax = 0.0;
	for (const auto& di : dv) {
		dmax = std::max(dmax, di.distance);
	}
	
	// 2) Convert distances to initial weights using softmax
	std::vector<double> weights;
	weights.reserve(dv.size());
	
	if (dmax <= 0.0) {
		// All distances are zero - uniform weights
		double uniformWeight = 1.0 / dv.size();
		weights.assign(dv.size(), uniformWeight);
	} else {
		// Apply softmax: exp(-sharpness * (distance/dmax))
		for (const auto& di : dv) {
			double normalizedDist = di.distance / dmax;
			weights.push_back(std::exp(-sharpness * normalizedDist));
		}
		
		// Normalize to sum = 1
		double sum = std::accumulate(weights.begin(), weights.end(), 0.0);
		for (auto& w : weights) {
			w /= sum;
		}
	}
	
	// 3) Apply contrast power scaling
	if (contrastPower != 1.0) {
		for (auto& w : weights) {
			w = std::pow(w, contrastPower);
		}
		
		// Re-normalize after power scaling
		double sum = std::accumulate(weights.begin(), weights.end(), 0.0);
		if (sum > 0.0) {
			for (auto& w : weights) {
				w /= sum;
			}
		}
	}
	
	// 4) Build result vector with WeightedIdx structs
	std::vector<util::WeightedIdx> result;
	result.reserve(dv.size());
	
	for (size_t i = 0; i < dv.size(); ++i) {
		result.emplace_back(dv[i].idx, weights[i]);
	}
	
	return result;
}

// New triangulation-based point selection function
std::vector<util::WeightedIdx> findPointsTriangulationBased(const Timbre5DPoint& target, const juce::Array<Timbre5DPoint>& database, const delaunator::Delaunator &d)
{
	if (database.isEmpty()) { return {}; }
	
	// Need at least 3 points for triangulation
	if (database.size() < 3) {
		// Fall back to distance-based method or return all points with equal weights
		std::vector<util::WeightedIdx> result;
		double weight = 1.0 / database.size();
		for (int i = 0; i < database.size(); ++i) {
			result.emplace_back(i, weight);
		}
		return result;
	}
	
	// Find triangle containing the target point
	timbre2DPoint targetPoint = target.get2D();
	auto triangleOpt = findContainingTriangle(d, targetPoint);
	
	if (!triangleOpt.has_value()) {
		// Target point is outside the convex hull
		// Fall back to distance-based method or handle as edge case
		return findNearestTrianglePoints(target, database, d);
	}
	
	// Get the triangle vertices
	auto triangle = triangleOpt.value();
	size_t idx0 = triangle[0];
	size_t idx1 = triangle[1];
	size_t idx2 = triangle[2];
	
	// Get the 2D points for barycentric calculation
	timbre2DPoint p0 = database.getReference(idx0).get2D();
	timbre2DPoint p1 = database.getReference(idx1).get2D();
	timbre2DPoint p2 = database.getReference(idx2).get2D();
	
	// Compute barycentric weights
	auto weights = computeBarycentricWeights(targetPoint, p0, p1, p2);
	
	// Create result vector
	std::vector<util::WeightedIdx> result;
	result.reserve(3);
	result.emplace_back(idx0, weights[0]);
	result.emplace_back(idx1, weights[1]);
	result.emplace_back(idx2, weights[2]);
	
	return result;
}

// Helper function for when target is outside convex hull
std::vector<util::WeightedIdx> findNearestTrianglePoints(const Timbre5DPoint& target,
														 const juce::Array<Timbre5DPoint>& database,
														 const delaunator::Delaunator& d)
{
	// Find the nearest edge or vertex of the convex hull
	// This is a simplified approach - you might want to be more sophisticated
	
	timbre2DPoint targetPoint = target.get2D();
	double minDistance = std::numeric_limits<double>::max();
	std::array<size_t, 3> bestTriangle;
	
	// Check all triangles and find the one with minimum distance to target
	for (size_t i = 0; i < d.triangles.size(); i += 3) {
		size_t idx0 = d.triangles[i];
		size_t idx1 = d.triangles[i + 1];
		size_t idx2 = d.triangles[i + 2];
		
		timbre2DPoint p0 = database.getReference(idx0).get2D();
		timbre2DPoint p1 = database.getReference(idx1).get2D();
		timbre2DPoint p2 = database.getReference(idx2).get2D();
		
		// Calculate centroid of triangle
		timbre2DPoint centroid(
			(p0.getX() + p1.getX() + p2.getX()) / 3.0,
			(p0.getY() + p1.getY() + p2.getY()) / 3.0
		);
		
		double dx = targetPoint.getX() - centroid.getX();
		double dy = targetPoint.getY() - centroid.getY();
		double distance = dx * dx + dy * dy;
		
		if (distance < minDistance) {
			minDistance = distance;
			bestTriangle = {idx0, idx1, idx2};
		}
	}
	
	// Use the closest triangle and project the point onto it
	// For simplicity, we'll use equal weights, but you could do proper projection
	std::vector<util::WeightedIdx> result;
	result.reserve(3);
	result.emplace_back(bestTriangle[0], 1.0/3.0);
	result.emplace_back(bestTriangle[1], 1.0/3.0);
	result.emplace_back(bestTriangle[2], 1.0/3.0);
	
	return result;
}

}	// namespace nvs::timbrespace
