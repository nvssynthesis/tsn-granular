#include "TsnGranularPluginProcessor.h"
#include "TsnGranularPluginEditor.h"
#include "fmt/core.h"
#include "Analysis/Settings.h"
#include "JucePluginDefines.h"

//==============================================================================
nvs::util::LoggingGuts::LoggingGuts()
: logFile(juce::File::getSpecialLocation(juce::File::SpecialLocationType::currentApplicationFile).getSiblingFile("log.txt"))
, fileLogger(logFile, "tsn_granular logging")
{
	juce::Logger::setCurrentLogger (&fileLogger);
}

TsnGranularAudioProcessor::TsnGranularAudioProcessor()
:	Slicer_granularAudioProcessor(std::make_unique<JuceTsnGranularSynthesizer>())
,	_analyzer(this)			// effectively adding this as listener to the analyzer
,	gui_lfo(apvts, 20.0)	// processor owns it, but editor facilitates communication from lfo > timbre space
{
	if (dynamic_cast<JuceTsnGranularSynthesizer *>(_granularSynth.get())){
		writeToLog("dynamic cast to JuceTsnGranularSynthesizer successful");
	}
	else {
		writeToLog("Failed to dynamic cast JuceTsnGranularSynthesizer");
	}
#if defined(DEBUG_BUILD) | defined(DEBUG) | defined(_DEBUG)
	writeToLog("TsnGranularAudioProcessor DEBUG MODE");
#else
	writeToLog("TsnGranularAudioProcessor RELEASE MODE\n");
#endif
	
	juce::ValueTree settingsVT = nonAutomatableState.getOrCreateChildWithName("Settings", nullptr);
	nvs::analysis::initializeSettingsBranches(settingsVT);
}

TsnGranularAudioProcessor::~TsnGranularAudioProcessor()
{}

//==============================================================================
juce::AudioProcessorEditor* TsnGranularAudioProcessor::createEditor() {
	TsnGranularAudioProcessorEditor* ed = new TsnGranularAudioProcessorEditor (*this);
	_analyzer.addChangeListener(ed);
	return ed;
}

void TsnGranularAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
	Slicer_granularAudioProcessor::setStateInformation(data, sizeInBytes);	// loads preset info & synthesis parameters
	
	std::unique_ptr<juce::XmlElement> xmlState (getXmlFromBinary (data, sizeInBytes));

	if (xmlState == nullptr || ! xmlState->hasTagName ("PluginState")){
		return;
	}
	
	juce::ValueTree root = juce::ValueTree::fromXml (*xmlState);

	juce::ValueTree nonAuto = root.getChildWithName ("NonAutomatable");
	juce::ValueTree settings = nonAuto.getChildWithName("Settings");
	if (!nvs::analysis::verifySettingsStructure(settings)){
		writeToLog("In setStateInformation: Settings tree invalid. Not overwriting constructed default settings.\n");
		return;
	}
	nonAutomatableState = nonAuto;
}
//==============================================================================

void TsnGranularAudioProcessor::loadAudioFile(juce::File const f, bool notifyEditor){
	writeToLog("TSN: loadAudioFile\n");
	// this used to have just copied and pasted code from slicer. it seems to work properly simply by manually calling the base function like so:
	Slicer_granularAudioProcessor::loadAudioFile(f, notifyEditor);
	askForAnalysis();
	writeToLog("TSN: loadAudioFile exiting");
}

void TsnGranularAudioProcessor::setWaveEvent(size_t index)
{
	if (auto *const tsn_synth_juce = dynamic_cast<JuceTsnGranularSynthesizer *const>(_granularSynth.get())){
		tsn_synth_juce->setWaveEvent(index);
	}
}

void TsnGranularAudioProcessor::setWaveEvents(std::array<size_t, 4> indices, std::array<float, 4> weights)
{
	if (auto *const tsn_synth_juce = dynamic_cast<JuceTsnGranularSynthesizer *const>(_granularSynth.get())){
		tsn_synth_juce->setWaveEvents(indices, weights);
	}
}

void TsnGranularAudioProcessor::askForAnalysis(){
	auto const buffer = sampleManagementGuts.sampleBuffer;
	if (!buffer.getNumChannels()){
		writeToLog("TSN: askForAnalysis: buffer had no channels. Early exit.");
		return;
	}
	if (!buffer.getNumSamples()){
		writeToLog("TSN: askForAnalysis: buffer had no samples. Early exit.");
		return;
	}
	_analyzer.updateWave(std::span<float const>(buffer.getReadPointer(0), buffer.getNumSamples()),
						 getSampleFilePath().hash());
	_analyzer.updateSettings(nonAutomatableState.getChildWithName("Settings"));
	if (_analyzer.startThread(juce::Thread::Priority::high)){
		writeToLog("analyzer onset thread started");
	}
}

void TsnGranularAudioProcessor::writeEvents(){
	auto const buffer = sampleManagementGuts.sampleBuffer;
	auto const waveSpan = std::span<float const>(buffer.getReadPointer(0), buffer.getNumSamples());
	std::vector<float> wave(waveSpan.size());
	wave.assign(waveSpan.begin(), waveSpan.end());
	
	// any reason to use getPropertyAsValue instead?
	juce::String audioFilePath = nonAutomatableState.getChildWithName("PresetInfo").getProperty("sampleFilePath");
	float sr = 	nonAutomatableState.getChildWithName("PresetInfo").getProperty("sampleRate");

	std::vector<float> onsetsTmp = _analyzer.getOnsets();
	auto analyzer = _analyzer.getAnalyzer();

	nvs::analysis::denormalizeOnsets(onsetsTmp, nvs::analysis::getLengthInSeconds(wave.size(), sr));
	nvs::analysis::writeEventsToWav(wave, onsetsTmp, audioFilePath.toStdString(), analyzer);
}

void TsnGranularAudioProcessor::changeListenerCallback(juce::ChangeBroadcaster* source) {
	writeToLog("processor: change message received\n");
	if (&_analyzer == dynamic_cast<nvs::analysis::ThreadedAnalyzer*>(source)){
		writeToLog("processor: dynamic cast to threaded analyzer successful\n");
		// now we can simply check on our own analyzer, don't even need to use source qua source
		// then load onsets into synth
		auto onsets = _analyzer.getOnsets();
		if (onsets.size()){
			if (auto *tsn_granular_synth = dynamic_cast<JuceTsnGranularSynthesizer *>(_granularSynth.get())){
				tsn_granular_synth->loadOnsets(onsets);
			}
			else {
				writeToLog("TsnGranularAudioProcessor::changeListenerCallback failed dynamic cast to TsnGranular");
			}
		}
		writeToLog("processor: change listener callback: got things\n");
	}
	else {
		writeToLog("processor: dynamic cast unsuccessful");
	}
}

void TsnGranularAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages) {
	auto const buffer_fn_hash = _granularSynth->getFilenameHash();
	auto const analysis_fn_hash = _analyzer.getFilenameHash();
	if (analysis_fn_hash != buffer_fn_hash) {
		juce::ScopedNoDenormals noDenormals;	// probably not necessary at this point but also doesnt hurt
		for (auto i = getTotalNumInputChannels(); i < getTotalNumOutputChannels(); ++i){
			buffer.clear (i, 0, buffer.getNumSamples());
		}
		
		logRateLimited("TsnGranularAudioProcessor::processBlock: synthesis/analysis hash mismatch, exiting early", 1200);
		return;
	}
	Slicer_granularAudioProcessor::processBlock (buffer, midiMessages);
}

//==============================================================================
// This creates new instances of the plugin..
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
	return new TsnGranularAudioProcessor();
}
