//
// Created by Nicholas Solem on 10/12/25.
//

#pragma once
#include <vector>

namespace nvs::analysis {

void filterOnsets(std::vector<float> &onsetsInSeconds, const double lengthInSeconds, float minimumOnsetDeltaSeconds = 0.02f);

void forceMinimumOnsets(std::vector<float> &onsets, int minOnsets, double lengthInSeconds);

void equalizeOnsetDensity(std::vector<float> &onsets, double lengthInSeconds);

void normalizeOnsets(std::vector<float> &onsetsInSeconds, const double lengthInSeconds);

void denormalizeOnsets(std::vector<float> &normalizedOnsets, const double lengthInSeconds);

}

