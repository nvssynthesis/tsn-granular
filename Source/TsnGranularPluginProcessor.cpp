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

TSNGranularAudioProcessor::TSNGranularAudioProcessor()
:	SlicerGranularAudioProcessor()
,	_analyzer(this)			// effectively adding this as listener to the analyzer
,	navigator{ [this](const std::vector<double>& v) -> void
	{
		// set navigator of timbre space component
		auto const p2 = juce::Point<float>(v[0], v[1]);
		// also attract to nearest point
		auto p5 = nvs::timbrespace::Timbre5DPoint {	// needs to handle arbitrary dimensions and just attract based on provided dims
			._p2D{p2},
			._p3D{0.f, 0.f, 0.f}
		};
		auto const sharpness = (double) *(apvts.getRawParameterValue("nav_selection_sharpness"));
		auto const neighborhood = (int) *(apvts.getRawParameterValue("nav_selection_neighborhood"));

		_timbreSpace.setProbabilisticPointFromTarget(p5, neighborhood, sharpness , 0.f);
		setReadBoundsFromChosenPoint();	// needs to affect processor but has final effect on gui
	},
	std::in_place_type<nvs::nav::LFO2D>, apvts, 40.0 }
{
#if defined(DEBUG_BUILD) | defined(DEBUG) | defined(_DEBUG)
	writeToLog("TsnGranularAudioProcessor DEBUG MODE");
#else
	writeToLog("TsnGranularAudioProcessor RELEASE MODE\n");
#endif
	
	juce::ValueTree settingsVT = nonAutomatableState.getOrCreateChildWithName("Settings", nullptr);
	nvs::analysis::initializeSettingsBranches(settingsVT);
	
	getNonAutomatableState().addListener(&_timbreSpace);
	_analyzer.addChangeListener(&_timbreSpace);
}
TSNGranularAudioProcessor::~TSNGranularAudioProcessor() {
	getNonAutomatableState().removeListener(&_timbreSpace);
	_analyzer.removeChangeListener(&_timbreSpace);
}
//==============================================================================
void TSNGranularAudioProcessor::initSynth() {
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
juce::AudioProcessorEditor* TSNGranularAudioProcessor::createEditor() {
	TsnGranularAudioProcessorEditor* ed = new TsnGranularAudioProcessorEditor (*this);
	return ed;
}

void TSNGranularAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
	std::unique_ptr<juce::XmlElement> xmlState (getXmlFromBinary (data, sizeInBytes));

	if (xmlState == nullptr || ! xmlState->hasTagName ("PluginState")){
		return;
	}
	
	juce::ValueTree root = juce::ValueTree::fromXml (*xmlState);

	if (auto params = root.getChildWithName (apvts.state.getType()); params.isValid()){
		apvts.replaceState (params);
	}
	
	nonAutomatableState = root.getChildWithName ("NonAutomatable");
	if (!nonAutomatableState.isValid()){ writeToLog("nonAutomatableState invalid; returning\n"); return; }

	auto const settings = nonAutomatableState.getChildWithName("Settings");
	if (!nvs::analysis::verifySettingsStructure(settings)){
		writeToLog("In setStateInformation: Settings tree invalid. Not overwriting constructed default settings.\n");
		return;
	}
	
	if (juce::String fp = nonAutomatableState.getProperty("analysisFile", {}); !fp.isEmpty()) {
		juce::File file(fp);
		auto fstream = juce::FileInputStream(file);
		
		if (fstream.failedToOpen()){
			writeToLog( fstream.getStatus().getErrorMessage() );
#pragma message("would be good to give popup opportunity for user to otherwise find the file")
			return;
		}
		
		auto const analysisFileStream = juce::ValueTree::readFromStream(fstream);
		auto const analysisFileTree = analysisFileStream.getChildWithName("TimbreAnalysis");
		if (analysisFileTree.isValid()){
			_timbreSpace.setTimbreSpaceTree(analysisFileTree);
		}
	}
	
	if (auto const presetInfo = nonAutomatableState.getChildWithName ("PresetInfo"); presetInfo.isValid()) {
		auto const audioSamplePath = presetInfo.getProperty ("sampleFilePath").toString();
		if (audioSamplePath.isNotEmpty()) {
			loadAudioFile ({ audioSamplePath }, true);
		}
	}
	writeToLog("setStateInformation fully successful\n");
}
void TSNGranularAudioProcessor::saveAnalysisToFile(const juce::String& filePath, std::function<void(bool)> resultCallback) {
	juce::ValueTree vt("super");
	nonAutomatableState.setProperty("analysisFile", filePath, nullptr);
	vt.addChild(nonAutomatableState, 0, nullptr);
	auto tsTree = _timbreSpace.getTimbreSpaceTree();
//	tsTree.getChildWithName("Metadata").setProperty("settingsHash", _analyzer.getSettingsHash(), nullptr);
	vt.addChild(tsTree, 1, nullptr);
	
	bool success = [vt, filePath](bool useBinary){
		juce::File const file(filePath);
		if (useBinary){
			return nvs::util::saveValueTreeToBinary(vt, file);
		}
		else {
			return nvs::util::saveValueTreeToJSON(vt, file);
		}
	}(true);
	juce::MessageManager::callAsync([resultCallback, success](){
		resultCallback(success);
	});
}
//==============================================================================
void TSNGranularAudioProcessor::loadAudioFile(juce::File const f, bool notifyEditor){
	writeToLog("TSN: loadAudioFile\n");
	// this used to have just copied and pasted code from slicer. it seems to work properly simply by manually calling the base function like so:
	SlicerGranularAudioProcessor::loadAudioFile(f, notifyEditor);
	
	// if _timbreSpace's audio file hash does not match _sampleManagementGuts' audio file hash:
	if (!_timbreSpace.hasValidAnalysisFor(sampleManagementGuts.getHash())) {
		askForAnalysis();
	}
	
	writeToLog("TSN: loadAudioFile exiting");
}

void TSNGranularAudioProcessor::setReadBoundsFromChosenPoint() {
	auto const pIndices = _timbreSpace.getCurrentPointIndices();
	auto const onsetOpt = _timbreSpace.getOnsets();

	if (onsetOpt.has_value() and (onsetOpt.value().size() != 0)){
		if (auto *const tsn_synth_juce = dynamic_cast<TSNGranularSynthesizer *const>(_granularSynth.get())){
			tsn_synth_juce->setWaveEvents(pIndices);
		}
	}
}

void TSNGranularAudioProcessor::askForAnalysis(){
	if (_analyzer.Thread::isThreadRunning()){
		_analyzer.stopAnalysis();
	}
	auto const buffer = sampleManagementGuts.getSampleBuffer();
	if (!buffer.getNumChannels()){
		writeToLog("TSN: askForAnalysis: buffer had no channels. Early exit.");
		return;
	}
	if (!buffer.getNumSamples()){
		writeToLog("TSN: askForAnalysis: buffer had no samples. Early exit.");
		return;
	}
	_analyzer.updateWave(std::span<float const>(buffer.getReadPointer(0), buffer.getNumSamples()));
	
	auto const settingsVT = nonAutomatableState.getChildWithName("Settings");
	jassert (settingsVT.getParent().getChildWithName("PresetInfo").hasProperty("sampleRate"));
	_analyzer.updateSettings(settingsVT);
	
	if (_analyzer.startThread(juce::Thread::Priority::high)){	// only entry point to analysis
		writeToLog("analyzer onset thread started");
	}
}

void TSNGranularAudioProcessor::writeEvents(){
	auto const onsetsOpt = _timbreSpace.getOnsets();
	if (!onsetsOpt.has_value()){
		writeToLog("writeEvents failed: onsets optional does not contain value");
		return;
	}
	auto const buffer = sampleManagementGuts.getSampleBuffer();
	auto const waveSpan = std::span<float const>(buffer.getReadPointer(0), buffer.getNumSamples());
	std::vector<float> wave(waveSpan.size());
	wave.assign(waveSpan.begin(), waveSpan.end());
	
	// any reason to use getPropertyAsValue instead?
	juce::String sampleFilePath = nonAutomatableState.getChildWithName("PresetInfo").getProperty("sampleFilePath");
	float sr = 	nonAutomatableState.getChildWithName("PresetInfo").getProperty("sampleRate");

	std::vector<float> onsetsTmp = onsetsOpt.value();

	nvs::analysis::denormalizeOnsets(onsetsTmp, nvs::analysis::getLengthInSeconds(wave.size(), sr));
	
	nvs::analysis::RunLoopStatus rls;
	nvs::analysis::ShouldExitFn shouldExitFn = [](){return false;};
	nvs::analysis::writeEventsToWav(wave, onsetsTmp, sampleFilePath.toStdString(), _analyzer.getAnalyzer(), rls, shouldExitFn);
}


void TSNGranularAudioProcessor::changeListenerCallback(juce::ChangeBroadcaster* source) {
	writeToLog("processor: change message received\n");
	if (&_analyzer == dynamic_cast<nvs::analysis::ThreadedAnalyzer*>(source)){
		writeToLog("processor: dynamic cast to threaded analyzer successful\n");
		// now we can simply check on our own analyzer, don't even need to use source qua source
		// then load onsets into synth
		auto onsetsOpt = _timbreSpace.getOnsets();
		
		if (onsetsOpt.has_value() and onsetsOpt->size()){
			
			if (auto *tsn_granular_synth = dynamic_cast<TSNGranularSynthesizer *>(_granularSynth.get())){
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
void TSNGranularAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages) {
	if (!_timbreSpace.hasValidAnalysisFor(sampleManagementGuts.getHash())) {
		juce::ScopedNoDenormals noDenormals;	// probably not necessary at this point but also doesnt hurt
		for (auto i = getTotalNumInputChannels(); i < getTotalNumOutputChannels(); ++i){
			buffer.clear (i, 0, buffer.getNumSamples());
		}
		
		logRateLimited("TsnGranularAudioProcessor::processBlock: analysis not current, exiting early", 1200);
		return;
	}
	SlicerGranularAudioProcessor::processBlock (buffer, midiMessages);
}

//==============================================================================
// This creates new instances of the plugin..
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
	return SlicerGranularAudioProcessor::create<TSNGranularAudioProcessor>();
}
