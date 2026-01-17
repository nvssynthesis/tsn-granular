/*
  ==============================================================================

    Features.h
    Created: 2 Jul 2025 8:35:17pm
    Author:  Nicholas Solem

  ==============================================================================
*/

#pragma once
#include <span>
#include <set>

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

const std::set bfccSet {
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
