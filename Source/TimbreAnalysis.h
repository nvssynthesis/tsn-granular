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

vecReal getBFCCs(std::span<Real> waveSpan, streamingFactory const &factory, bfccSettings const settings);

}	// namespace analysis
}	// namespace nvs
