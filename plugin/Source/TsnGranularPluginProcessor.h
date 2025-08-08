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
,									private juce::ActionListener
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
	//==============================================================================
	using TimbreSpace = nvs::timbrespace::TimbreSpace;
	using ThreadedAnalyzer = nvs::analysis::ThreadedAnalyzer;
	using Navigator = nvs::nav::Navigator;
	Navigator &getNavigator() {
		return _navigator;
	}
	ThreadedAnalyzer &getAnalyzer() {
		return _analyzer;
	}
	TimbreSpace &getTimbreSpace() {
		return _timbreSpace;
	}
	TSNGranularSynthesizer *getTsnGranularSynthesizer() {
		if (auto *synth = dynamic_cast<TSNGranularSynthesizer *>(_granularSynth.get())){ return synth; }
		else { jassertfalse; return nullptr; }
	}
	//==============================================================================
	void saveAnalysisToFile(const juce::String& filePath, std::function<void(bool)> resultCallback);
	void writeEvents();
	//==============================================================================
protected:
	void initSynth() override;
private:
	TSNGranularAudioProcessor();
	//==============================================================================
	ThreadedAnalyzer _analyzer;
	Navigator _navigator;
	TimbreSpace _timbreSpace;
	//==============================================================================
	void actionListenerCallback(juce::String const &message) override;
	//==============================================================================
	void ensureSettingsStructure() {
		auto settings = apvts.state.getChildWithName("Settings");
		if (!nvs::analysis::verifySettingsStructure(settings)) {
			juce::ValueTree settingsVT = apvts.state.getOrCreateChildWithName("Settings", nullptr);
			nvs::analysis::initializeSettingsBranches(settingsVT);
		}
	}

	bool loadAnalysisFileFromState()
	// TODO: Return more particular failure/success and handle each case. E.g. there could be auto-search in designated directory, manual find, and give up (just perform fresh analysis)
	{
		auto fileInfo = apvts.state.getChildWithName("FileInfo");
		if (!fileInfo.isValid()) return false;
		
		juce::String analysisFilePath = fileInfo.getProperty("analysisFile", {});
		if (analysisFilePath.isEmpty()) return false;
		
		juce::File file(analysisFilePath);
		auto fstream = juce::FileInputStream(file);
		
		if (fstream.failedToOpen()) {
			writeToLog(fstream.getStatus().getErrorMessage());
			// TODO: Give popup opportunity for user to find the file
			return false;
		}
		
		auto analysisFileStream = juce::ValueTree::readFromStream(fstream);
		auto analysisFileTree = analysisFileStream.getChildWithName("TimbreAnalysis");
		
		if (analysisFileTree.isValid()) {
			fmt::print("setting via setStateInformation\n");
			_timbreSpace.setTimbreSpaceTree(analysisFileTree);
		}
		return true;
	}
	//==============================================================================

	JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TSNGranularAudioProcessor)
};
