#include "TsnGranularPluginProcessor.h"
#include "TsnGranularPluginEditor.h"
#include "fmt/core.h"

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
	if (dynamic_cast<JuceTsnGranularSynthesizer *>(granular_synth_juce.get())){
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
	loggingGuts.fileLogger.logMessage("TSN processor: setStateInformation");
	// Restore parameters from memory block
	std::unique_ptr<juce::XmlElement> xmlState (getXmlFromBinary (data, sizeInBytes));

	if (xmlState.get() != nullptr){
		if (xmlState->hasTagName (apvts.state.getType())){
			apvts.replaceState (juce::ValueTree::fromXml (*xmlState));

			juce::Value sampleFilePath = apvts.state.getPropertyAsValue(sampleManagementGuts.audioFilePathValueTreeStateIdentifier, nullptr, true);
			juce::File const sampleFile = juce::File(sampleFilePath.toString());
			loadAudioFile(sampleFile, true);
		}
	}
}
//==============================================================================

void TsnGranularAudioProcessor::loadAudioFile(juce::File const f, bool notifyEditor){
	writeToLog("TSN: loadAudioFile\n");
	const juce::SpinLock::ScopedLockType lock(audioBlockLock);
	loggingGuts.fileLogger.logMessage("                                          ...locked");

	readInAudioFileToBuffer(f);
	granular_synth_juce->setAudioBlock(sampleManagementGuts.sampleBuffer, sampleManagementGuts.lastFileSampleRate, f.getFullPathName().hash());	// maybe this could just go inside readInAudioFileToBuffer()
	{
		juce::Value sampleFilePathValue = apvts.state.getPropertyAsValue(sampleManagementGuts.audioFilePathValueTreeStateIdentifier, nullptr, true);
		sampleFilePathValue.setValue(f.getFullPathName());
		sampleManagementGuts.sampleFilePath = sampleFilePathValue.toString();
		apvts.state.setProperty(sampleManagementGuts.audioFilePathValueTreeStateIdentifier, sampleFilePathValue, nullptr);
	}
	if (notifyEditor){
		loggingGuts.fileLogger.logMessage("Processor: sending change message from loadAudioFile");
		juce::MessageManager::callAsync([this]() { sampleManagementGuts.sendChangeMessage(); });
	}
	{	// limit tmp scope
		double const sr = sampleManagementGuts.lastFileSampleRate;
		auto anSettingsTmp = _analyzer.getAnalysisSettings();
		anSettingsTmp.sampleRate = static_cast<float>(sr);
		_analyzer.setAnalysisSettings(anSettingsTmp);
		askForAnalysis();
	}
	writeToLog("TSN: loadAudioFile exiting");
}

void TsnGranularAudioProcessor::setWaveEvent(size_t index)
{
	if (auto *const tsn_synth_juce = dynamic_cast<JuceTsnGranularSynthesizer *const>(granular_synth_juce.get())){
		tsn_synth_juce->setWaveEvent(index);
	}
}

void TsnGranularAudioProcessor::setWaveEvents(std::array<size_t, 4> indices, std::array<float, 4> weights)
{
	if (auto *const tsn_synth_juce = dynamic_cast<JuceTsnGranularSynthesizer *const>(granular_synth_juce.get())){
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
						 sampleManagementGuts.sampleFilePath.hash());
	if (_analyzer.startThread(juce::Thread::Priority::high)){
		writeToLog("analyzer onset thread started");
	}
}

std::vector<float> TsnGranularAudioProcessor::getOnsets() const {
	return _analyzer.getOnsets();
}

std::vector<std::vector<float>> TsnGranularAudioProcessor::getOnsetwiseTimbres() const {
	return _analyzer.getOnsetwiseTimbreMeasurements();
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
	
	std::vector<float> onsetsTmp = _analyzer.getOnsets();
	nvs::analysis::denormalizeOnsets(onsetsTmp, nvs::analysis::getLengthInSeconds(wave.size(), _analyzer.getAnalyzer()._analysisSettings.sampleRate));
	nvs::analysis::writeEventsToWav(wave, onsetsTmp, audioFilePath.toStdString(), _analyzer.getAnalyzer());
}

void TsnGranularAudioProcessor::changeListenerCallback(juce::ChangeBroadcaster* source) {
	writeToLog("processor: change message received\n");
	if (&_analyzer == dynamic_cast<nvs::analysis::ThreadedAnalyzer*>(source)){
		writeToLog("processor: dynamic cast to threaded analyzer successful\n");
		// now we can simply check on our own analyzer, don't even need to use source qua source
		// then load onsets into synth
		auto onsets = _analyzer.getOnsets();
		if (onsets.size()){
			if (auto *tsn_granular_synth = dynamic_cast<JuceTsnGranularSynthesizer *>(granular_synth_juce.get())){
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
	auto const buffer_fn_hash = granular_synth_juce->getFilenameHash();
	auto const analysis_fn_hash = _analyzer.getFilenameHash();
	if (analysis_fn_hash != buffer_fn_hash) {
		writeToLog("TsnGranularAudioProcessor::processBlock: synthesis/analysis hash mismatch, exiting early");
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
