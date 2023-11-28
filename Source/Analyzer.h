/*
  ==============================================================================

    Analyzer.h
    Created: 12 Sep 2023 10:25:24pm
    Author:  Nicholas Solem

  ==============================================================================
*/

#pragma once
#include <optional>
#include "onsetAnalysis/OnsetAnalysis.h"
#include "TimbreAnalysis.h"
#include <JuceHeader.h>

namespace nvs {
namespace analysis {

class Analyzer {
private:
	nvs::ess::EssentiaInitializer ess_init;
public:
	Analyzer();
	
	std::optional<vecReal> calculateOnsets(vecReal wave);
	std::optional<vecVecReal> calculateOnsetwiseBFCCs(vecReal wave, vecReal onsetsInSeconds);
	std::optional<vecVecReal> calculatePCA(vecVecReal const &V);

	nvs::ess::EssentiaHolder ess_hold;
	
	analysisSettings _analysisSettings;
	onsetSettings _onsetSettings;
	splitSettings _splitSettings;
	bfccSettings _bfccSettings;
};

inline void cleanOnsets (vecReal &onsetsInSeconds, float maxLengthInSeconds){
//	float const lengthInSeconds = static_cast<float>(_wavespan.size()) / _fileSampleRate;
	size_t properOnsets = onsetsInSeconds.size();
	for (auto it = onsetsInSeconds.rbegin(); it != onsetsInSeconds.rend(); ++it){
		if (*it > maxLengthInSeconds){
			--properOnsets;
		}
		else {	// since the vector is sorted, we know that there are no more exceeding the proper length
			break;
		}
	}
	assert(properOnsets <= onsetsInSeconds.size());
	onsetsInSeconds.resize(properOnsets);
}


Real mean(vecReal const &V);
vecVecReal truncate(vecVecReal const &V, size_t trunc);
vecVecReal transpose(vecVecReal const &V);
vecReal binwiseMean(vecVecReal const &V);


void writeEventsToWav(vecReal wave, std::vector<float> onsetsInSeconds, std::string_view ogPath, Analyzer analyzer);

}	// namespace analysis
}	// namespace nvs

