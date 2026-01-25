//
// Created by Nicholas Solem on 1/12/26.
//

#pragma once
#include "TimbreSpace.h"
#include "../../slicer_granular/Source/IndexTypes.h"

namespace nvs::timbrespace {

typedef WrappedPoint<5> WrappedPoint5D;

class TimbreSpacePointSelector final :   public juce::ActionListener
,                                        public juce::ValueTree::Listener
{
public:
    explicit TimbreSpacePointSelector (juce::AudioProcessorValueTreeState& apvts, TimbreSpace &timbreSpace);
    ~TimbreSpacePointSelector() override;

    std::vector<WrappedPoint5D> const &getTimbreSpacePoints() const;

    void computeExistingPointsFromTarget(const Timbre5DPoint &target);

    Timbre5DPoint const &getTargetPoint() const { return _target; }
    const std::vector<WeightedIdx> &getCurrentPointIndices() const { return _currentPointIndices; }
    //========================================================================================================
    void valueTreePropertyChanged (ValueTree &alteredTree, const juce::Identifier & property) override;

private:
    juce::AudioProcessorValueTreeState &_apvts;
    TimbreSpace &_timbreSpace;

    Timbre5DPoint _target {};
    size_t _lastTriangleIndex {0};
    std::vector<WeightedIdx> _currentPointIndices {{},{},{}};

    std::vector<WrappedPoint5D> _wrappedPoints {};
    std::vector<size_t> _activeIndices {};
    std::vector<size_t> _activeIndicesPending {};
    std::vector<Timbre5DPoint> _activePoints {}; // Reusable buffer THIS needs to ALSO BE SYNCHRONIZED with atomic pendingUpdate
    std::vector<Timbre5DPoint> _activePointsPending {};

    void reserveWrappedPoints();

    void rebuildActivePoints();

    nvs::analysis::Feature_e _filteredFeature {nvs::analysis::Feature_e::SpectralFlatness};
    void updateRanksForFilteredFeature();
    typedef std::pair<float, float> Range;
    Range _filteredFeatureTargetRangeNormalized {0.f, 1.f};
    std::map<analysis::Feature_e, std::vector<size_t>> _featurewiseRankIndices {};
    void updateGlobalFilter();

    void computeDelaunay();

    void swapIfPending();

    std::unique_ptr<delaunator::Delaunator> _delaunator;
    std::unique_ptr<delaunator::Delaunator> _delaunatorPending;
    std::atomic<bool> _pendingUpdate { false };

    void actionListenerCallback(const String &message) override;
};

}   // namespace nvs::timbrespace