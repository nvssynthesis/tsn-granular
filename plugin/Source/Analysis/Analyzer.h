/*
  ==============================================================================

    Analyzer.h
    Created: 12 Sep 2023 10:25:24pm
    Author:  Nicholas Solem

  ==============================================================================
*/

#pragma once
#include <optional>
#include <algorithm>
#include <JuceHeader.h>
#include "RunLoopStatus.h"
#include "TimbreAnalysis/TimbreAnalysis.h"
#include "Features.h"
#include "Statistics.h"
#include "Settings.h"


namespace nvs::analysis {

template<typename T>
using FeatureContainerMemberPtr = T FeatureContainer<T>::*;

template<typename T>
auto makeScalarLookup() {
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
std::vector<T>
extractFeatures(const FeatureContainer<T> & allFeatures,
				const std::vector<Features> featuresToUse)
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
				const std::vector<Features> &featuresToUse,
				const Statistic statisticToUse)
{
	const auto descriptions = extractFeatures(allFeatures, featuresToUse);
	Real EventwiseStatistics<Real>::* ptr = nullptr;
	switch (statisticToUse) {
		case Statistic::Mean:     ptr = &EventwiseStatistics<Real>::mean;    	break;
		case Statistic::Median:   ptr = &EventwiseStatistics<Real>::median;  	break;
		case Statistic::Variance: ptr = &EventwiseStatistics<Real>::variance;	break;
		case Statistic::Skewness: ptr = &EventwiseStatistics<Real>::skewness;	break;
		case Statistic::Kurtosis: ptr = &EventwiseStatistics<Real>::kurtosis;	break;
	    case Statistic::NumStatistics: jassertfalse; break;
		default: jassertfalse;
	}
	
	std::vector<Real> out;
	out.reserve(descriptions.size());
	for (auto const & d : descriptions) {
		out.push_back(d.*ptr);
	}
	return out;
}

class Analyzer {
private:
	nvs::ess::EssentiaInitializer ess_init;	  // this MUST be initialized before EssentiaHolder.
public:
	Analyzer();
	using EventwiseStats = EventwiseStatistics<Real>;

	std::optional<vecReal> calculateOnsetsInSeconds(vecReal const &wave, RunLoopStatus& rls, const ShouldExitFn &shouldExit) const;
	
	EventwisePitchDescription calculateEventwisePitchDescription(vecReal const &waveEvent) const;
	EventwiseBFCCDescription calculateEventwiseBFCCDescription(vecReal const &waveEvent) const;
	EventwiseStats calculateEventwiseLoudness(vecReal const &waveEvent) const;

	std::optional<std::vector<FeatureContainer<EventwiseStats>>> calculateOnsetwiseTimbreSpace(vecReal const &wave, vecReal const &normalizedOnsets,
																										  RunLoopStatus& rls, const ShouldExitFn &shouldExit) const;
	
	std::optional<vecVecReal> calculatePCA(std::vector<FeatureContainer<EventwiseStats>> const &allFeatures,
	    const std::vector<Features> &featuresToUse, Statistic statToUse) const;
	
	float getAnalyzedFileSampleRate() const;

	bool updateSettings(const juce::ValueTree &newSettings);
	AnalyzerSettings const &getSettings() const;
	
	nvs::ess::EssentiaHolder ess_hold;
private:
	AnalyzerSettings settings;
};

double getLengthInSeconds(auto lengthInSamples, auto sampleRate){
    jassert(sampleRate > 0);
	return static_cast<double>(lengthInSamples / sampleRate);
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

void writeEventsToWav(vecReal const &wave, std::vector<float> const &onsetsInSeconds, std::string_view ogPath, const Analyzer &analyzer, RunLoopStatus& rls, ShouldExitFn shouldExit);

}	// namespace nvs::analysis
