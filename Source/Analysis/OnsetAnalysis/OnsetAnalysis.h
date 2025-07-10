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
#include <JuceHeader.h>

namespace nvs {
namespace analysis {

//===================================================================================
// e.g. vecReal const *sweepVec = new vecReal({ makeSweptSine(100.f, 1000.f, 5000) });
vecReal makeSweptSine(Real const low, Real const high, size_t const len, Real const sampleRate = 44100.f);
//===================================================================================

array2dReal calculateOnsetsMatrix(vecReal const &waveform, streamingFactory const &factory, AnalyzerSettings const &settings,
								  RunLoopStatus& rls, ShouldExitFn shouldExit);
vecReal calculateOnsetsInSeconds(array2dReal onsetAnalysisMatrix, standardFactory const &factory, AnalyzerSettings const &settings);
vecVecReal featuresForSbic(vecReal const &waveform, AlgorithmFactory const &factory,  AnalyzerSettings const &settings,
						   RunLoopStatus& rls, ShouldExitFn shouldExit);

inline array2dReal vecVecToArray2dReal(vecVecReal const &vv){
	return essentia::transpose(essentia::vecvecToArray2D(vv));
}

vecReal sBic(array2dReal featureMatrix, standardFactory const &factory, AnalyzerSettings const &settings);
vecVecReal splitWaveIntoEvents(vecReal const &wave, vecReal const &onsetsInSeconds, streamingFactory const &factory, AnalyzerSettings const &settings,
							   RunLoopStatus& rls, ShouldExitFn shouldExit);
void writeWav(vecReal const &wave, std::string_view name, streamingFactory const &factory, AnalyzerSettings const &settings,
			  RunLoopStatus& rls, ShouldExitFn shouldExit);
void writeWavs(vecVecReal const &waves, std::string_view defName, streamingFactory const &factory, AnalyzerSettings const &settings,
			   RunLoopStatus& rls, ShouldExitFn shouldExit);

}	// namespace analysis
}	// namespace nvs
