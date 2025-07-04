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

struct timbre5DPoint {
	using timbre2DPoint = juce::Point<float>;
	using timbre3DPoint = std::array<float, 3>;
	timbre2DPoint _p2D;			// used to locate the point in x,y plane
	timbre3DPoint _p3D;	// used to describe the colour (hsv)

	bool operator==(timbre5DPoint const &other) const;
	timbre2DPoint get2D() const { return _p2D; }
	timbre3DPoint get3D() const { return _p3D; }

	// to easily trade hsv for rbg
	std::array<juce::uint8, 3> toUnsigned() const;
};

//==============================================================================

class TSNGranularAudioProcessor  : public SlicerGranularAudioProcessor
,									private juce::ChangeListener
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
protected:
	void initSynth() override {
		_granularSynth = std::make_unique<TSNGranularSynthesizer>(apvts);
		if (dynamic_cast<TSNGranularSynthesizer *>(_granularSynth.get())){
			writeToLog("dynamic cast to JuceTsnGranularSynthesizer successful");
		}
		else if (_granularSynth.get() == nullptr){
			writeToLog("Null JuceTsnGranularSynthesizer");
			jassertfalse;
		}
		else {
			writeToLog("Failed to dynamic cast JuceTsnGranularSynthesizer");
			jassertfalse;
		}
	}
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
