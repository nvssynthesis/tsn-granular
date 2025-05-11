/*
  ==============================================================================

    TimbreAnalysis.h
    Created: 30 Oct 2023 2:07:11pm
    Author:  Nicholas Solem

  ==============================================================================
*/

#pragma once

#include "Analysis/EssentiaSetup.h"
#include "Analysis/AnalysisUsing.h"
#include "Analysis/Settings.h"
#include <span>
#include <JuceHeader.h>
#ifdef INCLUDE_AUBIO
#include "../aubio/src/aubio.h"
#endif

namespace nvs {
namespace analysis {

struct PitchesAndConfidences {
	std::vector<Real> pitches, confidences;
};



PitchesAndConfidences calculatePitchesAndConfidences(vecReal waveEvent, streamingFactory const &factory, juce::ValueTree settingsTree);

vecReal calculateLoudnesses(std::span<Real const> wavespan, streamingFactory const &factory, juce::ValueTree settingsTree);

vecVecReal
calculateBFCCs(std::span<Real const> waveSpan, streamingFactory const &factory, juce::ValueTree settingsTree);

vecVecReal PCA(vecVecReal const &V, standardFactory const &factory, int num_features_out);

std::pair<Real, Real> calculateRangeOfDimension(vecVecReal const &V, size_t dim);
Real calculateNormalizationMultiplier(std::pair<Real, Real> range);

}	// namespace analysis
}	// namespace nvs
