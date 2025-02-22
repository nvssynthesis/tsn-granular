/*
  ==============================================================================

    Analyzer.h
    Created: 12 Sep 2023 10:25:24pm
    Author:  Nicholas Solem

  ==============================================================================
*/

#pragma once
#include <optional>
#include "OnsetAnalysis/OnsetAnalysis.h"
#include "Analysis/TimbreAnalysis/TimbreAnalysis.h"
#include <JuceHeader.h>

namespace nvs {
namespace analysis {

template <typename T>
struct EventwiseDescription {
	T median	{};
	T range		{};
	T slope		{};	// linear regression
};
using EventwisePitchDescription = EventwiseDescription<float>;
using EventwiseBFCCDescription = EventwiseDescription<std::vector<float>>;


class Analyzer {
private:
	nvs::ess::EssentiaInitializer ess_init;
public:
	Analyzer();
	
	std::optional<vecReal> calculateOnsets(vecReal wave, std::function<bool(void)> runLoopCallback=[](){return true;});
	EventwisePitchDescription calculateEventwisePitchDescription(vecReal waveEvent);
	EventwiseBFCCDescription calculateEventwiseBFCCDescription(vecReal waveEvent);
	std::optional<vecVecReal> calculateOnsetwiseTimbreSpace(vecReal wave, vecReal onsetsInSeconds);
	std::optional<vecVecReal> calculatePCA(vecVecReal const &V);

	nvs::ess::EssentiaHolder ess_hold;
	
	analysisSettings _analysisSettings;
	pitchSettings _pitchSettings {};
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

#if 0
Real mean(vecReal const &V);
Real median(vecReal V);
#endif
vecVecReal truncate(vecVecReal const &V, size_t trunc);
vecVecReal transpose(vecVecReal const &V);

template <typename Func>
concept StatisticVectorFunction = requires(Func f, vecReal const &v) {
	{ f(v) } -> std::convertible_to<Real>;
};

template <StatisticVectorFunction Func>
vecReal binwiseStatistic(vecVecReal const &V, Func statisticFunc) {
	vecVecReal Vtranspose = transpose(V);
	size_t const sz = Vtranspose.size();
	vecReal results(sz);
	for (size_t i = 0; i < sz; ++i) {
		results[i] = statisticFunc(Vtranspose[i]);
	}
	return results;
}

void writeEventsToWav(vecReal wave, std::vector<float> onsetsInSeconds, std::string_view ogPath, Analyzer analyzer);

}	// namespace analysis
}	// namespace nvs

