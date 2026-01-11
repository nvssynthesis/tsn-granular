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
constexpr auto makeScalarLookup() {
	// index this array by (enumValue - NumBFCC)
	// so scalarIndex = enumValue - NumBFCC must be 0..ScalarCount-1
	static_assert(static_cast<int>(Features::SpectralCentroid) == NumBFCC + 0);
	static_assert(static_cast<int>(Features::SpectralDecrease) == NumBFCC + 1);
	static_assert(static_cast<int>(Features::SpectralFlatness) == NumBFCC + 2);
    static_assert(static_cast<int>(Features::SpectralCrest) == NumBFCC + 3);
    static_assert(static_cast<int>(Features::SpectralComplexity) == NumBFCC + 4);
    static_assert(static_cast<int>(Features::StrongPeak) == NumBFCC + 5);

    static_assert(static_cast<int>(Features::Periodicity) == NumBFCC + 6);
    static_assert(static_cast<int>(Features::Loudness) == NumBFCC + 7);
    static_assert(static_cast<int>(Features::f0) == NumBFCC + 8);

	constexpr int ScalarCount = 9;
	static constexpr std::array<FeatureContainerMemberPtr<T>, ScalarCount> table {
	    &FeatureContainer<T>::features[Features::SpectralCentroid],
	    &FeatureContainer<T>::spectralDecrease,
	    &FeatureContainer<T>::spectralFlatness,
	    &FeatureContainer<T>::spectralCrest,
	    &FeatureContainer<T>::spectralComplexity,
	    &FeatureContainer<T>::strongPeak,

		&FeatureContainer<T>::periodicity,
		&FeatureContainer<T>::loudness,
		&FeatureContainer<T>::f0
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
	auto const & scalarTable = allFeatures.features;

	for (auto f : featuresToUse) {
		v.push_back(allFeatures.features[static_cast<size_t>(f)]);
	}
	return v;
}

[[nodiscard]]
inline vecReal
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

	std::optional<vecReal>
    calculateOnsetsInSeconds(
        vecReal const &wave,
        RunLoopStatus& rls,
	    const ShouldExitFn &shouldExit) const;
	
	void calculateEventwisePitchDescription(vecReal const &waveEvent, FeatureContainer<EventwiseStats> &features) const;
	void calculateEventwiseTimbreDescription(vecReal const &waveEvent, FeatureContainer<EventwiseStats> &features) const;
	void calculateEventwiseLoudness(vecReal const &waveEvent, FeatureContainer<EventwiseStats> &features) const;

	std::optional<std::vector<FeatureContainer<EventwiseStats>>>
    calculateOnsetwiseTimbreSpace(
        const vecReal &wave,
        const vecReal &onsetsInSeconds,
        RunLoopStatus& rls,
        const ShouldExitFn &shouldExit) const;

    static std::optional<vecVecReal> calculatePCA(
	    const std::vector<FeatureContainer<EventwiseStats>> &allFeatures,
	    const std::vector<Features> &featuresToUse,
	    Statistic statToUse);
	
	float getAnalyzedFileSampleRate() const;

	bool updateSettings(juce::ValueTree &newSettings, bool attemptFix);
	AnalyzerSettings const &getSettings() const;
    juce::String getSettingsHash() const {
        return _settingsHash;
    }

    //====================================================================================
	nvs::ess::EssentiaHolder ess_hold;
private:
	AnalyzerSettings settings;
    juce::String _settingsHash {};
};

double getLengthInSeconds(auto lengthInSamples, auto sampleRate){
    jassert(sampleRate > 0);
	return static_cast<double>(lengthInSamples / sampleRate);
}

vecVecReal truncate(vecVecReal const &V, size_t trunc);
vecVecReal transpose(vecVecReal const &V);
vecVecReal transpose(std::span<const vecReal> V);

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
