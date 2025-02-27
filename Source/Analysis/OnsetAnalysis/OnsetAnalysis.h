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

namespace nvs {
namespace analysis {

//===================================================================================
// e.g. vecReal const *sweepVec = new vecReal({ makeSweptSine(100.f, 1000.f, 5000) });
vecReal makeSweptSine(Real const low, Real const high, size_t const len, Real const sampleRate = 44100.f);
//===================================================================================

array2dReal calculateOnsetsMatrix(vecReal const &waveform, streamingFactory const &factory,
						  analysisSettings const anSettings, std::function<bool(void)> runLoopCallback=[](){return true;});
vecReal calculateOnsetsInSeconds(array2dReal onsetAnalysisMatrix, standardFactory const &factory,
						analysisSettings const anSettings, onsetSettings const onSettings);
vecVecReal featuresForSbic(vecReal const &waveform, AlgorithmFactory const &factory,
						   analysisSettings const anSettings, bfccSettings const bfccSettings,
						   std::function<bool(void)> runLoopCallback=[](){return true;});

inline array2dReal vecVecToArray2dReal(vecVecReal const &vv){
	return essentia::transpose(essentia::vecvecToArray2D(vv));
}

vecReal sBic(array2dReal featureMatrix, standardFactory const &factory,
			 analysisSettings const anSettings, sBicSettings const sbicSsettings);
vecVecReal splitWaveIntoEvents(vecReal const &wave, vecReal const &onsetsInSeconds, streamingFactory const &factory,
							   analysisSettings const anSettings, splitSettings const splitSettings,
							   std::function<bool(void)> runLoopCallback=[](){return true;});
void writeWav(vecReal const &wave, std::string_view name, streamingFactory const &factory,
			  analysisSettings const anSettings,
			  std::function<bool(void)> runLoopCallback=[](){return true;});
void writeWavs(vecVecReal const &waves, std::string_view defName, streamingFactory const &factory,
			   analysisSettings const anSettings,
			   std::function<bool(void)> runLoopCallback=[](){return true;});

}	// namespace analysis
}	// namespace nvs
