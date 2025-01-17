/** TODO:
	-output gain
*/
#pragma once

#include "Synthesis/TsnGranularSynth.h"
#include "Analysis/ThreadedAnalyzer.h"
#include "./Synthesis/JuceTsnGranularSynthesizer.h"
#include "../slicer_granular/Source/SlicerGranularPluginProcessor.h"

//==============================================================================

class TsnGranularAudioProcessor  : public Slicer_granularAudioProcessor
,									private juce::ChangeListener
{
public:
	//==============================================================================
	TsnGranularAudioProcessor();
	~TsnGranularAudioProcessor() override;
	//==============================================================================
	const juce::String getName() const override;
	
	void loadAudioFile(juce::File const f, bool notifyEditor) override;	// also affects analyzer
	
	void askForAnalysis();
	std::vector<float> getOnsets() const;
	std::vector<std::vector<float>> getOnsetwiseBFCCs() const;
	std::vector<std::vector<float>> getPCA() const;
	void writeEvents();
	
	nvs::analysis::onsetSettings getOnsetSettings(){
		return _analyzer.getOnsetSettings();
	}
	void setOnsetSettings(nvs::analysis::onsetSettings settings){
		_analyzer.setOnsetSettings(settings);
	}
	nvs::analysis::analysisSettings getAnalysisSettings() const {
		return _analyzer.getAnalysisSettings();
	}
	void setAnalysisSettings(nvs::analysis::analysisSettings settings){
		_analyzer.setAnalysisSettings(settings);
	}
private:
	JuceTsnGranularSynthesizer tsn_granular_synth_juce;
	constexpr static int num_voices =
#if defined(DEBUG_BUILD) | defined(DEBUG) | defined(_DEBUG)
										6;
#else
										16;
#endif
	nvs::analysis::ThreadedAnalyzer _analyzer;
	
	struct Features {
		std::optional<std::vector<float>> onsetsInSeconds;
		std::optional<std::vector<std::vector<float>>> onsetwiseBFCCs;
		std::optional<std::vector<std::vector<float>>> PCA;
	};
	Features _feat;
	void changeListenerCallback(juce::ChangeBroadcaster*  source) override;
	//==============================================================================
	JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TsnGranularAudioProcessor)
};
