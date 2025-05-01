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

enum class Features {
	bfcc0,
	bfcc1,
	bfcc2,
	bfcc3,
	bfcc4,
	bfcc5,
	bfcc6,
	bfcc7,
	bfcc8,
	bfcc9,
	bfcc10,
	bfcc11,
	bfcc12,
	
	Periodicity,
	Loudness,
	f0
};

const std::set<Features> bfccSet {
	Features::bfcc0,
	Features::bfcc1,
	Features::bfcc2,
	Features::bfcc3,
	Features::bfcc4,
	Features::bfcc5,
	Features::bfcc6,
	Features::bfcc7,
	Features::bfcc8,
	Features::bfcc9,
	Features::bfcc10,
	Features::bfcc11,
	Features::bfcc12,
};

constexpr int NumBFCC = 13;

inline bool isBFCC(Features f) {
	int i = static_cast<int>(f);
	return (0 <= i && i < NumBFCC);
}

enum class Statistic {
	Mean,
	Median,
	Variance,
	Skewness,
	Kurtosis
};

template <typename T>
struct EventwiseStatistics {
	T mean		{};
	T median	{};
	T variance	{};
	T skewness	{};
	T kurtosis	{};
};
using EventwisePitchDescription = EventwiseStatistics<float>;
using EventwiseBFCCDescription = std::vector<EventwiseStatistics<Real>>;

template <typename T>	// T can foreseeably be either single Real or EventwiseStatistics
struct FeatureContainer {
	std::vector<T> bfccs {};
	T periodicity {};
	T loudness {};
	T f0 {};
};

namespace {
template<typename T>
using FeatureContainerMemberPtr = T FeatureContainer<T>::*;
}
template<typename T>
inline auto makeScalarLookup() {
	// index this array by (enumValue - NumBFCC)
	// so scalarIndex = enumValue - NumBFCC must be 0..ScalarCount-1
	static_assert(static_cast<int>(Features::Periodicity) == NumBFCC + 0);
	static_assert(static_cast<int>(Features::Loudness) == NumBFCC + 1);
	static_assert(static_cast<int>(Features::f0) == NumBFCC + 2);
	constexpr int ScalarCount = 3;
	std::array<FeatureContainerMemberPtr<T>, ScalarCount> table {
		&FeatureContainer<T>::periodicity, // index 0
		&FeatureContainer<T>::loudness,    // index 1
		&FeatureContainer<T>::f0           // index 2
	};
	return table;
}


template<typename T>
[[nodiscard]]
inline std::vector<T>
extractFeatures(FeatureContainer<T> const & allFeatures,
				std::set<Features> featuresToUse)
{
	std::vector<T> v;
	v.reserve(featuresToUse.size());

	// one lookup table for the scalars
	auto const & scalarTable = makeScalarLookup<T>();

	for (auto f : featuresToUse) {
		int idx = static_cast<int>(f);
		if (0 <= idx && idx < NumBFCC) {
			// it's a BFCC
			v.push_back(allFeatures.bfccs[idx]);
		}
		else {
			// it's one of the scalars
			int scalarIndex = idx - NumBFCC; // 0=>periodicity,1=>loudness,2=>f0
			v.push_back(allFeatures.*(scalarTable[scalarIndex]));
		}
	}

	return v;
}

[[nodiscard]]
inline std::vector<Real>
extractFeatures(FeatureContainer<EventwiseStatistics<Real>> const & allFeatures,
				std::set<Features> featuresToUse,
				Statistic statisticToUse)
{
	auto descs = extractFeatures(allFeatures, featuresToUse);
	Real EventwiseStatistics<Real>::* ptr = nullptr;
	switch (statisticToUse) {
		case Statistic::Mean:     ptr = &EventwiseStatistics<Real>::mean;    	break;
		case Statistic::Median:   ptr = &EventwiseStatistics<Real>::median;  	break;
		case Statistic::Variance: ptr = &EventwiseStatistics<Real>::variance;	break;
		case Statistic::Skewness: ptr = &EventwiseStatistics<Real>::skewness;	break;
		case Statistic::Kurtosis: ptr = &EventwiseStatistics<Real>::kurtosis;	break;
	}
	
	std::vector<Real> out;
	out.reserve(descs.size());
	for (auto const & d : descs) {
		out.push_back(d.*ptr);
	}
	return out;
}

class Analyzer {
private:
	nvs::ess::EssentiaInitializer ess_init;
public:
	Analyzer();
	
	std::optional<vecReal> calculateOnsetsInSeconds(vecReal const &wave, std::function<bool(void)> runLoopCallback=[](){return true;});
	EventwisePitchDescription calculateEventwisePitchDescription(vecReal const &waveEvent);
	EventwiseBFCCDescription calculateEventwiseBFCCDescription(vecReal const &waveEvent);
	std::optional<std::vector<FeatureContainer<EventwiseStatistics<Real>>>> calculateOnsetwiseTimbreSpace(vecReal const &wave, vecReal const &normalizedOnsets);
	std::optional<vecVecReal> calculatePCA(std::vector<FeatureContainer<EventwiseStatistics<Real>>> const &allFeatures, std::set<Features> featuresToUse, Statistic statToUse);

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

