/** TODO:
	-output gain
*/
#pragma once

#include "./Analysis/ThreadedAnalyzer.h"

#include "./Synthesis/TSNPolyGrain.h"
#include "./Synthesis/TSNGranularSynthesizer.h"
#include "./TimbreSpace/TimbreSpaceHolder.h"

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
	class TimbreSpaceNeededData;
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
	TimbreSpaceNeededData &getTimbreSpaceNeededData() {
		return _timbreSpaceNeededData;
	}
	nvs::timbrespace::TimbreSpaceHolder &getTimbreSpaceHolder() {
		return _timbreSpaceHolder;
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
	struct TimbreSpacialSettings {
		float histogramEqualization {0.0f};
		std::vector<nvs::analysis::Features> dimensionWisefeatures {
			nvs::analysis::Features::bfcc1,
			nvs::analysis::Features::bfcc2,
			nvs::analysis::Features::bfcc3,
			nvs::analysis::Features::bfcc4,
			nvs::analysis::Features::bfcc5
		};
	};
	class TimbreSpaceNeededData	:	public juce::ChangeListener,	public juce::ValueTree::Listener,	public juce::ChangeBroadcaster
	{
	public:
		TimbreSpaceNeededData(TimbreSpacialSettings &settings)	:	spacialSettings(settings)	{}
		
		
		std::vector<std::pair<float, float>> ranges {}; // min, max per dimension
		std::vector<float> histoEqualizedD0, histoEqualizedD1 {};

		std::optional<std::vector<nvs::analysis::FeatureContainer<nvs::analysis::EventwiseStatistics<float>>>>  fullTimbreSpace;	// gets stolen FROM analyzer to save memory
		std::vector<std::vector<float>> eventwiseExtractedTimbrePoints;	// gets extracted FROM this->fulltimbreSpace any time new view (e.g. different feature set) is requested

		TimbreSpacialSettings &spacialSettings;
		void valueTreePropertyChanged (juce::ValueTree &alteredTree, const juce::Identifier &property) override;
		void changeListenerCallback(juce::ChangeBroadcaster *source) override;
	private:
		
		void updateTimbreSpacePoints();
		void extract(std::vector<nvs::analysis::Features> featuresToExtract);
	};
	void reshapeTimbreSpacePoints(bool verbose);
	TimbreSpacialSettings _timbreSpacialSettings;
	
	TimbreSpaceNeededData _timbreSpaceNeededData;
	
	nvs::timbrespace::TimbreSpaceHolder _timbreSpaceHolder;
	
	//========================================================================================================================

	
	void changeListenerCallback(juce::ChangeBroadcaster*  source) override;
	//==============================================================================
	JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TSNGranularAudioProcessor)
};
