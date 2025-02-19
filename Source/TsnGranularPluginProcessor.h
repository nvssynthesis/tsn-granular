/** TODO:
	-output gain
*/
#pragma once

#include "Synthesis/TsnGranularSynth.h"
#include "Analysis/ThreadedAnalyzer.h"
#include "./Synthesis/JuceTsnGranularSynthesizer.h"
#include "../slicer_granular/Source/SlicerGranularPluginProcessor.h"
#include "./Navigation/LFO.h"

//==============================================================================

class TsnGranularAudioProcessor  : public Slicer_granularAudioProcessor
,									private juce::ChangeListener
{
public:
	//==============================================================================
	TsnGranularAudioProcessor();
	~TsnGranularAudioProcessor() override;
	//==============================================================================
	juce::AudioProcessorEditor* createEditor() override;
	void setStateInformation (const void* data, int sizeInBytes) override;
	void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

	//==============================================================================

	void loadAudioFile(juce::File const f, bool notifyEditor) override;	// also affects analyzer
	
	void askForAnalysis();
	std::vector<float> getOnsets() const;
	std::vector<std::vector<float>> getOnsetwiseTimbres() const;
	std::vector<std::vector<float>> getPCA() const;
	void writeEvents();
	
	nvs::nav::GUILFO &getGUILFO() {
		return gui_lfo;
	}
	
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
	nvs::analysis::ThreadedAnalyzer _analyzer;
	
	nvs::nav::GUILFO gui_lfo;

	void changeListenerCallback(juce::ChangeBroadcaster*  source) override;
	//==============================================================================
	JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TsnGranularAudioProcessor)
};
