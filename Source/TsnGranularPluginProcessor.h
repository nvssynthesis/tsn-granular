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
	struct TimbreSpaceNeededData;
public:
	//==============================================================================
	TsnGranularAudioProcessor();
	//==============================================================================
	juce::AudioProcessorEditor* createEditor() override;
	void setStateInformation (const void* data, int sizeInBytes) override;
	void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

	//==============================================================================

	void loadAudioFile(juce::File const f, bool notifyEditor) override;	// also affects analyzer
	
	
	void setWaveEvent(size_t index);					// effectively sets readBounds for all grains of all voices
	void setWaveEvents(std::array<size_t, 4> indices,
					   std::array<float, 4> weights);	// effectively sets readBounds proportionally for all contained grains, for tetrahedral timbre selection
	
	void askForAnalysis();

	nvs::nav::GUILFO &getGUILFO() {
		return gui_lfo;
	}
	
	nvs::analysis::ThreadedAnalyzer &getAnalyzer() {
		return _analyzer;
	}
	TimbreSpaceNeededData &getTimbreSpaceNeededData() {
		return timbreSpaceNeededData;
	}
	void writeEvents();
private:
	nvs::analysis::ThreadedAnalyzer _analyzer;
	
	nvs::nav::GUILFO gui_lfo;
	
	struct TimbreSpaceNeededData {
		std::vector<std::pair<float, float>> ranges {}; // min, max per dimension
		std::vector<float> histoEqualizedD0, histoEqualizedD1 {};

		std::optional<std::vector<nvs::analysis::FeatureContainer<nvs::analysis::EventwiseStatistics<float>>>>  fullTimbreSpace;	// gets stolen FROM analyzer to save memory
		std::vector<std::vector<float>> eventwiseExtractedTimbrePoints;	// gets extracted FROM this->fulltimbreSpace any time new view (e.g. different feature set) is requested
		
		void extract(std::vector<nvs::analysis::Features> featuresToExtract);
	};
	TimbreSpaceNeededData timbreSpaceNeededData;
	
	void changeListenerCallback(juce::ChangeBroadcaster*  source) override;
	//==============================================================================
	JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TsnGranularAudioProcessor)
};
