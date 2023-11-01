/*
  ==============================================================================

    TimbreAnalysis.h
    Created: 30 Oct 2023 2:07:11pm
    Author:  Nicholas Solem

  ==============================================================================
*/

#pragma once

#include "EssentiaSetup.h"
#include "AnalysisUsing.h"
#include "Settings.h"
#include <span>

namespace nvs {
namespace analysis {

vecVecReal
getBFCCs(std::span<Real const> waveSpan, streamingFactory const &factory,
		 analysisSettings const anSettings, bfccSettings const bfSettings);

vecVecReal PCA(vecVecReal const &V, standardFactory const &factory);


std::pair<Real, Real> getRangeOfDimension(vecVecReal const &V, size_t dim);
Real getNormalizationMultiplier(std::pair<Real, Real> range);

}	// namespace analysis
}	// namespace nvs
