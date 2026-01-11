/*
  ==============================================================================

    Settings.cpp
    Created: 4 May 2025 2:13:31pm
    Author:  Nicholas Solem

  ==============================================================================
*/

#include "Settings.h"
#include "Analyzer.h"

namespace nvs::analysis {

static constexpr bool TIMBRE_SPACE_SETTINGS_EXIST {false};  // these 'settings' were meant to be automatable, so they are now parameters

static juce::NormalisableRange<double> makePowerOfTwoRange (double minValue, double maxValue)
{
    auto minLog = std::log2 (minValue), maxLog = std::log2 (maxValue);
    return {
        minValue, maxValue,
        [=] (double, double, double n) { return std::pow (2.0, juce::jmap (n, 0.0, 1.0, minLog, maxLog)); },
        [=] (double, double, double v) { return juce::jmap (std::log2(v), minLog, maxLog, 0.0, 1.0); },
        [] (double s, double e, double v) { auto c=juce::jlimit(s, e, v); return std::pow(2.0,std::round(std::log2(double(c)))); }
    };
}

const std::map<juce::String, AnySpec> analysisSpecs
{
	{ axiom::frameSize,     RangedSettingsSpec<int>{   makePowerOfTwoRange(64, 8192), 1024 } },
	{ axiom::hopSize,       RangedSettingsSpec<int>{   makePowerOfTwoRange(32, 4096),  512 } },
	{ axiom::windowingType,  ChoiceSettingsSpec{ 	{axiom::hann, axiom::hamming, axiom::hannnsgcq,
		axiom::triangular, axiom::square, axiom::blackmanharris62, axiom::blackmanharris70,
		axiom::blackmanharris74, 	axiom::blackmanharris92}, /* default: */		axiom::hann 		} },
    { axiom::numThreads, RangedSettingsSpec<int>{NormalisableRange<double>(1, SystemStats::getNumCpus()), SystemStats::getNumPhysicalCpus(),
        "The number of threads used for timbral analysis. Higher # of threads => faster analysis, but limited testing has been done for greater than 1 thread."}}
};

const std::map<juce::String, AnySpec> bfccSpecs
{
	{ axiom::highFrequencyBound,  RangedSettingsSpec<double>{ {20.0,24000.0,1.0,0.4},4000.0,
	    "Upper bound of the frequency range. Bandlimiting to ~4000 can help for classification.", 1, "Hz"} },
	{ axiom::lowFrequencyBound,   RangedSettingsSpec<double>{ {0.0,5000.0,1.0,0.4}, 500.0,
	    "Lower bound of the frequency range. Bandlimiting to ~500 can help with classification.", 1, "Hz"} },
	{ axiom::liftering,           RangedSettingsSpec<int>{   {0,100,1,1 },0,
	    "the liftering coefficient. Use '0' to bypass it"} },
	{ axiom::numBands,            RangedSettingsSpec<int>{
	    {1,128,1,1},40,
	    "the number of bark bands in the filter" } },
	{ axiom::numCoefficients,     RangedSettingsSpec<int>{   {5,26,1,1}, 13 } },
	{ axiom::normalize,           ChoiceSettingsSpec{
	    {axiom::unit_sum, axiom::unit_max},axiom::unit_sum,
	    "'unit_max' makes the vertex of all the triangles equal to 1, 'unit_sum' makes the area of all the triangles equal to 1." } },
    { axiom::BFCC0_frameNormalizationFactor, RangedSettingsSpec<double>{{0.0, 1.0, 0.0, 1.0}, 1.0/20.0,
        "Per frame, normalize the BFCC vector based on the 0th BFCC, which represents overall energy. This adjusts the frame's contribution to the overall event's BFCC calculation."}},
    { axiom::BFCC0_eventNormalize, BoolSettingsSpec{true,
        "Per event, normalize the BFCC vector based on the 0th BFCC, which represents overall energy. This normalizes the overall event's BFCCs based on the 0th."}},
	{ axiom::spectrumType,        ChoiceSettingsSpec{ {axiom::magnitude, axiom::power},axiom::power,
	    "use magnitude or power spectrum"} },
	{ axiom::weightingType,       ChoiceSettingsSpec{ {axiom::warping,  axiom::linear},axiom::warping,
	    "type of weighting function for determining triangle area"} },
	{ axiom::dctType,             ChoiceSettingsSpec{ {axiom::typeII,   axiom::typeIII},axiom::typeII } }
};

const std::map<juce::String, AnySpec> onsetSpecs
{
    { axiom::segmentation, ChoiceSettingsSpec {{axiom::Event, axiom::Uniform}, axiom::Event,
        "whether to segment by detected events or uniform frames"} },
	{ axiom::silenceThreshold,         RangedSettingsSpec<double>{ {0.0,1.0,0.01f,0.4}, 0.1f,
	    "the threshold for silence"} },
	{ axiom::alpha,                    RangedSettingsSpec<double>{ {0.0,1.0,0.01f,0.4}, 0.1f,
	    "the proportion of the mean included to reject smaller peaks; filters very short onsets" } },
	{ axiom::numFrames_shortOnsetFilter,RangedSettingsSpec<int>  { { 1,  64,  1,   1 },    5,
	    "the number of frames used to compute the threshold; size of short-onset filter"} },
	{ axiom::weight_hfc,               RangedSettingsSpec<double>{ {0.0,1.0,0.01f,1.0},  0.0,
	    "the High Frequency Content detection function which accurately detects percussive events" } },
	{ axiom::weight_complex,           RangedSettingsSpec<double>{ {0.0,1.0,0.01f,1.0},  0.1,
	    "the Complex-Domain spectral difference function taking into account changes in magnitude and phase. It emphasizes note onsets either as a result of significant change in energy in the magnitude spectrum, and/or a deviation from the expected phase values in the phase spectrum, caused by a change in pitch." } },
	{ axiom::weight_complexPhase,      RangedSettingsSpec<double>{ {0.0,1.0,0.01f,1.0},  0.0,
	    "the simplified Complex-Domain spectral difference function taking into account phase changes, weighted by magnitude. It reacts better on tonal sounds such as bowed string, but tends to over-detect percussive events."} },
	{ axiom::weight_flux,              RangedSettingsSpec<double>{ {0.0,1.0,0.01f,1.0},  0.0,
	    "the Spectral Flux detection function which characterizes changes in magnitude spectrum." } },
	{ axiom::weight_rms,               RangedSettingsSpec<double>{ {0.0,1.0,0.01f,1.0},  0.0,
	    "the difference function, measuring the half-rectified change of the RMS of the magnitude spectrum (i.e., measuring overall energy flux)" } }
};

const std::map<juce::String, AnySpec> sBicSpecs
{
	{ axiom::complexityPenaltyWeight, RangedSettingsSpec<double>{ {0.0,10.0,0.1f,1.0}, 1.5f } },
	{ axiom::incrementFirstPass,      RangedSettingsSpec<int>{   {1,500,1,1},      60  } },
	{ axiom::incrementSecondPass,     RangedSettingsSpec<int>{   {1,500,1,1},      20  } },
	{ axiom::minSegmentLengthFrames,  RangedSettingsSpec<int>{   {1,100,1,1},      10  } },
	{ axiom::sizeFirstPass,           RangedSettingsSpec<int>{   {1,1000,1,1},     300 } },
	{ axiom::sizeSecondPass,          RangedSettingsSpec<int>{   {1,1000,1,1},     200 } }
};

const std::map<juce::String, AnySpec> pitchSpecs
{
	{ axiom::pitchDetectionAlgorithm,  ChoiceSettingsSpec{ {axiom::yin,axiom::pYin,axiom::chroma}, axiom::yin } },
	{ axiom::interpolate,              BoolSettingsSpec{ true } },
	{ axiom::maxFrequency,             RangedSettingsSpec<double>{ {20.0,22050.0, 1.0, 1.0}, 4000.0 } },
	{ axiom::minFrequency,             RangedSettingsSpec<double>{ {20.0,22050.0, 1.0, 1.0},  140.0 } },
	{ axiom::tolerance,                RangedSettingsSpec<double>{ {0.0, 1.0,  0.001f, 1.0},   0.15 } }
};

const std::map<juce::String, AnySpec> loudnessSpecs
{
    {axiom::equalizeLoudness, BoolSettingsSpec{true}}
};

const std::map<juce::String, AnySpec> splitSpecs
{
	{ axiom::fadeInSamps,  RangedSettingsSpec<int>{ {0,10000,1,1}, 5 } },
	{ axiom::fadeOutSamps, RangedSettingsSpec<int>{ {0,10000,1,1}, 5 } }
};

const std::map<juce::String, const std::map<juce::String,AnySpec>*>
	specsByBranch
{
	{ axiom::Analysis, &analysisSpecs },
	{ axiom::BFCC,     &bfccSpecs     },
	{ axiom::Onset,    &onsetSpecs    },
	{ axiom::sBic,     &sBicSpecs     },
	{ axiom::Pitch,    &pitchSpecs    },
	{ axiom::Loudness,  &loudnessSpecs    },
	{ axiom::Split,    &splitSpecs    },
};

void ensureBranchAndInitializeDefaults (juce::ValueTree& settingsVT,
										const juce::String& branchName) {
	auto branchVT = settingsVT.getOrCreateChildWithName (branchName, nullptr);

	if (const auto it = specsByBranch.find (branchName); it != specsByBranch.end()) {
        for (auto const& specMap = *it->second; const auto&[fst, snd] : specMap) {
			auto propName = fst;
			auto const& branchSpec = snd;

			std::visit ([&]<typename T0>(T0&& spec){
				using SpecT = std::decay_t<T0>;

				if constexpr (std::is_same_v<SpecT, RangedSettingsSpec<int>> ||
							  std::is_same_v<SpecT, RangedSettingsSpec<double>>) {
					branchVT.setProperty (propName, spec.defaultValue, nullptr);
				}
				else if constexpr (std::is_same_v<SpecT, ChoiceSettingsSpec>) {
					branchVT.setProperty (propName, spec.defaultValue, nullptr);
				}
				else if constexpr (std::is_same_v<SpecT, BoolSettingsSpec>) {
					branchVT.setProperty (propName, spec.defaultValue, nullptr);
				}
			}, branchSpec);
		}
	}
}

void initializeSettingsBranches(juce::ValueTree& settingsVT, const bool dbg){
	for (auto& [branchName, _] : specsByBranch) {
		ensureBranchAndInitializeDefaults (settingsVT, branchName);
	}
    if constexpr (!TIMBRE_SPACE_SETTINGS_EXIST) {
        if (const auto timbreSpaceSettingsTree = settingsVT.getChildWithName(axiom::TimbreSpace); timbreSpaceSettingsTree.isValid())
        {
            settingsVT.removeChild(timbreSpaceSettingsTree, nullptr);
            std::cout << "removed previously existing TimbreSpace settings subtree\n";
        }
    }
	if (dbg){
		std::cout << "initializeSettingsBranches tree: " << settingsVT.toXmlString();
	}
}

bool verifySettingsStructure (const juce::ValueTree& settingsVT)
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
            if (juce::Identifier propertyId (propertyName);
                !branchVT.hasProperty(propertyId))
            {
				return false;
			}
		}
	}

	return true;
}
bool verifySettingsStructureWithAttemptedFix (juce::ValueTree& settingsVT)
{
    if (! settingsVT.isValid()){
        jassertfalse;
        return false;
    }
    for (auto const& [branchName, specMapPtr] : specsByBranch) {
        auto branchId = juce::Identifier (branchName);
        auto branchVT = settingsVT.getChildWithName (branchId);

        // check branch validity
        if (! branchVT.isValid()) {
            ensureBranchAndInitializeDefaults (settingsVT, branchName);
            branchVT = settingsVT.getChildWithName (branchId);
            jassert(branchVT.isValid());
        }

        // check every parameter key inside that branch
        for (auto const& [fst, spec] : *specMapPtr) {
            const auto propertyName = fst;  // copy because structured bindings aren't lambda-captured in C++20
            if (juce::Identifier propertyId (propertyName);
                !branchVT.hasProperty(propertyId))
            {
                const auto it = specsByBranch.find (branchName);
                if (it == specsByBranch.end()) {
                    jassertfalse;
                    return false;
                }
                const std::map<juce::String, AnySpec> branchSpecs = *it->second;
                auto thing = branchSpecs.find(propertyName);
                if (thing == branchSpecs.end()) {
                    jassertfalse;
                    return false;
                }
                auto propSpec = thing->second;
                std::visit ([&]<typename T0>(T0&& anySpec){
                    using SpecT = std::decay_t<T0>;
                    branchVT.setProperty (propertyName, anySpec.defaultValue, nullptr);
                }, propSpec);
            }
        }
    }
    return true;
}

bool updateSettingsFromValueTree(AnalyzerSettings& settings, const juce::ValueTree& settingsTree) {
    auto parent = settingsTree.getParent();
    auto const fileInfoTree = parent.getChildWithName(axiom::FileInfo);
    settings.info.sampleFilePath = fileInfoTree.getProperty(axiom::sampleFilePath).toString();
    // settings.info.author = parent.getProperty("author").toString();
    if (!fileInfoTree.hasProperty(axiom::sampleRate)) {
        std::cerr << "Parent node missing required properties\n";
        jassertfalse;
        return false;
    }
    settings.analysis.sampleRate = fileInfoTree.getProperty(axiom::sampleRate);
    jassert(0.0 < settings.analysis.sampleRate);


    auto analysisNode = settingsTree.getChildWithName(axiom::Analysis);

    if (!analysisNode.isValid()) {
        std::cerr << "Analysis node missing\n";
        jassertfalse;
        return false;
    }
    if (!analysisNode.hasProperty(axiom::frameSize) || !analysisNode.hasProperty(axiom::hopSize)
        || !analysisNode.hasProperty(axiom::windowingType)) {
        std::cerr << "Analysis node missing required properties\n";
        jassertfalse;
        return false;
        }
    settings.analysis.frameSize = analysisNode.getProperty(axiom::frameSize);
    settings.analysis.hopSize = analysisNode.getProperty(axiom::hopSize);
    settings.analysis.windowingType = analysisNode.getProperty(axiom::windowingType).toString();
    settings.analysis.numThreads = analysisNode.getProperty(axiom::numThreads);

    // BFCC settings
    auto bfccNode = settingsTree.getChildWithName(axiom::BFCC);
    if (!bfccNode.isValid()) {
        std::cerr << "BFCC node missing\n";
        jassertfalse;
        return false;
    }
    if (!bfccNode.hasProperty(axiom::dctType) || !bfccNode.hasProperty(axiom::highFrequencyBound) ||
        !bfccNode.hasProperty(axiom::liftering) || !bfccNode.hasProperty(axiom::lowFrequencyBound) ||
        !bfccNode.hasProperty(axiom::normalize) || !bfccNode.hasProperty(axiom::numBands) ||
        !bfccNode.hasProperty(axiom::numCoefficients) || !bfccNode.hasProperty(axiom::spectrumType) ||
        !bfccNode.hasProperty(axiom::weightingType)
        || !bfccNode.hasProperty(axiom::BFCC0_frameNormalizationFactor) || !bfccNode.hasProperty(axiom::BFCC0_eventNormalize))
    {
        std::cerr << "BFCC node missing required properties\n";
        jassertfalse;
        return false;
    }
    settings.bfcc.dctType = bfccNode.getProperty(axiom::dctType).toString();
    settings.bfcc.highFrequencyBound = bfccNode.getProperty(axiom::highFrequencyBound);
    settings.bfcc.liftering = bfccNode.getProperty(axiom::liftering);
    settings.bfcc.lowFrequencyBound = bfccNode.getProperty(axiom::lowFrequencyBound);
    settings.bfcc.normalize = bfccNode.getProperty(axiom::normalize).toString();
    settings.bfcc.numBands = bfccNode.getProperty(axiom::numBands);
    settings.bfcc.numCoefficients = bfccNode.getProperty(axiom::numCoefficients);
    settings.bfcc.spectrumType = bfccNode.getProperty(axiom::spectrumType).toString();
    settings.bfcc.weightingType = bfccNode.getProperty(axiom::weightingType).toString();
    settings.bfcc.BFCC0_frameNormalizationFactor = bfccNode.getProperty(axiom::BFCC0_frameNormalizationFactor);
    settings.bfcc.BFCC0_eventNormalize = bfccNode.getProperty(axiom::BFCC0_eventNormalize);

    // Onset settings
    auto onsetNode = settingsTree.getChildWithName(axiom::Onset);
    if (!onsetNode.isValid()) {
        std::cerr << "Onset node missing\n";
        jassertfalse;
        return false;
    }
    if (!onsetNode.hasProperty(axiom::alpha) || !onsetNode.hasProperty(axiom::numFrames_shortOnsetFilter) ||
        !onsetNode.hasProperty(axiom::silenceThreshold) || !onsetNode.hasProperty(axiom::weight_complex) ||
        !onsetNode.hasProperty(axiom::weight_complexPhase) || !onsetNode.hasProperty(axiom::weight_flux) ||
        !onsetNode.hasProperty(axiom::weight_hfc) || !onsetNode.hasProperty(axiom::weight_rms))
    {
        std::cerr << "Onset node missing required properties\n";
        jassertfalse;
        return false;
    }
    if (onsetNode.hasProperty(axiom::segmentation)) {
        const auto segmentationStr = onsetNode.getProperty(axiom::segmentation).toString();
        settings.onset.segmentation = segmentationStr == axiom::Uniform ? AnalyzerSettings::Onset::Segmentation::Uniform : AnalyzerSettings::Onset::Segmentation::Event;
    } else {
        settings.onset.segmentation = AnalyzerSettings::Onset::Segmentation::Event;
        DBG(juce::String("No property ") + axiom::segmentation + " found in settingsTree\n");
    }
	settings.onset.alpha = onsetNode.getProperty(axiom::alpha);
	settings.onset.numFrames_shortOnsetFilter = onsetNode.getProperty(axiom::numFrames_shortOnsetFilter);
	settings.onset.silenceThreshold = onsetNode.getProperty(axiom::silenceThreshold);
	settings.onset.weight_complex = onsetNode.getProperty(axiom::weight_complex);
	settings.onset.weight_complexPhase = onsetNode.getProperty(axiom::weight_complexPhase);
	settings.onset.weight_flux = onsetNode.getProperty(axiom::weight_flux);
	settings.onset.weight_hfc = onsetNode.getProperty(axiom::weight_hfc);
	settings.onset.weight_rms = onsetNode.getProperty(axiom::weight_rms);
	
	// Pitch settings
	auto pitchNode = settingsTree.getChildWithName(axiom::Pitch);
	if (!pitchNode.isValid()) {
		std::cerr << "Pitch node missing\n";
		jassertfalse;
		return false;
	}
	if (!pitchNode.hasProperty(axiom::interpolate) || !pitchNode.hasProperty(axiom::maxFrequency) ||
		!pitchNode.hasProperty(axiom::minFrequency) || !pitchNode.hasProperty(axiom::pitchDetectionAlgorithm) ||
		!pitchNode.hasProperty(axiom::tolerance)) {
		std::cerr << "Pitch node missing required properties\n";
		jassertfalse;
		return false;
	}
	settings.pitch.interpolate = pitchNode.getProperty(axiom::interpolate);
	settings.pitch.maxFrequency = pitchNode.getProperty(axiom::maxFrequency);
	settings.pitch.minFrequency = pitchNode.getProperty(axiom::minFrequency);
	settings.pitch.pitchDetectionAlgorithm = pitchNode.getProperty(axiom::pitchDetectionAlgorithm).toString();
	settings.pitch.tolerance = pitchNode.getProperty(axiom::tolerance);

    // Loudness settings
    auto loudnessNode = settingsTree.getChildWithName(axiom::Loudness);
    if (!loudnessNode.isValid()) {
        std::cerr << "Loudness node missing\n";
        jassertfalse;
        return false;
    }
    if (!loudnessNode.hasProperty(axiom::equalizeLoudness)) {
        std::cerr << "Loudness node missing required properties\n";
        jassertfalse;
        return false;
    }
    settings.loudness.equalizeLoudness = loudnessNode.getProperty(axiom::equalizeLoudness);

	// Split settings
	auto splitNode = settingsTree.getChildWithName(axiom::Split);
	if (!splitNode.isValid()) {
		std::cerr << "Split node missing\n";
		jassertfalse;
		return false;
	}
	if (!splitNode.hasProperty(axiom::fadeInSamps) || !splitNode.hasProperty(axiom::fadeOutSamps)) {
		std::cerr << "Split node missing required properties\n";
		jassertfalse;
		return false;
	}
	settings.split.fadeInSamps = splitNode.getProperty(axiom::fadeInSamps);
	settings.split.fadeOutSamps = splitNode.getProperty(axiom::fadeOutSamps);
	
	// TimbreSpace settings
    if constexpr (TIMBRE_SPACE_SETTINGS_EXIST) {
        ValueTree timbreSpaceNode = settingsTree.getChildWithName("TimbreSpace");
        if (!timbreSpaceNode.isValid()) {
            std::cerr << "TimbreSpace node missing\n";
            jassertfalse;
            return false;
        }
        if (!timbreSpaceNode.hasProperty("HistogramEqualization") || !timbreSpaceNode.hasProperty("xAxis") ||
            !timbreSpaceNode.hasProperty("yAxis")) {
            std::cerr << "TimbreSpace node missing required properties\n";
            jassertfalse;
            return false;
            }
    }

	// sBic settings
	auto sBicNode = settingsTree.getChildWithName(axiom::sBic);
	if (!sBicNode.isValid()) {
		std::cerr << "sBic node missing\n";
		jassertfalse;
		return false;
	}
	if (!sBicNode.hasProperty(axiom::complexityPenaltyWeight) || !sBicNode.hasProperty(axiom::incrementFirstPass) ||
		!sBicNode.hasProperty(axiom::incrementSecondPass) || !sBicNode.hasProperty(axiom::minSegmentLengthFrames) ||
		!sBicNode.hasProperty(axiom::sizeFirstPass) || !sBicNode.hasProperty(axiom::sizeSecondPass)) {
		std::cerr << "sBic node missing required properties\n";
		jassertfalse;
		return false;
	}
	settings.sBic.complexityPenaltyWeight = sBicNode.getProperty(axiom::complexityPenaltyWeight);
	settings.sBic.incrementFirstPass = sBicNode.getProperty(axiom::incrementFirstPass);
	settings.sBic.incrementSecondPass = sBicNode.getProperty(axiom::incrementSecondPass);
	settings.sBic.minSegmentLengthFrames = sBicNode.getProperty(axiom::minSegmentLengthFrames);
	settings.sBic.sizeFirstPass = sBicNode.getProperty(axiom::sizeFirstPass);
	settings.sBic.sizeSecondPass = sBicNode.getProperty(axiom::sizeSecondPass);
	
	return true;
}

}	// namespace nvs::analysis
