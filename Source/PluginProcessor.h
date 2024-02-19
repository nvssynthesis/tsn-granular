/** TODO:
	-output gain

 */
#pragma once

#ifndef FROZEN_MAP
#define FROZEN_MAP 1
#endif

#include <JuceHeader.h>
#include "Synthesis/TsaraGranularSynth.h"
#include "Analysis/ThreadedAnalyzer.h"
#include "AudioBuffersChannels.h"
#include "dsp_util.h"
#include "params.h"

#if FROZEN_MAP
	#include "frozen/map.h"
	#define STATIC_MAP 0
	#define MAP frozen::map
#endif

#if STATIC_MAP
	#include "DataStructures.h"
	#define MAP StaticMap
#endif

//==============================================================================

class TsaraGranularAudioProcessor  : public juce::AudioProcessor
							#if JucePlugin_Enable_ARA
							 , public juce::AudioProcessorARAExtension
							#endif
,									private juce::ChangeListener
{
public:
	//==============================================================================
	TsaraGranularAudioProcessor();
	~TsaraGranularAudioProcessor() override;
	
	//==============================================================================
	void prepareToPlay (double sampleRate, int samplesPerBlock) override;
	void releaseResources() override;
	
#ifndef JucePlugin_PreferredChannelConfigurations
	bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
#endif
	
	void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
	
	//==============================================================================
	juce::AudioProcessorEditor* createEditor() override;
	bool hasEditor() const override;
	
	//==============================================================================
	const juce::String getName() const override;
	
	bool acceptsMidi() const override;
	bool producesMidi() const override;
	bool isMidiEffect() const override;
	double getTailLengthSeconds() const override;
	
	//==============================================================================
	int getNumPrograms() override;
	int getCurrentProgram() override;
	void setCurrentProgram (int index) override;
	const juce::String getProgramName (int index) override;
	void changeProgramName (int index, const juce::String& newName) override;
	
	//==============================================================================
	void getStateInformation (juce::MemoryBlock& destData) override;
	void setStateInformation (const void* data, int sizeInBytes) override;
	
	juce::AudioProcessorValueTreeState apvts;
	
	void writeToLog(std::string const &s);
	void loadAudioFile(juce::File const f, juce::AudioThumbnail *const thumbnail);
	
	void askForAnalysis();
	std::optional<std::vector<float>> getOnsets() const;
	std::optional<std::vector<std::vector<float>>> getOnsetwiseBFCCs() const;
	std::optional<std::vector<std::vector<float>>> getPCA() const;
	void writeEvents();
	
	bool triggerValFromEditor {false};

	template<typename T>
	struct editorInformant{
		T val;
	};
	editorInformant<float> rmsInformant;
	editorInformant<float> rmsWAinformant;
	
	juce::AudioFormatManager &getAudioFormatManager(){
		return formatManager;
	}
	
	size_t getCurrentWaveSize()  {
		return audioBuffersChannels.getActiveSpanRef().size();
	}
	std::span<float> const &getActiveSpanRef()  {
		return audioBuffersChannels.getActiveSpanRef();
	}
	void setOnsetSettings(nvs::analysis::onsetSettings settings){
		_analyzer.setOnsetSettings(settings);
	}
	nvs::analysis::analysisSettings getAnalysisSettings() const {
		return _analyzer.getAnalysisSettings();
	}
private:
	
	AudioBuffersChannels audioBuffersChannels;
	
	nvs::gran::TsaraGranular tsara_granular;
	nvs::analysis::ThreadedAnalyzer _analyzer;
	
	struct Features {
		std::optional<std::vector<float>> onsetsInSeconds;
		std::optional<std::vector<std::vector<float>>> onsetwiseBFCCs;
		std::optional<std::vector<std::vector<float>>> PCA;
	};
	Features _feat;
	void changeListenerCallback(juce::ChangeBroadcaster*  source) override;
//	void loadOnsetsIntoSynth();
	
	float lastTranspose 	{getParamDefault(params_e::transpose)};
	float lastPosition 		{getParamDefault(params_e::position)};
	float lastSpeed 		{getParamDefault(params_e::speed)};
	float lastDuration 		{getParamDefault(params_e::duration)};
	float lastSkew 			{getParamDefault(params_e::skew)};
	float lastPlateau 		{getParamDefault(params_e::plateau)};
	float lastPan 			{getParamDefault(params_e::pan)};

	float lastTransposeRand {getParamDefault(params_e::transp_randomness)};
	float lastPositionRand 	{getParamDefault(params_e::pos_randomness)};
	float lastSpeedRand 	{getParamDefault(params_e::speed_randomness)};
	float lastDurationRand 	{getParamDefault(params_e::dur_randomness)};
	float lastSkewRand 		{getParamDefault(params_e::skew_randomness)};
	float lastPlateauRand	{getParamDefault(params_e::plat_randomness)};
	float lastPanRand 		{getParamDefault(params_e::pan_randomness)};
	
#if (STATIC_MAP | FROZEN_MAP)
	using granMembrSetFunc = void(nvs::gran::genGranPoly1::*) (float);

	static constexpr inline MAP<params_e, granMembrSetFunc, static_cast<size_t>(params_e::count)>
	paramSetterMap {
		std::make_pair<params_e, granMembrSetFunc>(params_e::transpose, 		&nvs::gran::TsaraGranular::setTranspose),
		std::make_pair<params_e, granMembrSetFunc>(params_e::position, 			&nvs::gran::TsaraGranular::setPosition),
		std::make_pair<params_e, granMembrSetFunc>(params_e::speed, 			&nvs::gran::TsaraGranular::setSpeed),
		std::make_pair<params_e, granMembrSetFunc>(params_e::duration, 			&nvs::gran::TsaraGranular::setDuration),
		std::make_pair<params_e, granMembrSetFunc>(params_e::skew, 				&nvs::gran::TsaraGranular::setSkew),
		std::make_pair<params_e, granMembrSetFunc>(params_e::plateau,			&nvs::gran::TsaraGranular::setPlateau),
		std::make_pair<params_e, granMembrSetFunc>(params_e::pan, 				&nvs::gran::TsaraGranular::setPan),
		std::make_pair<params_e, granMembrSetFunc>(params_e::transp_randomness, &nvs::gran::TsaraGranular::setTransposeRandomness),
		std::make_pair<params_e, granMembrSetFunc>(params_e::pos_randomness, 	&nvs::gran::TsaraGranular::setPositionRandomness),
		std::make_pair<params_e, granMembrSetFunc>(params_e::speed_randomness, 	&nvs::gran::TsaraGranular::setSpeedRandomness),
		std::make_pair<params_e, granMembrSetFunc>(params_e::dur_randomness, 	&nvs::gran::TsaraGranular::setDurationRandomness),
		std::make_pair<params_e, granMembrSetFunc>(params_e::skew_randomness, 	&nvs::gran::TsaraGranular::setSkewRandomness),
		std::make_pair<params_e, granMembrSetFunc>(params_e::plat_randomness,	&nvs::gran::TsaraGranular::setPlateauRandomness),
		std::make_pair<params_e, granMembrSetFunc>(params_e::pan_randomness, 	&nvs::gran::TsaraGranular::setPanRandomness)
	};
	template <auto Start, auto End>
	constexpr void paramSet();
	
	MAP<params_e, float *, static_cast<size_t>(params_e::count)>
	lastParamsMap{
		std::make_pair<params_e, float *>(params_e::transpose, 	&lastTranspose),
		std::make_pair<params_e, float *>(params_e::position, 	&lastPosition),
		std::make_pair<params_e, float *>(params_e::speed, 		&lastSpeed),
		std::make_pair<params_e, float *>(params_e::duration, 	&lastDuration),
		std::make_pair<params_e, float *>(params_e::skew, 		&lastSkew),
		std::make_pair<params_e, float *>(params_e::plateau,	&lastPlateau),
		std::make_pair<params_e, float *>(params_e::pan, 		&lastPan),
		std::make_pair<params_e, float *>(params_e::transp_randomness, 	&lastTransposeRand),
		std::make_pair<params_e, float *>(params_e::pos_randomness, 	&lastPositionRand),
		std::make_pair<params_e, float *>(params_e::speed_randomness, 	&lastSpeedRand),
		std::make_pair<params_e, float *>(params_e::dur_randomness, 	&lastDurationRand),
		std::make_pair<params_e, float *>(params_e::skew_randomness, 	&lastSkewRand),
		std::make_pair<params_e, float *>(params_e::plat_randomness, 	&lastPlateauRand),
		std::make_pair<params_e, float *>(params_e::pan_randomness, 	&lastPanRand)
	};
#endif
	double lastSampleRate 	{ 0.0 };
	int lastSamplesPerBlock { 0 };
	
	static constexpr int N_GRAINS =
#if defined(DEBUG_BUILD) | defined(DEBUG) | defined(_DEBUG)
									50;
#else
									100;
#endif
	
	float normalizationValue {1.f};	// a MULTIPLIER for the overall output, based on the inverse of the absolute max value for the current sample

	nvs::util::RMS<float> rms;
	nvs::util::WeightedAveragingBuffer<float, 3> weightAvg;
	
	juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();
	juce::AudioFormatManager formatManager;
	std::string currentFile;

	//======logging=======================
	juce::File logFile;
	juce::FileLogger fileLogger;
	//==============================================================================
	JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TsaraGranularAudioProcessor)
};
