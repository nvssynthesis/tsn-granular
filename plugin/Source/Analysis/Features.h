/*
  ==============================================================================

    Features.h
    Created: 2 Jul 2025 8:35:17pm
    Author:  Nicholas Solem

  ==============================================================================
*/

#pragma once
#include "../../slicer_granular/Source/misc_util.h"
// #include "Statistics.h"
#include <span>

namespace nvs::analysis {

enum class Feature_e {
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

    SpectralCentroid,
    SpectralDecrease,
    SpectralFlatness,
    SpectralCrest,
    SpectralComplexity,
    StrongPeak,

	Periodicity,
	Loudness,
	f0,
	
	NumFeatures
};
static constexpr int NumBFCC = 13;
static constexpr auto NumTimbralFeatures = static_cast<int>(Feature_e::StrongPeak);
static_assert(NumTimbralFeatures == 18);

typedef nvs::util::Iterator<Feature_e, Feature_e::bfcc0, Feature_e::f0> featuresIterator;

inline juce::String toString(Feature_e f){
	if (static_cast<int>(f) < NumBFCC){
		juce::String s = "bfcc";
		s += static_cast<int>(f);
		return s;
	}
	switch (f) {
	    case Feature_e::SpectralCentroid:
	        return "SpectralCentroid";
	    case Feature_e::SpectralDecrease:
	        return "SpectralDecrease";
	    case Feature_e::SpectralFlatness:
	        return "SpectralFlatness";
	    case Feature_e::SpectralCrest:
	        return "SpectralCrest";
	    case Feature_e::SpectralComplexity:
	        return "SpectralComplexity";
	    case Feature_e::StrongPeak:
	        return "StrongPeak";
		case Feature_e::Periodicity:
			return "Periodicity";
		case Feature_e::Loudness:
			return "Loudness";
		case Feature_e::f0:
			return "f0";
		default:
			jassertfalse;
			return "";
	}
}
inline Feature_e toFeature(const juce::String &s){
	if (s.contains("bfcc")){
		const juce::String intPart = s.removeCharacters("bfcc");
		int i = intPart.getIntValue();
		return static_cast<Feature_e>(i);
	}
	if (s == "Periodicity"){
		return Feature_e::Periodicity;
	}
	else if (s == "Loudness"){
		return Feature_e::Loudness;
	}
	else if (s == "f0"){
		return Feature_e::f0;
	}
	jassertfalse;
	return Feature_e::bfcc0;
}

inline const juce::StringArray& getFeaturesStringArray() {
    static const auto features = [] -> juce::StringArray {
        StringArray result;
        for (const auto f : featuresIterator()) {
            result.add(toString(f));
        }
        return result;
    }();
    return features;
}

const std::set<Feature_e> bfccSet {
	Feature_e::bfcc0,
	Feature_e::bfcc1,
	Feature_e::bfcc2,
	Feature_e::bfcc3,
	Feature_e::bfcc4,
	Feature_e::bfcc5,
	Feature_e::bfcc6,
	Feature_e::bfcc7,
	Feature_e::bfcc8,
	Feature_e::bfcc9,
	Feature_e::bfcc10,
	Feature_e::bfcc11,
	Feature_e::bfcc12,
};


inline bool isBFCC(Feature_e f) {
	int i = static_cast<int>(f);
	return (0 <= i && i < NumBFCC);
}


template <typename T>	// T can foreseeably be either single Real, vecReal, or EventwiseStatistics
struct FeatureContainer {
    std::array<T, static_cast<size_t>(Feature_e::NumFeatures)> features {};

    T& operator[](Feature_e f) { return features[static_cast<size_t>(f)]; }
    const T& operator[](Feature_e f) const { return features[static_cast<size_t>(f)]; }

    std::span<T> bfccs() { return {features.data(), NumBFCC}; }
    std::span<const T> bfccs() const { return {features.data(), NumBFCC}; }
};
inline void pushBFCCFrame(FeatureContainer<std::vector<float>>& container, std::span<const float> bfccFrame) {
    assert(bfccFrame.size() == NumBFCC);
    for (size_t i = 0; i < NumBFCC; ++i) {
        container[static_cast<Feature_e>(i)].push_back(bfccFrame[i]);
    }
}

}	// namespace nvs::analysis
