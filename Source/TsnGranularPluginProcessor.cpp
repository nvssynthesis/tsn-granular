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
,	_analyzer(this)	// effectively adding this as listener to the analyzer
{
	if (JuceTsnGranularSynthesizer *s = dynamic_cast<JuceTsnGranularSynthesizer *>(granular_synth_juce.get())){
		writeToLog("dynamic cast to JuceTsnGranularSynthesizer successful");
	}
#if defined(DEBUG_BUILD) | defined(DEBUG) | defined(_DEBUG)
	writeToLog("TsnGranularAudioProcessor DEBUG MODE");
#else
	writeToLog("TsnGranularAudioProcessor RELEASE MODE\n");
#endif
	
	juce::String hasLogger = granular_synth_juce->hasLogger() ? "true" : "false";
	juce::String msg = juce::String("synth has logger? ");
	msg.append(hasLogger, 10);
	writeToLog(msg);
}

TsnGranularAudioProcessor::~TsnGranularAudioProcessor()
{}

//==============================================================================
juce::AudioProcessorEditor* TsnGranularAudioProcessor::createEditor() {
	return new TsnGranularAudioProcessorEditor(*this);
}
//==============================================================================

void TsnGranularAudioProcessor::loadAudioFile(juce::File const f, bool notifyEditor){
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
		writeToLog("analyzer onset thread started\n");
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

void TsnGranularAudioProcessor::changeListenerCallback(juce::ChangeBroadcaster* source) {
	writeToLog("processor: change message received\n");
	if (&_analyzer == dynamic_cast<nvs::analysis::ThreadedAnalyzer*>(source)){
		writeToLog("processor: dynamic cast to threaded analyzer successful\n");
		// now we can simply check on our own analyzer, don't even need to use source qua source
		// then load onsets into synth
		auto onsets = _analyzer.getOnsetsInSeconds();
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
}

//==============================================================================
// This creates new instances of the plugin..
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
	return new TsnGranularAudioProcessor();
}
