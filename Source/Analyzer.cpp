/*
  ==============================================================================

    Analyzer.cpp
    Created: 27 Sep 2023 12:25:12am
    Author:  Nicholas Solem

  ==============================================================================
*/

#include "Analyzer.h"

namespace nvs {
namespace analysis {

Analyzer::Analyzer()
:	ess_hold(ess_init)
{}
	
std::optional<std::vector<float>> Analyzer::calculateOnsets(std::vector<float> wave){
	if (!wave.size()){
		return std::nullopt;
	}
	
	analysis::array2dReal onsets2d = onsetAnalysis(wave, ess_hold.factory, _analysisSettings);
	essentia::standard::AlgorithmFactory &tmpStFac = essentia::standard::AlgorithmFactory::instance();
	
	std::vector<float> onsetsInSeconds = analysis::onsetsInSeconds(onsets2d, tmpStFac, _analysisSettings, _onsetSettings);
	
	float const sr = _analysisSettings.sampleRate;
	assert(sr > 0.f);
	float const lengthInSeconds = wave.size() / sr;
	cleanOnsets(onsetsInSeconds, lengthInSeconds);
	return onsetsInSeconds;
}

}	// namespace analysis
}	// namespace nvs

