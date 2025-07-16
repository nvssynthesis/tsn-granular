/** TODO:
	-output gain
*/
#pragma once

#include "./Analysis/ThreadedAnalyzer.h"

#include "./Synthesis/TSNPolyGrain.h"
#include "./Synthesis/TSNGranularSynthesizer.h"
#include "./TimbreSpace/TimbreSpace.h"

#include "../slicer_granular/Source/SlicerGranularPluginProcessor.h"
#include "./Navigation/LFO.h"
#include "../slicer_granular/Source/misc_util.h"

//==============================================================================

class TSNGranularAudioProcessor  :	public SlicerGranularAudioProcessor
{
	friend class SlicerGranularAudioProcessor;	// allow base class to access private ctor
public:
	//==============================================================================
	~TSNGranularAudioProcessor();
	//==============================================================================
	juce::AudioProcessorEditor* createEditor() override;
	void setStateInformation (const void* data, int sizeInBytes) override;
	void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

	//==============================================================================
	void loadAudioFile(juce::File const f, bool notifyEditor) override;	// also affects analyzer
	
	void setReadBoundsFromChosenPoint();
	
	void askForAnalysis();

	nvs::nav::Navigator &getNavigator() {
		return navigator;
	}
	nvs::analysis::ThreadedAnalyzer &getAnalyzer() {
		return _analyzer;
	}
	
	using TimbreSpace = nvs::timbrespace::TimbreSpace;
	
	nvs::timbrespace::TimbreSpace &getTimbreSpace() {
		return _timbreSpace;
	}
	TSNGranularSynthesizer *getTsnGranularSynthesizer() {
		if (TSNGranularSynthesizer *synth = dynamic_cast<TSNGranularSynthesizer *>(_granularSynth.get())){
			return synth;
		}
		else {
			jassertfalse;
			return nullptr;
		}
	}
	
	void writeEvents();
	
//	void actionListenerCallback(juce::String const &message) override;
	void saveAnalysisToFile(const juce::String& filePath, std::function<void(bool)> resultCallback);
	
protected:
	void initSynth() override;
private:
	TSNGranularAudioProcessor();

	nvs::analysis::ThreadedAnalyzer _analyzer;
	
	nvs::nav::Navigator navigator;
	
	//========================================================================================================================

	TimbreSpace _timbreSpace;
	
	//========================================================================================================================
	void changeListenerCallback(juce::ChangeBroadcaster*  source) override;
	//==============================================================================
	JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TSNGranularAudioProcessor)
};
