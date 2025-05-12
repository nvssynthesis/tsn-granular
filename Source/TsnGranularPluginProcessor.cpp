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
,	lfo2D(apvts, 40.0)	// processor owns it, but editor facilitates communication from lfo > timbre space
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


//==============================================================================
juce::AudioProcessorEditor* TsnGranularAudioProcessor::createEditor() {
	TsnGranularAudioProcessorEditor* ed = new TsnGranularAudioProcessorEditor (*this);
	_analyzer.addChangeListener(ed);
	
	nonAutomatableState.addListener(ed);

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
	
	writeToLog("setStateInformation successful; nonAutomatableState reassigned");
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
	if (_analyzer.startThread(juce::Thread::Priority::high)){	// only entry point to analysis
		writeToLog("analyzer onset thread started");
	}
}

void TsnGranularAudioProcessor::writeEvents(){
	auto const onsetsOpt = _analyzer.getOnsets();
	if (!onsetsOpt.has_value()){
		writeToLog("writeEvents failed: onsets optional does not contain value");
		return;
	}
	auto const buffer = sampleManagementGuts.sampleBuffer;
	auto const waveSpan = std::span<float const>(buffer.getReadPointer(0), buffer.getNumSamples());
	std::vector<float> wave(waveSpan.size());
	wave.assign(waveSpan.begin(), waveSpan.end());
	
	// any reason to use getPropertyAsValue instead?
	juce::String audioFilePath = nonAutomatableState.getChildWithName("PresetInfo").getProperty("sampleFilePath");
	float sr = 	nonAutomatableState.getChildWithName("PresetInfo").getProperty("sampleRate");

	std::vector<float> onsetsTmp = onsetsOpt.value();
	auto analyzer = _analyzer.getAnalyzer();

	nvs::analysis::denormalizeOnsets(onsetsTmp, nvs::analysis::getLengthInSeconds(wave.size(), sr));
	
	
	nvs::analysis::RunLoopStatus rls;
	nvs::analysis::ShouldExitFn shouldExitFn = [](){return false;};
	nvs::analysis::writeEventsToWav(wave, onsetsTmp, audioFilePath.toStdString(), analyzer, rls, shouldExitFn);
}

void TsnGranularAudioProcessor::changeListenerCallback(juce::ChangeBroadcaster* source) {
	writeToLog("processor: change message received\n");
	if (&_analyzer == dynamic_cast<nvs::analysis::ThreadedAnalyzer*>(source)){
		writeToLog("processor: dynamic cast to threaded analyzer successful\n");
		// now we can simply check on our own analyzer, don't even need to use source qua source
		// then load onsets into synth
		auto onsetsOpt = _analyzer.getOnsets();
		
		if (onsetsOpt.has_value() and onsetsOpt->size()){
			if (auto *tsn_granular_synth = dynamic_cast<JuceTsnGranularSynthesizer *>(_granularSynth.get())){
				tsn_granular_synth->loadOnsets(onsetsOpt.value());
				writeToLog("TsnGranularAudioProcessor::changeListenerCallback: loaded onsets successfully");
			}
			else {
				writeToLog("TsnGranularAudioProcessor::changeListenerCallback failed dynamic cast to TsnGranular");
			}
		}
		else {
			writeToLog("TsnGranularAudioProcessor::changeListenerCallback: either onsets optional has no value or has 0 size");
		}
	}
	else {
		writeToLog("TsnGranularAudioProcessor::changeListenerCallback: dynamic cast from ChangeBroadcaster to ThreadedAnalyzer unsuccessful");
	}
}
void TsnGranularAudioProcessor::TimbreSpaceNeededData::extract(std::vector<nvs::analysis::Features> featuresToExtract) {
	if (!fullTimbreSpace.has_value()){
		std::cerr << "TsnGranularAudioProcessorEditor::TimbreSpaceNeededData::extract: timbre space empty, early exit\n";
	}
	eventwiseExtractedTimbrePoints.clear();
	eventwiseExtractedTimbrePoints.reserve(fullTimbreSpace->size());
	
	for (auto const &t : fullTimbreSpace.value()) {
		std::vector<float> v = nvs::analysis::extractFeatures(t, featuresToExtract, nvs::analysis::Statistic::Median);
		eventwiseExtractedTimbrePoints.push_back(v);
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
