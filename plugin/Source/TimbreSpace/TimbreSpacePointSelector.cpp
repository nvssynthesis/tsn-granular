//
// Created by Nicholas Solem on 1/12/26.
//

#include "TimbreSpacePointSelector.h"
#include "TimbreSpaceTriangulation.h"
#include <ranges>

#include "dsp_util.h"

namespace nvs::timbrespace {

TimbreSpacePointSelector::TimbreSpacePointSelector(juce::AudioProcessorValueTreeState &apvts, TimbreSpace &timbreSpace)
:   _apvts(apvts)
,   _timbreSpace(timbreSpace)
{
    _apvts.state.addListener(this);
    _timbreSpace.addActionListener(this);

}
TimbreSpacePointSelector::~TimbreSpacePointSelector() {
    _timbreSpace.removeActionListener(this);
}

void TimbreSpacePointSelector::valueTreePropertyChanged (ValueTree &alteredTree, const juce::Identifier & property) {
    if (alteredTree.hasType(nvs::axiom::PARAM) && property == juce::Identifier("value")) {
        const auto paramID = alteredTree["id"].toString();
        const float newValue = alteredTree["value"];

        if (paramID == nvs::axiom::filtered_feature) {
            updateRanksForFilteredFeature();
            updateGlobalFilter();
            return;
        }
        if ((paramID == nvs::axiom::filtered_feature_min) || (paramID == nvs::axiom::filtered_feature_max)) {
            // ideally we should only update these when their slider is RELEASED, not as it drags!
            updateGlobalFilter();
            return;
        }
    }
}

void TimbreSpacePointSelector::actionListenerCallback(const String &message) {
    if (message == axiom::timbreSpaceTreeChanged) {
        _featurewiseRankIndices.clear();    // first time around for new dataset, we must clear its state
        updateRanksForFilteredFeature();    // and then fill it in for the currently selected feature
        reserveWrappedPoints();
    }
    if (message == axiom::shapedPointsAvailable) {
        updateGlobalFilter();
    }
}

void TimbreSpacePointSelector::reserveWrappedPoints() {
    // reserves based on rank for filtered feature having been already set
    _wrappedPoints.clear();
    _wrappedPoints.reserve(_featurewiseRankIndices[_filteredFeature].size());
}
void TimbreSpacePointSelector::rebuildActivePoints() {
    // assumes we already have wrappedPoints and activeIndices built
    _activePoints.clear();
    for (const size_t idx : _activeIndices) {
        _activePoints.push_back(_wrappedPoints[idx].point);
    }
}
void TimbreSpacePointSelector::updateGlobalFilter() {
    const auto &rawPoints = _timbreSpace.getTimbreSpacePoints();

    const float minFrac = _apvts.getRawParameterValue(nvs::axiom::filtered_feature_min)->load();
    const float maxFrac = _apvts.getRawParameterValue(nvs::axiom::filtered_feature_max)->load();

    typedef signed long long SLL;

    SLL minRank = rawPoints.size() * minFrac;
    SLL maxRank = rawPoints.size() * maxFrac;
    if (maxRank - minRank < 3) {
        maxRank = std::min(minRank + 3, static_cast<SLL>(rawPoints.size()-1));
        if (maxRank - minRank < 3) {    // then maxRank got clipped by rawPoints.size
            jassert(maxRank == rawPoints.size() - 1);
            minRank = std::max(maxRank - 3, static_cast<SLL>(0));
        }
        jassert (maxRank - minRank >= 3);
    }

    _wrappedPoints.clear();
    _wrappedPoints.reserve(rawPoints.size());
    for (size_t i = 0; i < rawPoints.size(); ++i) {
        const auto rawPoint = rawPoints[i];
        const size_t rank = _featurewiseRankIndices[_filteredFeature][i];
        bool active = (rank >= minRank && rank < maxRank);
        _wrappedPoints.push_back({rawPoint, active});
    }

    _activeIndices.clear();
    for (size_t i = 0; i < _wrappedPoints.size(); ++i) {
        if (_wrappedPoints[i].active) {
            _activeIndices.push_back(i);
        }
    }
    rebuildActivePoints();
    computeDelaunay();
}
void TimbreSpacePointSelector::computeExistingPointsFromTarget(const Timbre5DPoint &target) {
    _target = target;

    swapIfPending();
    if (_delaunator == nullptr) {
        return;
    }

    std::vector<util::WeightedIdx> weightedIndices =
        findPointsTriangulationBased(
            _target,
            _activePoints,
            *_delaunator
        );
    jassert(weightedIndices.size() == 3);

    for (auto const &widx : weightedIndices) {
        jassert(0 <= widx.idx);
        jassert((widx.idx < _activePoints.size()));
    }

    for (size_t i = 0; i < weightedIndices.size(); ++i) {
        jassert(weightedIndices[i].idx < _activeIndices.size());

        weightedIndices[i].idx = _activeIndices[weightedIndices[i].idx];
        _currentPointIndices[i] = weightedIndices[i];
    }
}
std::vector<WrappedPoint5D> const &TimbreSpacePointSelector::getTimbreSpacePoints() const {
    return _wrappedPoints;
}

std::vector<size_t> computeRanks(const std::vector<float>& featureValues) {
    std::vector<size_t> indices(featureValues.size());
    std::iota(indices.begin(), indices.end(), 0);

    std::ranges::sort(indices,
                      [&featureValues](const size_t a, const size_t b) { return featureValues[a] < featureValues[b]; });

    // order inversion
    std::vector<size_t> ranks(featureValues.size());
    for (size_t rank = 0; rank < indices.size(); ++rank) {
        ranks[indices[rank]] = rank;
    }

    return ranks;
}
void TimbreSpacePointSelector::updateRanksForFilteredFeature() {
    _filteredFeature = static_cast<nvs::analysis::Feature_e>(_apvts.getRawParameterValue(axiom::filtered_feature)->load());
    _filteredFeatureTargetRangeNormalized.first = _apvts.getRawParameterValue(axiom::filtered_feature_min)->load();
    _filteredFeatureTargetRangeNormalized.second = _apvts.getRawParameterValue(axiom::filtered_feature_max)->load();

    const auto vals = _timbreSpace.getRawFeatureValues(_filteredFeature);

    _featurewiseRankIndices[_filteredFeature] = computeRanks(vals);
}


void TimbreSpacePointSelector::computeDelaunay() {
    DBG("TimbreSpacePointSelector::triangulatePoints computing _delaunator & storing _pendingUpdate flag\n");
    if (_activePoints.empty()) {
        DBG("TimbreSpacePointSelector::triangulatePoints timbres5D empty; returning\n");
        return;
    }
    const auto coords2D = make2dCoordinates(_activePoints);
    try {
        _delaunator_pending = std::make_unique<delaunator::Delaunator>(coords2D);   // IF THIS FAILS, _pendingUpdate does not store `true`
    }
    catch (std::exception &e) {
        DBG(e.what());
        return;
    }
    catch (...) {
        return;
    }
    _pendingUpdate.store(true, std::memory_order_release);
}
void TimbreSpacePointSelector::swapIfPending() {
    if (_pendingUpdate.exchange(false, std::memory_order_acq_rel))
    {
        DBG("TimbreSpacePointSelector::swapIfPending exchanging delaunator\n");
        _delaunator = std::move(_delaunator_pending);
    }
}


//=============================================================================================================================


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
std::vector<util::WeightedIdx> findPointsDistanceBased(
    const Timbre5DPoint&               target,
    const juce::Array<Timbre5DPoint>&  database,
    int                                K,
    const int                          numToPick,
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
       if (std::ranges::all_of(weightArr, [](const double w) { return w == 0.0; })) {
          break;
       }
    }
    return picked;
}
std::vector<util::WeightedIdx> toWeightedIndices(
    const std::vector<util::DistanceIdx> &dv,
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
	std::vector<util::WeightedIdx> result;
	result.reserve(dv.size());

	for (size_t i = 0; i < dv.size(); ++i) {
		result.emplace_back(dv[i].idx, weights[i]);
	}

	return result;
}

// triangulation-based point selection function
std::vector<util::WeightedIdx> findPointsTriangulationBased(const Timbre5DPoint& target,
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
	std::vector<util::WeightedIdx> result;
	result.reserve(3);
	result.emplace_back(idx0, weights[0]);
	result.emplace_back(idx1, weights[1]);
	result.emplace_back(idx2, weights[2]);

    jassert (result.size() == 3);
	return result;
}

// Helper function for when target is outside convex hull
std::vector<util::WeightedIdx> findNearestTrianglePoints(const Timbre5DPoint& target,
														 const std::vector<Timbre5DPoint>& database,
														 const delaunator::Delaunator& d)
{
	// Find the nearest edge or vertex of the convex hull
	// This is a simplified approach - you might want to be more sophisticated

	Timbre2DPoint targetPoint = get2D(target);
	double minDistance = std::numeric_limits<double>::max();
	std::array<size_t, 3> bestTriangle {0, 1, 2};

	// Check all triangles and find the one with minimum distance to target
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
}   // namespace nvs::timbrespace