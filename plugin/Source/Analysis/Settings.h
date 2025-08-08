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


struct AnalyzerSettings {
	struct Analysis {
		double sampleRate = 0.0;
		int frameSize = 1024;
		int hopSize = 1024;
		juce::String windowingType = "hann";
	} analysis;
	
	struct BFCC {
		juce::String dctType = "typeII";
		double highFrequencyBound = 8000.0;
		int liftering = 0;
		double lowFrequencyBound = 100.0;
		juce::String normalize = "unit_sum";
		int numBands = 40;
		int numCoefficients = 13;
		juce::String spectrumType = "power";
		juce::String weightingType = "warping";
	} bfcc;
	
	struct Onset {
		double alpha = 0.1;
		int numFrames_shortOnsetFilter = 5;
		double silenceThreshold = 0.1;
		double weight_complex = 0.5;
		double weight_complexPhase = 0.5;
		double weight_flux = 0.5;
		double weight_hfc = 0.5;
		double weight_melFlux = 0.5;
		double weight_rms = 0.5;
	} onset;
	
	struct Pitch {
		bool interpolate = true;
		double maxFrequency = 3000.0;
		double minFrequency = 100.0;
		juce::String pitchDetectionAlgorithm = "yin";
		double tolerance = 0.15;
	} pitch;
	
	struct Split {
		int fadeInSamps = 5;
		int fadeOutSamps = 5;
	} split;
	
	struct TimbreSpace {
		double histogramEqualization = 0.0;
		juce::String xAxis = "bfcc1";
		juce::String yAxis = "bfcc2";
	} timbreSpace;
	
	struct SBic {
		double complexityPenaltyWeight = 1.5;
		int incrementFirstPass = 60;
		int incrementSecondPass = 20;
		int minSegmentLengthFrames = 10;
		int sizeFirstPass = 300;
		int sizeSecondPass = 200;
	} sBic;
	
	struct Info {
		juce::String sampleFilePath;
		juce::String author;
	} info;
};
bool updateSettingsFromValueTree(AnalyzerSettings& settings, const juce::ValueTree& settingsTree);

}	// namespace analysis
}	// namespace nvs
