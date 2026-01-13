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

class TSNGranularAudioProcessor final
:	public SlicerGranularAudioProcessor
{
	friend class SlicerGranularAudioProcessor;	// allow base class to access private ctor
public:
	//==============================================================================
    // AudioProcessor
	~TSNGranularAudioProcessor() override;
	juce::AudioProcessorEditor* createEditor() override;
	void setStateInformation (const void* data, int sizeInBytes) override;
	void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    //==============================================================================
    void changeListenerCallback (juce::ChangeBroadcaster *source) override;
    //==============================================================================
    // SlicerGranularAudioProcessor
	void loadAudioFileAndUpdateState(juce::File const f, bool notifyEditor) override;	// also affects analyzer
	void askForAnalysis();
	//==============================================================================
	using TimbreSpace = nvs::timbrespace::TimbreSpace;
    using TimbreSpacePointSelector = nvs::timbrespace::TimbreSpacePointSelector;
	using ThreadedAnalyzer = nvs::analysis::ThreadedAnalyzer;
    using TSNGranularSynth = nvs::gran::TSNGranularSynthesizer;

	ThreadedAnalyzer &getAnalyzer() {
		return _analyzer;
	}
	[[deprecated("theory: the only valid reasons to get timbreSpace from here would be saving, writing, and validation. create helper methods instead.")]]
    TimbreSpace &getTimbreSpace() const { return _tsnGranularSynth->getTimbreSpace(); }
    TimbreSpacePointSelector &getTimbreSpacePointSelector() const { return _tsnGranularSynth->getTimbreSpacePointSelector(); }

    TSNGranularSynth *getTsnGranularSynthesizer() const {
	    jassert(_tsnGranularSynth);
		return _tsnGranularSynth;
	}
	//==============================================================================
	void saveAnalysisToFile(const juce::String& filePath, std::function<void(bool)> resultCallback) const;
	void writeEvents();
	//==============================================================================
protected:
    // SlicerGranularAudioProcessor
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
