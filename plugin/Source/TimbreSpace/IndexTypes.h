//
// Created by Nicholas Solem on 1/16/26.
//

#pragma once

namespace nvs::timbrespace {

struct WeightedIdx
{
    int idx	{0};
    double weight{0.0}; // normalized probability (sums to 1 over all returned)

    WeightedIdx(int i, double w) : idx(i), weight(w) {}
    WeightedIdx() = default;
};
struct DistanceIdx	// effectively same class as weightedIdx but reminds you that we speak of distance, not weight
{
    int idx	{0};
    double distance{0.0}; // normalized probability (sums to 1 over all returned)

    DistanceIdx(int i, double d) : idx(i), distance(d) {}
    DistanceIdx() = default;
};

} // namespace nvs::timbrespace