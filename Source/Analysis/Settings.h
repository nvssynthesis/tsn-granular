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
	{ "frameSize",     RangeWithDefault<int>{   makePowerOfTwoRange(64, 8192), 1024 } },
	{ "hopSize",       RangeWithDefault<int>{   makePowerOfTwoRange(16, 4096),  512 } },
	{ "windowingType",  ChoiceWithDefault{ 	{"hann", "hamming", "hannnsgcq",
		"triangular", "square", "blackmanharris62", "blackmanharris70",
		"blackmanharris74", 	"blackmanharris92"}, /* default: */		"hann" 		} }
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
	{ "weight_hfc",               RangeWithDefault<double>{ {0.0,1.0,0.1f,1.0},  1.0 } },
	{ "weight_complex",           RangeWithDefault<double>{ {0.0,1.0,0.1f,1.0},  1.0 } },
	{ "weight_complexPhase",      RangeWithDefault<double>{ {0.0,1.0,0.1f,1.0},  1.0 } },
	{ "weight_flux",              RangeWithDefault<double>{ {0.0,1.0,0.1f,1.0},  1.0 } },
	{ "weight_melFlux",           RangeWithDefault<double>{ {0.0,1.0,0.1f,1.0},  1.0 } },
	{ "weight_rms",               RangeWithDefault<double>{ {0.0,1.0,0.1f,1.0},  1.0 } }
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
										const juce::String& branchName)  {
	auto branchVT = settingsVT.getOrCreateChildWithName (branchName, nullptr);

	if (auto it = specsByBranch.find (branchName); it != specsByBranch.end()) {
		auto const& specMap = *it->second;
		for (auto const& kv : specMap) {
			auto prop = kv.first;
			auto const& anySpec = kv.second;

			std::visit ([&](auto&& spec){
				using SpecT = std::decay_t<decltype(spec)>;

				if constexpr (std::is_same_v<SpecT, RangeWithDefault<int>> ||
							  std::is_same_v<SpecT, RangeWithDefault<double>>) {
					branchVT.setProperty (prop, spec.defaultValue, nullptr);
				}
				else if constexpr (std::is_same_v<SpecT, ChoiceWithDefault>) {
					branchVT.setProperty (prop, spec.defaultValue, nullptr);
				}
				else if constexpr (std::is_same_v<SpecT, BoolWithDefault>) {
					branchVT.setProperty (prop, spec.defaultValue, nullptr);
				}
			}, anySpec);
		}
	}
}


inline void initializeSettingsBranches(juce::ValueTree& settingsVT){
	for (auto& [branchName, _] : specsByBranch) {
		ensureBranchAndInitializeDefaults (settingsVT, branchName);
	}
}

inline bool verifySettingsStructure (const juce::ValueTree& settingsVT)
{
	if (! settingsVT.isValid()){
		return false;
	}
	for (auto const& [branchName, specMapPtr] : specsByBranch) {
		auto branchId = juce::Identifier (branchName);
		auto branchVT = settingsVT.getChildWithName (branchId);

		// check branch validity
		if (! branchVT.isValid()) {
			return false; // missing entire branch
		}

		// check every parameter key inside that branch
		for (auto const& [propertyName, spec] : *specMapPtr) {
			juce::Identifier propertyId (propertyName);
			if (! branchVT.getChildWithName (propertyId).isValid()) {
				return false; // missing this setting
			}
		}
	}

	return true;
}

}	// namespace analysis
}	// namespace nvs
