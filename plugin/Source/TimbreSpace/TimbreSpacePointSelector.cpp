//
// Created by Nicholas Solem on 1/12/26.
//

#include <ranges>
#include "TimbreSpacePointSelector.h"
#include "TimbreSpaceTriangulation.h"
#include "dsp_util.h"
#include "StringAxiom.h"

namespace nvs::timbrespace {

TimbreSpacePointSelector::TimbreSpacePointSelector(juce::AudioProcessorValueTreeState &apvts, TimbreSpace &timbreSpace)
:   _apvts(apvts)
,   _timbreSpace(timbreSpace)
{
    _apvts.state.addListener(this);
    _timbreSpace.addActionListener(this);

}
TimbreSpacePointSelector::~TimbreSpacePointSelector() {
    _apvts.state.removeListener(this);
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

    {
        auto snapshot = std::make_shared<TriangulationSnapshot>();

    // rebuildActivePoints
        auto &activeIndices = snapshot->_activeIndices;
        for (size_t i = 0; i < _wrappedPoints.size(); ++i) {
            if (_wrappedPoints[i].active) {
                activeIndices.push_back(i);
            }
        }
        auto &activePoints = snapshot->_activePoints;
        // assumes we already have wrappedPoints and _activePointsPending built
        for (const size_t idx : activeIndices) {
            activePoints.push_back(_wrappedPoints[idx].point);
        }

    // computeDelaunay
        if (activePoints.empty()) {
            DBG("TimbreSpacePointSelector::triangulatePoints timbres5D empty; returning\n");
            return;
        }
        const auto coords2D = make2dCoordinates(activePoints);
        try {
            snapshot->_delaunator = std::make_unique<delaunator::Delaunator>(coords2D);   // IF THIS FAILS, _pendingUpdate does not store `true`
        }
        catch (std::exception &e) {
            DBG(e.what());
            return;
        }
        catch (...) {
            return;
        }
        std::atomic_store_explicit(&_triangulationSnapshotPending, snapshot, std::memory_order_release); // memory_order_release indicating: done writing, publish it
    }
}
void TimbreSpacePointSelector::computeExistingPointsFromTarget(const Timbre5DPoint &target) {
    _target = target;

    swapIfPending();
    auto currentSnapshot = _triangulationSnapshotCurrent;
    if (currentSnapshot == nullptr) {
        return;
    }
    if (currentSnapshot->_delaunator == nullptr) {
        return;
    }
    std::vector<WeightedIdx> weightedIndices =
        findPointsTriangulationBased(
            _target,
            currentSnapshot->_activePoints,
            *currentSnapshot->_delaunator,
            &_lastTriangleIndex
        );
    jassert(weightedIndices.size() == 3);

    for (auto const &widx : weightedIndices) {
        jassert(0 <= widx.idx);
        jassert((widx.idx < currentSnapshot->_activePoints.size()));
    }

    for (size_t i = 0; i < weightedIndices.size(); ++i) {
        jassert(weightedIndices[i].idx < currentSnapshot->_activeIndices.size());

        weightedIndices[i].idx = currentSnapshot->_activeIndices[weightedIndices[i].idx];
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
void TimbreSpacePointSelector::swapIfPending() {
    auto pending = std::atomic_exchange_explicit(&_triangulationSnapshotPending,    // get value
        std::shared_ptr<TriangulationSnapshot>(),                                 // AND clear pending slot (default ctor is nullptr)
        std::memory_order_acq_rel);                                     // acq_rel: "acquire the new data" + "release the nullptr write"
    if (pending)
    {
        DBG("TimbreSpacePointSelector::swapIfPending exchanging delaunator and active points\n");
        _triangulationSnapshotCurrent = pending;
    }
}


//=============================================================================================================================



}   // namespace nvs::timbrespace