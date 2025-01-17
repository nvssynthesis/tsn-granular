#include "TsnGranularPluginProcessor.h"
#include "TsnGranularPluginEditor.h"
#include "fmt/core.h"

#include "JucePluginDefines.h"

//==============================================================================
TsnGranularAudioProcessor::TsnGranularAudioProcessor()
:	//apvts(*this, nullptr, "PARAMETERS", createParameterLayout())
	tsn_granular_synth_juce(num_voices)
, 	_analyzer(this)	// effectively adding this as listener to the analyzer
//, 	logFile(juce::File::getSpecialLocation
//		  (juce::File::SpecialLocationType::currentApplicationFile)
//		  .getSiblingFile("log.txt"))
//, 	fileLogger(logFile, "hello")
{
//	juce::Logger::setCurrentLogger (&fileLogger);
//	formatManager.registerBasicFormats();
#if defined(DEBUG_BUILD) | defined(DEBUG) | defined(_DEBUG)
	writeToLog("TsnGranularAudioProcessor DEBUG MODE");
#else
	writeToLog("TsnGranularAudioProcessor RELEASE MODE\n");
#endif
}

TsnGranularAudioProcessor::~TsnGranularAudioProcessor()
{
//	fileLogger.trimFileSize(logFile , 64 * 1024);
//	juce::Logger::setCurrentLogger (nullptr);
//	formatManager.clearFormats();
}

//==============================================================================
const juce::String TsnGranularAudioProcessor::getName() const
{
	return JucePlugin_Name;
}
//
//bool TsnGranularAudioProcessor::acceptsMidi() const
//{
//   #if JucePlugin_WantsMidiInput
//	return true;
//   #else
//	return false;
//   #endif
//}
//
//bool TsnGranularAudioProcessor::producesMidi() const
//{
//   #if JucePlugin_ProducesMidiOutput
//	return true;
//   #else
//	return false;
//   #endif
//}
//
//bool TsnGranularAudioProcessor::isMidiEffect() const
//{
//   #if JucePlugin_IsMidiEffect
//	return true;
//   #else
//	return false;
//   #endif
//}
//
//double TsnGranularAudioProcessor::getTailLengthSeconds() const
//{
//	return 0.0;
//}
//
//int TsnGranularAudioProcessor::getNumPrograms()
//{
//	return 1;   // NB: some hosts don't cope very well if you tell them there are 0 programs,
//				// so this should be at least 1, even if you're not really implementing programs.
//}
//
//int TsnGranularAudioProcessor::getCurrentProgram()
//{
//	return 0;
//}
//
//void TsnGranularAudioProcessor::setCurrentProgram (int index)
//{
//}
//
//const juce::String TsnGranularAudioProcessor::getProgramName (int index)
//{
//	return {};
//}
//
//void TsnGranularAudioProcessor::changeProgramName (int index, const juce::String& newName){}

//==============================================================================
//void TsnGranularAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
//{
////	lastSampleRate = sampleRate;
////	lastSamplesPerBlock = samplesPerBlock;
//
//	tsn_granular_synth_juce.setCurrentPlaybackSampleRate (sampleRate);
//	for (int i = 0; i < tsn_granular_synth_juce.getNumVoices(); i++)
//	{
//		if (auto voice = dynamic_cast<GranularVoice*>(tsn_granular_synth_juce.getVoice(i)))
//		{
//			voice->prepareToPlay (sampleRate, samplesPerBlock);
//		}
//	}
//}

//void TsnGranularAudioProcessor::releaseResources(){}

//#ifndef JucePlugin_PreferredChannelConfigurations
//bool TsnGranularAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
//{
//  #if JucePlugin_IsMidiEffect
//	juce::ignoreUnused (layouts);
//	return true;
//  #else
//	// This is the place where you check if the layout is supported.
//	// In this template code we only support mono or stereo.
//	// Some plugin hosts, such as certain GarageBand versions, will only
//	// load plugins that support stereo bus layouts.
//	if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::mono()
//	 && layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo())
//		return false;
//
//	// This checks if the input layout matches the output layout
//   #if ! JucePlugin_IsSynth
//	if (layouts.getMainOutputChannelSet() != layouts.getMainInputChannelSet())
//		return false;
//   #endif
//
//	return true;
//  #endif
//}
//#endif

//void TsnGranularAudioProcessor::writeToLog(std::string const &s){
//	fileLogger.writeToLog (s);
//}
void TsnGranularAudioProcessor::loadAudioFile(juce::File const f, bool notifyEditor){
	// juce::AudioFormatReader *reader = formatManager.createReaderFor(f);
	// if (!reader){
	// 	std::cerr << "could not read file: " << f.getFileName() << "\n";
	// 	return;
	// }
	// int newLength = static_cast<int>(reader->lengthInSamples);
	Slicer_granularAudioProcessor::loadAudioFile(f, notifyEditor);
	double const sr = sampleManagementGuts.lastFileSampleRate;
	{	// limit tmp scope
		auto anSettingsTmp = _analyzer.getAnalysisSettings();
		anSettingsTmp.sampleRate = static_cast<float>(sr);
		_analyzer.setAnalysisSettings(anSettingsTmp);
	}
}
void TsnGranularAudioProcessor::askForAnalysis(){
	auto const buffer = sampleManagementGuts.sampleBuffer;
	auto const waveSpan = std::span<float const>(buffer.getReadPointer(0), buffer.getNumSamples());
	
	_analyzer.updateWave(waveSpan);
	if (_analyzer.startThread(juce::Thread::Priority::normal)){
		fmt::print("analyzer onset thread started\n");
	}
}

std::vector<float> TsnGranularAudioProcessor::getOnsets() const {
	return _analyzer.getOnsetsInSeconds();
}

std::vector<std::vector<float>> TsnGranularAudioProcessor::getOnsetwiseBFCCs() const {
	return _analyzer.getOnsetwiseBFCCs();
}

std::vector<std::vector<float>> TsnGranularAudioProcessor::getPCA() const {
	return _analyzer.getPCA();
}

void TsnGranularAudioProcessor::writeEvents(){
	auto const buffer = sampleManagementGuts.sampleBuffer;
	auto const waveSpan = std::span<float const>(buffer.getReadPointer(0), buffer.getNumSamples());
	std::vector<float> wave(waveSpan.size());
	wave.assign(waveSpan.begin(), waveSpan.end());
	
	// any reason to use getPropertyAsValue instead?
	juce::String audioFilePath = apvts.state.getProperty(sampleManagementGuts.audioFilePathValueTreeStateIdentifier);
	nvs::analysis::writeEventsToWav(wave, _analyzer.getOnsetsInSeconds(), audioFilePath.toStdString(), _analyzer.getAnalyzer());
}


//void TsnGranularAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
//{
//	juce::ScopedNoDenormals noDenormals;
//	auto totalNumInputChannels  = getTotalNumInputChannels();
//	auto totalNumOutputChannels = getTotalNumOutputChannels();
//
//	for (auto i = totalNumInputChannels; i < totalNumOutputChannels; ++i){
//		buffer.clear (i, 0, buffer.getNumSamples());
//	}
//
//	tsn_granular_synth_juce.granularMainParamSet<0, num_voices>(apvts);	// this just sets the params internal to the granular synth (effectively a voice)
//	tsn_granular_synth_juce.envelopeParamSet<0, num_voices>(apvts);
//
//	if ( !(audioBuffersChannels.getActiveSpanRef().size()) ){
//		return;
//	}
//
//	tsn_granular_synth_juce.renderNextBlock(buffer,
//						  midiMessages,
//						  0,
//						  buffer.getNumSamples());
//	// apply gain based on normalizationValue
//	// limit with jlimit?
//
//	const auto rms_val = rms.query();
//	rmsInformant.val = rms_val;
//	rmsWAinformant.val = weightAvg(rms_val);
//}

//==============================================================================
//bool TsnGranularAudioProcessor::hasEditor() const
//{
//	return true; // (change this to false if you choose to not supply an editor)
//}
//
//juce::AudioProcessorEditor* TsnGranularAudioProcessor::createEditor()
//{
//	TsnGranularAudioProcessorEditor* ed = new TsnGranularAudioProcessorEditor (*this);
//	_analyzer.addChangeListener(ed);
//	return ed;
//}
//
////==============================================================================
//void TsnGranularAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
//{
//	// You should use this method to store your parameters in the memory block.
//	// You could do that either as raw data, or use the XML or ValueTree classes
//	// as intermediaries to make it easy to save and load complex data.
//}
//
//void TsnGranularAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
//{
//	// You should use this method to restore your parameters from this memory block,
//	// whose contents will have been created by the getStateInformation() call.
//}

void TsnGranularAudioProcessor::changeListenerCallback(juce::ChangeBroadcaster* source) {
	fmt::print("processor: change message received\n");
	if (&_analyzer == dynamic_cast<nvs::analysis::ThreadedAnalyzer*>(source)){
		fmt::print("processor: dynamic cast to threaded analyzer successful\n");
		// now we can simply check on our own analyzer, don't even need to use source qua source
		// then load onsets into synth
		auto onsets = _analyzer.getOnsetsInSeconds();
		if (onsets.size()){
			tsn_granular_synth_juce.loadOnsets(onsets);
		}
		fmt::print("processor: change listener callback: got things\n");
	}
}

//juce::AudioProcessorValueTreeState::ParameterLayout TsnGranularAudioProcessor::createParameterLayout(){
//#if defined(DEBUG_BUILD) | defined(DEBUG) | defined(_DEBUG)
//	fmt::print("createParamLayout\n");
//#endif
//
//	juce::AudioProcessorValueTreeState::ParameterLayout layout;
//	auto mainGranularParams = std::make_unique<juce::AudioProcessorParameterGroup>("Gran", "MainGranularParams", "|");
//
//	auto stringFromValue = [&](float value, int maximumStringLength){
//		return juce::String (value, 2);	//getNumDecimalPlacesToDisplay()
//	};
//
//	auto a = [&](params_e p){
//		auto tup = paramMap.at(p);
//		auto name = std::get<static_cast<size_t>(param_elem_e::name)>(tup);
//		juce::NormalisableRange<float> range = getNormalizableRange<float>(p);
//		auto defaultVal = getParamDefault(p);
//
//		return std::make_unique<juce::AudioParameterFloat>(juce::ParameterID(name,  1),	//	parameterID
//												   name,		// parameterName
//												   range,		// NormalizableRange
//												   defaultVal,	// defaultValue
//												   name,		// parameterLabel
//												   juce::AudioProcessorParameter::genericParameter,	// category
//												   stringFromValue,		// stringFromValue
//												   nullptr);	// valueFromString
//	};
//
//	for (size_t i = 0; i < static_cast<size_t>(params_e::count_main_granular_params); ++i){
//		params_e param = static_cast<params_e>(i);
//		mainGranularParams->addChild(a(param));
//	}
//	layout.add(std::move(mainGranularParams));
//
//	auto envelopeParams = std::make_unique<juce::AudioProcessorParameterGroup>("Env", "EnvelopeParams", "|");
//
//	for (size_t i = static_cast<size_t>(params_e::count_main_granular_params) + 1;
//		 i < static_cast<size_t>(params_e::count_envelope_params);
//		 ++i){
//		params_e param = static_cast<params_e>(i);
//		envelopeParams->addChild(a(param));
//	}
//	layout.add(std::move(envelopeParams));
//	return layout;
//}


//==============================================================================
// This creates new instances of the plugin..
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
	return new TsnGranularAudioProcessor();
}
