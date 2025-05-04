/*
  ==============================================================================

    Settings.h
    Created: 30 Oct 2023 2:15:28pm
    Author:  Nicholas Solem

  ==============================================================================
*/

#pragma once
#include "essentia/types.h"
#include "Analyzer.h"
#include <JuceHeader.h>

namespace nvs {
namespace analysis {

void ensureBranchAndInitializeDefaults (juce::ValueTree& settingsVT, const juce::String& branchName);
void initializeSettingsBranches(juce::ValueTree& settingsVT, bool dbg=true);
bool verifySettingsStructure (const juce::ValueTree& settingsVT);

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

using AnyRangeWithDefault = std::variant<
	RangeWithDefault<int>,
	RangeWithDefault<double>
>;

// defined in .cpp to avoid circular include (issue was just with  calling nvs::analysis::buildFeatureChoiceVec, but this allows consistency)
extern const std::map<juce::String, AnySpec> analysisSpecs, bfccSpecs, onsetSpecs, sBicSpecs, pitchSpecs, splitSpecs, timbreSpaceSpecs;
extern const std::map<juce::String, const std::map<juce::String,AnySpec>*> specsByBranch;

}	// namespace analysis
}	// namespace nvs
