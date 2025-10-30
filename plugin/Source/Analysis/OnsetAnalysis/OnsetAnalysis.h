/*
  ==============================================================================

    OnsetAnalysis.h
    Created: 14 Jun 2023 10:28:35am
    Author:  Nicholas Solem

  ==============================================================================
*/

#pragma once
#include "Analysis/AnalysisUsing.h"
#include "Analysis/Settings.h"
#include "../RunLoopStatus.h"

namespace nvs {
namespace analysis {

//===================================================================================
// e.g. vecReal const *sweepVec = new vecReal({ makeSweptSine(100.f, 1000.f, 5000) });
vecReal makeSweptSine(Real low, Real high, size_t len, Real sampleRate = 44100.f);
//===================================================================================
inline array2dReal vecVecToArray2dReal(vecVecReal const &vv){
    return essentia::transpose(essentia::vecvecToArray2D(vv));
}

array2dReal calculateOnsetsMatrix(vecReal const &waveform, streamingFactory const &factory, AnalyzerSettings const &settings,
								  RunLoopStatus& rls, const ShouldExitFn &shouldExit);
vecReal calculateOnsetsInSeconds(const array2dReal &onsetAnalysisMatrix, standardFactory const &factory, AnalyzerSettings const &settings);

vecVecReal featuresForSbic(vecReal const &waveform, AlgorithmFactory const &factory,  AnalyzerSettings const &settings,
						   RunLoopStatus& rls, const ShouldExitFn &shouldExit);
vecReal sBic(const array2dReal &featureMatrix, standardFactory const &factory, AnalyzerSettings const &settings);

vecVecReal splitWaveIntoEvents(vecReal const &wave, vecReal const &onsetsInSeconds, streamingFactory const &factory, AnalyzerSettings const &settings,
							   RunLoopStatus& rls, const ShouldExitFn &shouldExit);

void writeWav(vecReal const &wave, std::string_view name, streamingFactory const &factory, AnalyzerSettings const &settings,
			  RunLoopStatus& rls, const ShouldExitFn &shouldExit);
void writeWavs(vecVecReal const &waves, std::string_view defName, streamingFactory const &factory, AnalyzerSettings const &settings,
			   RunLoopStatus& rls, const ShouldExitFn &shouldExit);

}	// namespace analysis
}	// namespace nvs
