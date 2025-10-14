/*
  ==============================================================================

    TimbreAnalysis.h
    Created: 30 Oct 2023 2:07:11pm
    Author:  Nicholas Solem

  ==============================================================================
*/

#pragma once

#include "Analysis/AnalysisUsing.h"
#include "Analysis/Settings.h"
#include <span>

#ifdef INCLUDE_AUBIO
#include "../aubio/src/aubio.h"
#endif

namespace nvs::analysis {

struct PitchesAndConfidences {
    std::vector<Real> pitches, confidences;
};

PitchesAndConfidences calculatePitchesAndConfidences(vecReal waveEvent, streamingFactory const &factory, AnalyzerSettings const& settings);

vecReal calculateLoudnesses(std::span<Real const> waveSpan, streamingFactory const &factory, AnalyzerSettings const& settings);

vecVecReal
calculateBFCCs(std::span<Real const> waveSpan, streamingFactory const &factory, AnalyzerSettings const& settings);

vecVecReal PCA(vecVecReal const &V, standardFactory const &factory, int num_features_out);

std::pair<Real, Real> calculateRangeOfDimension(vecVecReal const &V, size_t dim);
Real calculateNormalizationMultiplier(const std::pair<Real, Real> &range);

} // namespace nvs::analysis
