/** TODO:
	-output gain
*/
#pragma once

#include "./Analysis/ThreadedAnalyzer.h"

#include "./Synthesis/TSNPolyGrain.h"
#include "./Synthesis/TSNGranularSynthesizer.h"
#include "./TimbreSpace/TimbreSpace.h"

#include "../slicer_granular/Source/SlicerGranularPluginProcessor.h"
#include "../slicer_granular/Source/misc_util.h"
#include "./Navigation/Navigator.h"

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
	void loadAudioFileAndUpdateState(juce::File const f, bool notifyEditor) override;	// also affects analyzer
	void askForAnalysis();
	//==============================================================================
	using TimbreSpace = nvs::timbrespace::TimbreSpace;
	using ThreadedAnalyzer = nvs::analysis::ThreadedAnalyzer;
    using TSNGranularSynth = nvs::gran::TSNGranularSynthesizer;

	ThreadedAnalyzer &getAnalyzer() {
		return _analyzer;
	}
	TimbreSpace &getTimbreSpace() { return _tsnGranularSynth->getTimbreSpace(); }
    TSNGranularSynth *getTsnGranularSynthesizer() const {
	    jassert(_tsnGranularSynth);
		return _tsnGranularSynth;
	}
	//==============================================================================
	void saveAnalysisToFile(const juce::String& filePath, std::function<void(bool)> resultCallback) const;
	void writeEvents();
	//==============================================================================
protected:
	void initSynth() override;
private:
	TSNGranularAudioProcessor();
	//==============================================================================
	ThreadedAnalyzer _analyzer;
    TSNGranularSynth * _tsnGranularSynth {nullptr};    // gets initialized from subclass's _granularSynth unique_ptr
	//==============================================================================
	void ensureSettingsStructure();
	bool loadAnalysisFileFromState();
	//==============================================================================

	JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TSNGranularAudioProcessor)
};
