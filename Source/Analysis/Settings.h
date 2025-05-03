/*
  ==============================================================================

    Settings.h
    Created: 30 Oct 2023 2:15:28pm
    Author:  Nicholas Solem

  ==============================================================================
*/

#pragma once
#include "essentia/types.h"
#include <JuceHeader.h>

namespace nvs {
namespace analysis {

template<typename T>
struct RangeWithDefault
{
	juce::NormalisableRange<double> range;	// always double to be compatible with slider
	T defaultValue;
};
struct ChoiceWithDefault
{
	std::vector<juce::String> options;
	juce::String              defaultValue;
};

struct BoolWithDefault
{
	bool defaultValue;
};

// 2) Extend the variant

using AnySpec = std::variant<
	RangeWithDefault<int>,
	RangeWithDefault<double>,
	ChoiceWithDefault,
	BoolWithDefault
>;

static juce::NormalisableRange<double> makePowerOfTwoRange (double minValue, double maxValue)
{
	auto minLog = std::log2 (minValue), maxLog = std::log2 (maxValue);
	return {
		minValue, maxValue,
		[=] (double s, double e, double n) { return std::pow (2.0, juce::jmap (n, 0.0, 1.0, minLog, maxLog)); },
		[=] (double s, double e, double v) { return juce::jmap (std::log2(v), minLog, maxLog, 0.0, 1.0); },
		[] (double s, double e, double v) { auto c=juce::jlimit(s, e, v); return std::pow(2.0,std::round(std::log2(double(c)))); }
	};
}
using AnyRangeWithDefault = std::variant<
	RangeWithDefault<int>,
	RangeWithDefault<double>
>;
struct NumericRanges
{
	std::map<juce::String, AnyRangeWithDefault> floatAndIntRanges;
};

static const std::map<juce::String, AnySpec> analysisSpecs
{
	{ "sampleRate",    RangeWithDefault<double>{ {8000.0,192000.0,1.0,1.0}, 44100.0 } },
	{ "frameSize",     RangeWithDefault<int>{   makePowerOfTwoRange(64, 8192), 1024 } },
	{ "hopSize",       RangeWithDefault<int>{   makePowerOfTwoRange(16, 4096),  512 } }
};

static const std::map<juce::String, AnySpec> bfccSpecs
{
	{ "highFrequencyBound",  RangeWithDefault<double>{ {20.0,24000.0,1.0,1.0}, 11000.0 } },
	{ "lowFrequencyBound",   RangeWithDefault<double>{ {0.0,5000.0,1.0,1.0},    20.0 } },
	{ "liftering",           RangeWithDefault<int>{   {0,100,1,1},            0 } },
	{ "numBands",            RangeWithDefault<int>{   {1,128,1,1},           40 } },
	{ "numCoefficients",     RangeWithDefault<int>{   {1,64,1,1},            13 } },
	{ "normalize",           ChoiceWithDefault{ {"unit_sum", "unit_max"},     "unit_sum" } },
	{ "spectrumType",        ChoiceWithDefault{ {"magnitude", "power"},       "power"    } },
	{ "weightingType",       ChoiceWithDefault{ {"warping",  "linear"},       "warping"  } },
	{ "dctType",             ChoiceWithDefault{ {"typeII",   "typeIII"},      "typeII"   } }
};

static const std::map<juce::String, AnySpec> onsetSpecs
{
	{ "silenceThreshold",         RangeWithDefault<double>{ {0.0,1.0,0.01f,1.0}, 0.2f } },
	{ "alpha",                    RangeWithDefault<double>{ {0.0,1.0,0.01f,1.0}, 0.1f } },
	{ "numFrames_shortOnsetFilter",RangeWithDefault<int>{   {1,64,1,1},          5 } },
	{ "weight_hfc",               RangeWithDefault<double>{ {0.0,10.0,0.1f,1.0},  1.0 } },
	{ "weight_complex",           RangeWithDefault<double>{ {0.0,10.0,0.1f,1.0},  1.0 } },
	{ "weight_complexPhase",      RangeWithDefault<double>{ {0.0,10.0,0.1f,1.0},  1.0 } },
	{ "weight_flux",              RangeWithDefault<double>{ {0.0,10.0,0.1f,1.0},  1.0 } },
	{ "weight_melFlux",           RangeWithDefault<double>{ {0.0,10.0,0.1f,1.0},  1.0 } },
	{ "weight_rms",               RangeWithDefault<double>{ {0.0,10.0,0.1f,1.0},  1.0 } }
};

static const std::map<juce::String, AnySpec> sBicSpecs
{
	{ "complexityPenaltyWeight", RangeWithDefault<double>{ {0.0,10.0,0.1f,1.0}, 1.5f } },
	{ "incrementFirstPass",      RangeWithDefault<int>{   {1,500,1,1},      60  } },
	{ "incrementSecondPass",     RangeWithDefault<int>{   {1,500,1,1},      20  } },
	{ "minSegmentLengthFrames",  RangeWithDefault<int>{   {1,100,1,1},      10  } },
	{ "sizeFirstPass",           RangeWithDefault<int>{   {1,1000,1,1},     300 } },
	{ "sizeSecondPass",          RangeWithDefault<int>{   {1,1000,1,1},     200 } }
};

static const std::map<juce::String, AnySpec> pitchSpecs
{
	{ "pitchDetectionAlgorithm",  ChoiceWithDefault{ {"yin","pYin","chroma"}, "yin" } },
	{ "interpolate",              BoolWithDefault{ true } },
	{ "maxFrequency",             RangeWithDefault<double>{ {20.0,22050.0, 1.0, 1.0}, 22050.0 } },
	{ "minFrequency",             RangeWithDefault<double>{ {20.0,22050.0, 1.0, 1.0}, 20.0    } },
	{ "tolerance",                RangeWithDefault<double>{ {0.0, 1.0,  0.001f, 1.0},  0.15 } }
};

static const std::map<juce::String, AnySpec> splitSpecs
{
	{ "fadeInSamps",  RangeWithDefault<int>{ {0,10000,1,1}, 5 } },
	{ "fadeOutSamps", RangeWithDefault<int>{ {0,10000,1,1}, 5 } }
};

static const std::map<juce::String, const std::map<juce::String,AnySpec>*>
	specsByBranch
{
	{ "Analysis", &analysisSpecs },
	{ "BFCC",     &bfccSpecs     },
	{ "Onset",    &onsetSpecs    },
	{ "sBic",     &sBicSpecs     },
	{ "Pitch",    &pitchSpecs    },
	{ "Split",    &splitSpecs    }
};
inline void ensureBranchAndInitializeDefaults (juce::ValueTree& settingsVT,
										const juce::String& branchName)
{
	auto branchVT = settingsVT.getOrCreateChildWithName (branchName, nullptr);

	if (auto it = specsByBranch.find (branchName); it != specsByBranch.end())
	{
		auto const& specMap = *it->second;
		for (auto const& kv : specMap)
		{
			auto prop = kv.first;
			auto const& anySpec = kv.second;

			std::visit ([&](auto&& spec){
				using SpecT = std::decay_t<decltype(spec)>;

				if constexpr (std::is_same_v<SpecT, RangeWithDefault<int>> ||
							  std::is_same_v<SpecT, RangeWithDefault<double>>)
				{
					branchVT.setProperty (prop, spec.defaultValue, nullptr);
				}
				else if constexpr (std::is_same_v<SpecT, ChoiceWithDefault>)
				{
					branchVT.setProperty (prop, spec.defaultValue, nullptr);
				}
				else if constexpr (std::is_same_v<SpecT, BoolWithDefault>)
				{
					branchVT.setProperty (prop, spec.defaultValue, nullptr);
				}
			}, anySpec);
		}
	}
}


inline void initializeSettingsBranches(juce::ValueTree& settingsVT){
	for (auto& [branchName, _] : specsByBranch)
	{
		ensureBranchAndInitializeDefaults (settingsVT, branchName);
	}
}



using essentia::Real;

// settings structs
struct analysisSettings {
	Real sampleRate {44100.f};
	int frameSize {1024};
	int hopSize {512};

	enum DimensionalityReduction_e {
		noReduction = 0,
		PCA = 1
	};
	DimensionalityReduction_e dimensionalityReduction { noReduction };
	const static inline std::map<DimensionalityReduction_e, std::string>
	dimensionalityReductionMap {
		{noReduction, "No Reduction"},
		{PCA, "PCA"}
	};
};
struct onsetSettings
{
	Real silenceThreshold {0.2f};
	Real alpha {0.1f};
	unsigned int numFrames_shortOnsetFilter {5};
	
	Real weight_hfc {1.f};
	Real weight_complex {1.f};
	Real weight_complexPhase {1.f};
	Real weight_flux {1.f};
	Real weight_melFlux {1.f};
	Real weight_rms {1.f};
	
	std::array<Real*, 6> weights {
		&weight_hfc,
		&weight_complex,
		&weight_complexPhase,
		&weight_flux,
		&weight_melFlux,
		&weight_rms
	};
};
struct bfccSettings
{
	Real highFrequencyBound {11000.f};
	Real lowFrequencyBound {0.f};
	//	int inputSize {1025};	get automatically from incoming spectrum
	
	int liftering {0};
	int numBands {40};
	int numCoefficients{13};
	
	enum normalize_e {
		unit_sum = 0,
		unit_max
	};
	normalize_e normType {normalize_e::unit_max};
	enum spectrumType_e {
		magnitude = 0,
		power
	};
	spectrumType_e specType {spectrumType_e::power};
	enum weightingType_e {
		warping = 0,
		linear
	};
	weightingType_e weightType {weightingType_e::linear};
	enum dctType_e {
		typeII = 2,
		typeIII = 3
	};
	int dctType {dctType_e::typeII};

	const static inline std::map<normalize_e, std::string>
	normMap {
		{unit_sum, "unit_sum"},
		{unit_max, "unit_max"}
	};
	const static inline std::map<spectrumType_e, std::string>
	spectrumTypeMap {
		{magnitude, "magnitude"},
		{power, "power"}
	};
	const static inline	std::map<weightingType_e, std::string>
	weightingTypeMap {
		{warping, "warping"},
		{linear, "linear"}
	};
	
	std::string getNormalizeTypeAsString() const {return normMap.at(normType);}
	std::string getSpectrumTypeAsString() const {return spectrumTypeMap.at(specType);}
	std::string getWeightingTypeAsString() const {return weightingTypeMap.at(weightType);}
};
struct sBicSettings
{
	Real complexityPenaltyWeight {1.5f};
	int incrementFirstPass {60};	// min: 1
	int incrementSecondPass {20};
	int minSegmentLengthFrames {10};
	int sizeFirstPass {300};
	int sizeSecondPass {200};
};
struct pitchSettings
{
	enum pitchDetectionAlgorithm_e {
		yin = 0,
		pYin,
		chroma
	};
	pitchDetectionAlgorithm_e algo {pitchDetectionAlgorithm_e::yin};
	bool interpolate {true};
	Real maxFrequency {22050.f};
	Real minFrequency {20.f};
	Real tolerance {0.15f};
	
	const static inline std::map<pitchDetectionAlgorithm_e, std::string>
	pitchDetectionAlgorithmMap {
		{yin, "PitchYin"},
		{pYin, "PitchYinProbabilistic"},
		{chroma, "Chromagram"}
	};
	std::string getPitchDetectionAlgoAsString() const { return pitchDetectionAlgorithmMap.at(algo); }
};
struct splitSettings
{
	size_t fadeInSamps {0};
	size_t fadeOutSamps {0};
};

}	// namespace analysis
}	// namespace nvs
