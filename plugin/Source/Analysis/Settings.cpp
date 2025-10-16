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
	{ "frameSize",     RangeWithDefault<int>{   makePowerOfTwoRange(64, 8192), 1024 } },
	{ "hopSize",       RangeWithDefault<int>{   makePowerOfTwoRange(16, 4096),  512 } },
	{ "windowingType",  ChoiceWithDefault{ 	{"hann", "hamming", "hannnsgcq",
		"triangular", "square", "blackmanharris62", "blackmanharris70",
		"blackmanharris74", 	"blackmanharris92"}, /* default: */		"hann" 		} }
};

const std::map<juce::String, AnySpec> bfccSpecs
{
	{ "highFrequencyBound",  RangeWithDefault<double>{ {20.0,24000.0,1.0,1.0}, 8000.0 } },
	{ "lowFrequencyBound",   RangeWithDefault<double>{ {0.0,5000.0,1.0,1.0},    120.0 } },
	{ "liftering",           RangeWithDefault<int>{   {0,100,1,1},            0 } },
	{ "numBands",            RangeWithDefault<int>{   {1,128,1,1},           40 } },
	{ "numCoefficients",     RangeWithDefault<int>{   {5,26,1,1},            13 } },
	{ "normalize",           ChoiceWithDefault{ {"unit_sum", "unit_max"},     "unit_sum" } },
	{ "spectrumType",        ChoiceWithDefault{ {"magnitude", "power"},       "power"    } },
	{ "weightingType",       ChoiceWithDefault{ {"warping",  "linear"},       "warping"  } },
	{ "dctType",             ChoiceWithDefault{ {"typeII",   "typeIII"},      "typeII"   } }
};

const std::map<juce::String, AnySpec> onsetSpecs
{
	{ "silenceThreshold",         RangeWithDefault<double>{ {0.0,1.0,0.01f,1.0}, 0.1f } },
	{ "alpha",                    RangeWithDefault<double>{ {0.0,1.0,0.01f,1.0}, 0.1f } },
	{ "numFrames_shortOnsetFilter",RangeWithDefault<int>  { { 1,  64,  1,   1 },    5 } },
	{ "weight_hfc",               RangeWithDefault<double>{ {0.0,1.0,0.01f,1.0},  0.5 } },
	{ "weight_complex",           RangeWithDefault<double>{ {0.0,1.0,0.01f,1.0},  0.5 } },
	{ "weight_complexPhase",      RangeWithDefault<double>{ {0.0,1.0,0.01f,1.0},  0.5 } },
	{ "weight_flux",              RangeWithDefault<double>{ {0.0,1.0,0.01f,1.0},  0.5 } },
	{ "weight_melFlux",           RangeWithDefault<double>{ {0.0,1.0,0.01f,1.0},  0.5 } },
	{ "weight_rms",               RangeWithDefault<double>{ {0.0,1.0,0.01f,1.0},  0.5 } }
};

const std::map<juce::String, AnySpec> sBicSpecs
{
	{ "complexityPenaltyWeight", RangeWithDefault<double>{ {0.0,10.0,0.1f,1.0}, 1.5f } },
	{ "incrementFirstPass",      RangeWithDefault<int>{   {1,500,1,1},      60  } },
	{ "incrementSecondPass",     RangeWithDefault<int>{   {1,500,1,1},      20  } },
	{ "minSegmentLengthFrames",  RangeWithDefault<int>{   {1,100,1,1},      10  } },
	{ "sizeFirstPass",           RangeWithDefault<int>{   {1,1000,1,1},     300 } },
	{ "sizeSecondPass",          RangeWithDefault<int>{   {1,1000,1,1},     200 } }
};

const std::map<juce::String, AnySpec> pitchSpecs
{
	{ "pitchDetectionAlgorithm",  ChoiceWithDefault{ {"yin","pYin","chroma"}, "yin" } },
	{ "interpolate",              BoolWithDefault{ true } },
	{ "maxFrequency",             RangeWithDefault<double>{ {20.0,22050.0, 1.0, 1.0}, 4000.0 } },
	{ "minFrequency",             RangeWithDefault<double>{ {20.0,22050.0, 1.0, 1.0},  140.0 } },
	{ "tolerance",                RangeWithDefault<double>{ {0.0, 1.0,  0.001f, 1.0},   0.15 } }
};

const std::map<juce::String, AnySpec> splitSpecs
{
	{ "fadeInSamps",  RangeWithDefault<int>{ {0,10000,1,1}, 5 } },
	{ "fadeOutSamps", RangeWithDefault<int>{ {0,10000,1,1}, 5 } }
};

const std::map<juce::String, const std::map<juce::String,AnySpec>*>
	specsByBranch
{
	{ "Analysis", &analysisSpecs },
	{ "BFCC",     &bfccSpecs     },
	{ "Onset",    &onsetSpecs    },
	{ "sBic",     &sBicSpecs     },
	{ "Pitch",    &pitchSpecs    },
	{ "Split",    &splitSpecs    },
};

void ensureBranchAndInitializeDefaults (juce::ValueTree& settingsVT,
										const juce::String& branchName) {
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

void initializeSettingsBranches(juce::ValueTree& settingsVT, const bool dbg){
	for (auto& [branchName, _] : specsByBranch) {
		ensureBranchAndInitializeDefaults (settingsVT, branchName);
	}
    if constexpr (!TIMBRE_SPACE_SETTINGS_EXIST) {
        if (auto timbreSpaceSettingsTree = settingsVT.getChildWithName("TimbreSpace"); timbreSpaceSettingsTree.isValid())
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
			juce::Identifier propertyId (propertyName);
			if (!branchVT.hasProperty(propertyId)){
				return false;
			}
		}
	}

	return true;
}

bool updateSettingsFromValueTree(AnalyzerSettings& settings, const juce::ValueTree& settingsTree) {
	// Verify tree structure first
	bool const valid = verifySettingsStructure(settingsTree);
	if (!valid) {
		std::cerr << "settings tree invalid\n";
		jassertfalse;
		return false;
	}
	
	auto parent = settingsTree.getParent();
	auto const fileInfoTree = parent.getChildWithName("FileInfo");
	settings.info.sampleFilePath = fileInfoTree.getProperty("sampleFilePath").toString();
	// settings.info.author = parent.getProperty("author").toString();
	if (!fileInfoTree.hasProperty("sampleRate")) {
		std::cerr << "Parent node missing required properties\n";
		jassertfalse;
		return false;
	}
	settings.analysis.sampleRate = fileInfoTree.getProperty("sampleRate");
	jassert(0.0 < settings.analysis.sampleRate);


	auto analysisNode = settingsTree.getChildWithName("Analysis");

	if (!analysisNode.isValid()) {
		std::cerr << "Analysis node missing\n";
		jassertfalse;
		return false;
	}
	if (!analysisNode.hasProperty("frameSize") || !analysisNode.hasProperty("hopSize")
		|| !analysisNode.hasProperty("windowingType")) {
		std::cerr << "Analysis node missing required properties\n";
		jassertfalse;
		return false;
	}
	settings.analysis.frameSize = analysisNode.getProperty("frameSize");
	settings.analysis.hopSize = analysisNode.getProperty("hopSize");
	settings.analysis.windowingType = analysisNode.getProperty("windowingType").toString();
	
	// BFCC settings
	auto bfccNode = settingsTree.getChildWithName("BFCC");
	if (!bfccNode.isValid()) {
		std::cerr << "BFCC node missing\n";
		jassertfalse;
		return false;
	}
	if (!bfccNode.hasProperty("dctType") || !bfccNode.hasProperty("highFrequencyBound") ||
		!bfccNode.hasProperty("liftering") || !bfccNode.hasProperty("lowFrequencyBound") ||
		!bfccNode.hasProperty("normalize") || !bfccNode.hasProperty("numBands") ||
		!bfccNode.hasProperty("numCoefficients") || !bfccNode.hasProperty("spectrumType") ||
		!bfccNode.hasProperty("weightingType")) {
		std::cerr << "BFCC node missing required properties\n";
		jassertfalse;
		return false;
	}
	settings.bfcc.dctType = bfccNode.getProperty("dctType").toString();
	settings.bfcc.highFrequencyBound = bfccNode.getProperty("highFrequencyBound");
	settings.bfcc.liftering = bfccNode.getProperty("liftering");
	settings.bfcc.lowFrequencyBound = bfccNode.getProperty("lowFrequencyBound");
	settings.bfcc.normalize = bfccNode.getProperty("normalize").toString();
	settings.bfcc.numBands = bfccNode.getProperty("numBands");
	settings.bfcc.numCoefficients = bfccNode.getProperty("numCoefficients");
	settings.bfcc.spectrumType = bfccNode.getProperty("spectrumType").toString();
	settings.bfcc.weightingType = bfccNode.getProperty("weightingType").toString();
	
	// Onset settings
	auto onsetNode = settingsTree.getChildWithName("Onset");
	if (!onsetNode.isValid()) {
		std::cerr << "Onset node missing\n";
		jassertfalse;
		return false;
	}
	if (!onsetNode.hasProperty("alpha") || !onsetNode.hasProperty("numFrames_shortOnsetFilter") ||
		!onsetNode.hasProperty("silenceThreshold") || !onsetNode.hasProperty("weight_complex") ||
		!onsetNode.hasProperty("weight_complexPhase") || !onsetNode.hasProperty("weight_flux") ||
		!onsetNode.hasProperty("weight_hfc") || !onsetNode.hasProperty("weight_melFlux") ||
		!onsetNode.hasProperty("weight_rms")) {
		std::cerr << "Onset node missing required properties\n";
		jassertfalse;
		return false;
	}
	settings.onset.alpha = onsetNode.getProperty("alpha");
	settings.onset.numFrames_shortOnsetFilter = onsetNode.getProperty("numFrames_shortOnsetFilter");
	settings.onset.silenceThreshold = onsetNode.getProperty("silenceThreshold");
	settings.onset.weight_complex = onsetNode.getProperty("weight_complex");
	settings.onset.weight_complexPhase = onsetNode.getProperty("weight_complexPhase");
	settings.onset.weight_flux = onsetNode.getProperty("weight_flux");
	settings.onset.weight_hfc = onsetNode.getProperty("weight_hfc");
	settings.onset.weight_melFlux = onsetNode.getProperty("weight_melFlux");
	settings.onset.weight_rms = onsetNode.getProperty("weight_rms");
	
	// Pitch settings
	auto pitchNode = settingsTree.getChildWithName("Pitch");
	if (!pitchNode.isValid()) {
		std::cerr << "Pitch node missing\n";
		jassertfalse;
		return false;
	}
	if (!pitchNode.hasProperty("interpolate") || !pitchNode.hasProperty("maxFrequency") ||
		!pitchNode.hasProperty("minFrequency") || !pitchNode.hasProperty("pitchDetectionAlgorithm") ||
		!pitchNode.hasProperty("tolerance")) {
		std::cerr << "Pitch node missing required properties\n";
		jassertfalse;
		return false;
	}
	settings.pitch.interpolate = pitchNode.getProperty("interpolate");
	settings.pitch.maxFrequency = pitchNode.getProperty("maxFrequency");
	settings.pitch.minFrequency = pitchNode.getProperty("minFrequency");
	settings.pitch.pitchDetectionAlgorithm = pitchNode.getProperty("pitchDetectionAlgorithm").toString();
	settings.pitch.tolerance = pitchNode.getProperty("tolerance");
	
	// Split settings
	auto splitNode = settingsTree.getChildWithName("Split");
	if (!splitNode.isValid()) {
		std::cerr << "Split node missing\n";
		jassertfalse;
		return false;
	}
	if (!splitNode.hasProperty("fadeInSamps") || !splitNode.hasProperty("fadeOutSamps")) {
		std::cerr << "Split node missing required properties\n";
		jassertfalse;
		return false;
	}
	settings.split.fadeInSamps = splitNode.getProperty("fadeInSamps");
	settings.split.fadeOutSamps = splitNode.getProperty("fadeOutSamps");
	
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
	auto sBicNode = settingsTree.getChildWithName("sBic");
	if (!sBicNode.isValid()) {
		std::cerr << "sBic node missing\n";
		jassertfalse;
		return false;
	}
	if (!sBicNode.hasProperty("complexityPenaltyWeight") || !sBicNode.hasProperty("incrementFirstPass") ||
		!sBicNode.hasProperty("incrementSecondPass") || !sBicNode.hasProperty("minSegmentLengthFrames") ||
		!sBicNode.hasProperty("sizeFirstPass") || !sBicNode.hasProperty("sizeSecondPass")) {
		std::cerr << "sBic node missing required properties\n";
		jassertfalse;
		return false;
	}
	settings.sBic.complexityPenaltyWeight = sBicNode.getProperty("complexityPenaltyWeight");
	settings.sBic.incrementFirstPass = sBicNode.getProperty("incrementFirstPass");
	settings.sBic.incrementSecondPass = sBicNode.getProperty("incrementSecondPass");
	settings.sBic.minSegmentLengthFrames = sBicNode.getProperty("minSegmentLengthFrames");
	settings.sBic.sizeFirstPass = sBicNode.getProperty("sizeFirstPass");
	settings.sBic.sizeSecondPass = sBicNode.getProperty("sizeSecondPass");
	
	return true;
}

}	// namespace nvs::analysis
