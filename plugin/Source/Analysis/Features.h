/*
  ==============================================================================

    Features.h
    Created: 2 Jul 2025 8:35:17pm
    Author:  Nicholas Solem

  ==============================================================================
*/

#pragma once
#include "../../slicer_granular/Source/misc_util.h"


namespace nvs::analysis {

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
	f0,
	
	NumFeatures
};
constexpr int NumBFCC = 13;

typedef nvs::util::Iterator<Features, Features::bfcc0, Features::f0> featuresIterator;

inline juce::String toString(Features f){
	if (static_cast<int>(f) < NumBFCC){
		juce::String s = "bfcc";
		s += static_cast<int>(f);
		return s;
	}
	switch (f) {
		case Features::Periodicity:
			return "Periodicity";
		case Features::Loudness:
			return "Loudness";
		case Features::f0:
			return "f0";
		default:
			jassertfalse;
			return "";
	}
}
inline Features toFeature(const juce::String &s){
	if (s.contains("bfcc")){
		const juce::String intPart = s.removeCharacters("bfcc");
		int i = intPart.getIntValue();
		return static_cast<Features>(i);
	}
	if (s == "Periodicity"){
		return Features::Periodicity;
	}
	else if (s == "Loudness"){
		return Features::Loudness;
	}
	else if (s == "f0"){
		return Features::f0;
	}
	jassertfalse;
	return Features::bfcc0;
}

inline const juce::StringArray& getFeatureChoiceVec() {
    static const auto features = [] -> juce::StringArray {
        StringArray result;
        for (const auto f : featuresIterator()) {
            result.add(toString(f));
        }
        return result;
    }();
    return features;
}

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


inline bool isBFCC(Features f) {
	int i = static_cast<int>(f);
	return (0 <= i && i < NumBFCC);
}


template <typename T>	// T can foreseeably be either single Real or EventwiseStatistics
struct FeatureContainer {
	std::vector<T> bfccs {};
	T periodicity {};
	T loudness {};
	T f0 {};
};

}	// namespace nvs::analysis
