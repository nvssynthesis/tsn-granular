//
// Created by Nicholas Solem on 1/12/26.
//

#pragma once
#include "TimbreSpace.h"

namespace nvs::timbrespace {

class TimbreSpacePointSelector  :   public juce::ActionListener
{
public:
    ~TimbreSpacePointSelector();
    void setTimbreSpace(TimbreSpace &timbreSpace);

    void update(); // needs to be called whenever referenced timbre points change or when the global point filtering settings change
    void computeExistingPointsFromTarget(const Timbre5DPoint &target);

    Timbre5DPoint const &getTargetPoint() const { return _target; }
    std::vector<Timbre5DPoint> const &getTimbreSpacePoints() const;

    const std::vector<util::WeightedIdx> &getCurrentPointIndices() const { return _currentPointIndices; }
private:
    TimbreSpace *_timbreSpace { nullptr };

    Timbre5DPoint _target {};
    std::vector<util::WeightedIdx> _currentPointIndices {{},{},{}};

    void computeDelaunay();
    void swapIfPending();

    std::unique_ptr<delaunator::Delaunator> _delaunator;
    std::unique_ptr<delaunator::Delaunator> _delaunator_pending;
    std::atomic<bool> _pendingUpdate { false };

    void actionListenerCallback(const String &message) override;
};


//=============================================================================================================================

std::vector<util::WeightedIdx> findPointsDistanceBased (const Timbre5DPoint& target,
                                                        const juce::Array<Timbre5DPoint>&  database,
                                                        int K,
                                                        int numToPick,
                                                        double sharpness,
                                                        float higher3Dweight);

std::vector<util::WeightedIdx> findPointsTriangulationBased(const Timbre5DPoint& target,
                                                            const std::vector<Timbre5DPoint>& database,
                                                            const delaunator::Delaunator &d);

std::vector<util::WeightedIdx> findNearestTrianglePoints(const Timbre5DPoint& target,
                                                         const std::vector<Timbre5DPoint>& database,
                                                         const delaunator::Delaunator& d);

std::vector<util::WeightedIdx> toWeightedIndices(std::vector<util::DistanceIdx> const &dv, double sharpness, double contrastPower = 2.0);

}   // namespace nvs::timbrespace