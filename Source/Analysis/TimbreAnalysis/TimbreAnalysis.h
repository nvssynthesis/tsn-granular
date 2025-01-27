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
#include "../aubio/src/aubio.h"

namespace nvs {
namespace analysis {

std::vector<float> getPitches(std::span<Real> waveSpan);

vecVecReal
getBFCCs(std::span<Real const> waveSpan, streamingFactory const &factory,
		 analysisSettings const anSettings, bfccSettings const bfSettings);

vecVecReal PCA(vecVecReal const &V, standardFactory const &factory);


std::pair<Real, Real> getRangeOfDimension(vecVecReal const &V, size_t dim);
Real getNormalizationMultiplier(std::pair<Real, Real> range);

}	// namespace analysis
}	// namespace nvs
