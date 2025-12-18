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

namespace nvs::analysis {

void ensureBranchAndInitializeDefaults (juce::ValueTree& settingsVT, const juce::String& branchName);
void initializeSettingsBranches(juce::ValueTree& settingsVT, bool dbg=true);
bool verifySettingsStructure (const juce::ValueTree& settingsVT);

template<typename T>
struct RangedSettingsSpec
{
    juce::NormalisableRange<double> range;
    T defaultValue;
    juce::String tooltip = {};           // Optional tooltip
    int numDecimalPlaces = 2;            // Default precision
    juce::String unit = {};              // e.g., "Hz", "dB", "ms"
};
struct ChoiceSettingsSpec
{
    std::vector<juce::String> options;
    juce::String defaultValue;
    juce::String tooltip = {};
};

struct BoolSettingsSpec
{
    bool defaultValue;
    juce::String tooltip = {};
};

using AnySpec = std::variant<
    RangedSettingsSpec<int>,
    RangedSettingsSpec<double>,
    ChoiceSettingsSpec,
    BoolSettingsSpec
>;

// defined in .cpp to avoid circular include (issue was just with calling nvs::analysis::buildFeatureChoiceVec, but this allows consistency)
extern const std::map<juce::String, AnySpec> analysisSpecs, bfccSpecs, onsetSpecs, sBicSpecs, pitchSpecs, splitSpecs, timbreSpaceSpecs;
extern const std::map<juce::String, const std::map<juce::String,AnySpec>*> specsByBranch;


struct AnalyzerSettings {
    struct Analysis {
        double sampleRate = 0.0;
        int frameSize = 1024;
        int hopSize = 1024;
        juce::String windowingType = "hann";
        int numThreads = 2;
    } analysis;

    struct BFCC {
        juce::String dctType = "typeII";
        double highFrequencyBound = 4000.0;
        int liftering = 0;
        double lowFrequencyBound = 500.0;
        juce::String normalize = "unit_sum";
        double BFCC0_frameNormalizationFactor {1.0 / 20.0};
        bool BFCC0_eventNormalize {true};
        int numBands = 40;
        int numCoefficients = 13;
        juce::String spectrumType = "power";
        juce::String weightingType = "warping";
    } bfcc;

    struct SpectralComplexity {
        double magnitudeThreshold = 0.005;
    } spectralComplexity;

    struct Onset {
        double alpha = 0.1;
        int numFrames_shortOnsetFilter = 5;
        double silenceThreshold = 0.1;
        double weight_complex = 0.5;
        double weight_complexPhase = 0.5;
        double weight_flux = 0.5;
        double weight_hfc = 0.5;
        double weight_rms = 0.5;
    } onset;

    struct Pitch {
        bool interpolate = true;
        double maxFrequency = 3000.0;
        double minFrequency = 100.0;
        juce::String pitchDetectionAlgorithm = "yin";
        double tolerance = 0.15;
    } pitch;

    struct Loudness {
        bool equalizeLoudness = true;
    } loudness;

    struct Split {
        int fadeInSamps = 5;
        int fadeOutSamps = 5;
    } split;

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

}
