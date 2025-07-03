/*
  ==============================================================================

    TimbreSpaceHolder.cpp
    Created: 2 Jul 2025 2:02:13pm
    Author:  Nicholas Solem

  ==============================================================================
*/

#include "TimbreSpaceHolder.h"
#include "../../slicer_granular/nvs_libraries/nvs_libraries/include/nvs_memoryless.h"
#include <random>
#include <ranges>
#include "fmt/core.h"

namespace nvs::timbrespace {

//===============================================timbre5DPoint=================================================
bool Timbre5DPoint::operator==(Timbre5DPoint const &other) const {
	if (other.get2D() != _p2D){
		return false;
	}
	auto const other3 = other.get3D();
	for (size_t i = 0; i < _p3D.size(); ++i){
		if (_p3D[i] != other3[i]){
			return false;
		}
	}
	return true;
}

constexpr bool inRange0_1(float x){
	return ( (x >= 0.f) && (x <= 1.f) );
}
constexpr bool inRangeM1_1(float x){
	return ( (x >= -1.f) && (x <= 1.f) );
}

std::array<juce::uint8, 3> Timbre5DPoint::toUnsigned() const {
	for (auto p : _p3D){
		assert(inRangeM1_1(p));
	}
	using namespace nvs::memoryless;
	std::array<juce::uint8, 3> u {
		static_cast<juce::uint8>(biuni(_p3D[0]) * 255.f),
		static_cast<juce::uint8>(biuni(_p3D[1]) * 255.f),
		static_cast<juce::uint8>(biuni(_p3D[2]) * 255.f)
	};
	return u;
}

//===============================================TimbreSpaceHolder=================================================

void TimbreSpaceHolder::add5DPoint(timbre2DPoint p2D, std::array<float, 3> p3D){
	using namespace nvs::util;
	assert(inRangeM1_1(p2D.getX()) && inRangeM1_1(p2D.getY())
		   && inRangeM1_1(p3D[0]) && inRangeM1_1(p3D[1]) && inRangeM1_1(p3D[2])
		   );
	timbres5D.add({
		._p2D{p2D},
		._p3D{p3D}
	});
}

void TimbreSpaceHolder::clear() {
	timbres5D.clear();
}

void TimbreSpaceHolder::setCurrentPointIndices(std::vector<util::WeightedIdx> &&newWeightedIndices) {
	jassert (newWeightedIndices.size() > 0);
	
	for (auto const &widx : newWeightedIndices){
		jassert (0 <= widx.idx);
		jassert ((widx.idx < timbres5D.size()) or ((timbres5D.size()==0) and (widx.idx == 0)));
	}
	currentPointIndices = newWeightedIndices;
}

std::vector<util::WeightedIdx> findWeightedPoints(
	const Timbre5DPoint&               target,
	const juce::Array<Timbre5DPoint>&  database,
	int                                K,
	int                                numToPick,
	double                             sharpness,
	float          					   higher3Dweight = 0.001f);
std::vector<util::WeightedIdx> toWeightedIndices(std::vector<util::DistanceIdx> const &dv, double sharpness, double contrastPower = 2.0);

void TimbreSpaceHolder::setProbabilisticPointFromTarget(const Timbre5DPoint& target, int K_neighbors, double sharpness, float higher3Dweight){
	if (timbres5D.size() == 0){
		return;
	}
	setCurrentPointIndices(findWeightedPoints(target, timbres5D, K_neighbors, 3, sharpness, higher3Dweight));
}

void reshapeTimbreSpacePoints(TimbreSpaceNeededData const &neededData, TimbreSpaceHolder &holder, bool verbose)
{	/** to be called when we only want to change the view of the timbre points (which will also need to happen when the timbre space itself changes) */
	
	if (neededData.eventwiseExtractedTimbrePoints.size() == 0){
//		writeToLog("drawTimbreSpacePoints: timbreSpaceNeededData empty, returning...");
		return;
	}
	std::vector<std::vector<float>> const &timbreSpaceRepr = neededData.eventwiseExtractedTimbrePoints;
	std::vector<std::pair<float, float>> const &ranges = neededData.ranges;
	if (!(timbreSpaceRepr.size() == ranges.size())){
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
	holder.clear(); // clearing to make way for points we're about to be adding
	for (size_t i = 0; i < timbreSpaceRepr.size(); ++i) {
		std::vector<float> const &timbreFrame = timbreSpaceRepr[i];
		
		jassert (timbreFrame.size() >= nDim);

		// ========================================2D========================================
		// squash normalized points within dimension range
		auto pNL = juce::Point<float>(foo(timbreFrame[0], neededData.ranges[0]),
									   foo(timbreFrame[1], neededData.ranges[1]));
		// histogram equalization
		float const &equalizedX  = neededData.histoEqualizedD0[i];
		float const &equalizedY  = neededData.histoEqualizedD1[i];
		
		auto pHE = juce::Point<float>( juce::jmap(equalizedX, -1.f, 1.f),
								juce::jmap(equalizedY, -1.f, 1.f));
	
		float c = neededData.spacialSettings.histogramEqualization;
		jassert (0.0 <= c && c <= 1.0);
		juce::Point<float> p = (1.f - c) * pNL + c * pHE;
		
		// ========================================3D========================================
		std::array<float, 3> const color {
			( normalizer(timbreFrame[2], neededData.ranges[2]) ),
			( normalizer(timbreFrame[3], neededData.ranges[3]) ),
			( normalizer(timbreFrame[4], neededData.ranges[4]) )
		};
		// with this method, there is the gaurantee that
		// the Nth member of timbreSpaceComponent.timbres5D corresponds to
		// the Nth member of onsets.
		float const padding_scalar = 0.95f;
		holder.add5DPoint(p * padding_scalar, color);
		if (verbose){
			fmt::print("adding the point {:.3f}, {:.3f}\n", p.x, p.y);
		}
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
std::vector<util::WeightedIdx> findWeightedPoints(
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



}	// namespace nvs::timbrespace
