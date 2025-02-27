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
	
	std::optional<vecReal> calculateOnsetsInSeconds(vecReal const &wave, std::function<bool(void)> runLoopCallback=[](){return true;});
	EventwisePitchDescription calculateEventwisePitchDescription(vecReal const &waveEvent);
	EventwiseBFCCDescription calculateEventwiseBFCCDescription(vecReal const &waveEvent);
	std::optional<vecVecReal> calculateOnsetwiseTimbreSpace(vecReal const &wave, vecReal const &normalizedOnsets);
	std::optional<vecVecReal> calculatePCA(vecVecReal const &V);

	nvs::ess::EssentiaHolder ess_hold;
	
	analysisSettings _analysisSettings;
	pitchSettings _pitchSettings {};
	onsetSettings _onsetSettings;
	splitSettings _splitSettings;
	bfccSettings _bfccSettings;
};

inline double getLengthInSeconds(auto lengthInSamples, auto sampleRate){
	return static_cast<double>(lengthInSamples / sampleRate);
}

inline void filterOnsets(std::vector<float> &onsetsInSeconds, double lengthInSeconds, float minimumOnsetDeltaSeconds = 0.02f)
{
	assert( std::is_sorted(onsetsInSeconds.begin(), onsetsInSeconds.end()) );
	{	// filter out onsets that exceed the file length
		
		// starting at end, count onsets exceeding lengthInSeconds
		size_t numProperOnsets = onsetsInSeconds.size();
		for (auto it = onsetsInSeconds.rbegin(); it != onsetsInSeconds.rend(); ++it){
			if (*it > lengthInSeconds){
				--numProperOnsets;
			}
			else {	// since the vector is sorted, we know that there are no more exceeding the proper length
				break;
			}
		}
		onsetsInSeconds.resize(numProperOnsets);
		
	}
	// filter out redundant onsets (can be defined by onsets which are too close together)
	{
		auto new_end = std::unique(onsetsInSeconds.begin(), onsetsInSeconds.end(), [minimumOnsetDeltaSeconds](float a, float b){
			return (b - a) < minimumOnsetDeltaSeconds;
		});
		onsetsInSeconds.erase(new_end, onsetsInSeconds.end());	// erase from new end to original end
	}
}

inline void normalizeOnsets(std::vector<float> &onsetsInSeconds, float lengthInSeconds) {
	std::transform(onsetsInSeconds.begin(), onsetsInSeconds.end(),	// formerly terminated via begin() + numProperOnsets
				   onsetsInSeconds.begin(), [=](float f)
				{
					double res = f / lengthInSeconds;
					assert(res <= 1.f);
					return res;
				});
}
inline void denormalizeOnsets(std::vector<float> &normalizedOnsets, float lengthInSeconds) {
	std::transform(normalizedOnsets.begin(), normalizedOnsets.end(),
				   normalizedOnsets.begin(), [=](float f)
				{
					return f * lengthInSeconds;
				});
}

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

void writeEventsToWav(vecReal const &wave, std::vector<float> const &onsetsInSeconds, std::string_view ogPath, Analyzer &analyzer);

}	// namespace analysis
}	// namespace nvs

