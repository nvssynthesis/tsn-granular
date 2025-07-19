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
,	_analyzer()
,	_navigator{ [this](const std::vector<double>& v) -> void
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
	
	juce::ValueTree settingsVT = apvts.state.getOrCreateChildWithName("Settings", nullptr);
	nvs::analysis::initializeSettingsBranches(settingsVT, false);
	
	apvts.state.addListener(&_timbreSpace);
	_analyzer.addChangeListener(&_timbreSpace);
	_timbreSpace.addActionListener(this);
}
TSNGranularAudioProcessor::~TSNGranularAudioProcessor() {
	apvts.state.removeListener(&_timbreSpace);
	_analyzer.removeChangeListener(&_timbreSpace);
	_timbreSpace.removeActionListener(this);	// is this even necessary if processor owns timbreSpace?
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
	std::unique_ptr<juce::XmlElement> xmlState(getXmlFromBinary(data, sizeInBytes));
	if (xmlState == nullptr || !xmlState->hasTagName("PLUGIN_STATE")) {
		return;
	}
	
	juce::ValueTree root = juce::ValueTree::fromXml(*xmlState);
	apvts.replaceState(root);
	
	ensureSettingsStructure();
	loadAnalysisFileFromState();
	
	writeToLog("setStateInformation fully successful\n");
}
void TSNGranularAudioProcessor::saveAnalysisToFile(const juce::String& filePath, std::function<void(bool)> resultCallback) {
	auto fileInfo = apvts.state.getChildWithName("FileInfo");
	if (!fileInfo.isValid()) return;
	fileInfo.setProperty("analysisFile", filePath, nullptr);

	juce::ValueTree analysisVT("super");

	/* metadata needs:
 	 -audio sample absolute path (for loading audio file when analysis is imported)
	 -audio file sample rate?
	 -settings hash (to quickly confirm that analysis has/has not been done for a given analysisSettings on a given audio file)
	 -audio wave hash (for confirming that the analysis is definitely relevant for a given audio file (e.g. if the audio gets analyzed, but then is later edited, this will require new analysis))
	 -later: maybe the settings themselves, which would allow to load analysis file and populate the settings of the plugin instance?
	*/
	auto tsTree = _timbreSpace.getTimbreSpaceTree();
	auto mdTree = tsTree.getChildWithName("Metadata");
	mdTree.setProperty("sampleFilePath", apvts.state.getProperty("sampleFilePath"), nullptr);
	mdTree.setProperty("sampleRate", apvts.state.getProperty("sampleRate"), nullptr);
	mdTree.setProperty("waveformHash", sampleManagementGuts.getHash(), nullptr);
	mdTree.setProperty("settingsHash", _analyzer.getSettingsHash(), nullptr);
	analysisVT.addChild(tsTree, 1, nullptr);
	
	bool success = [vt=analysisVT, filePath](bool useBinary){
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
	SlicerGranularAudioProcessor::loadAudioFile(f, notifyEditor);	// has async call to set value tree prop "sampleRate"
	
	_timbreSpace.setAudioPaths(f.getFullPathName());

	if (_timbreSpace.hasValidAnalysisFor(sampleManagementGuts.getHash())) {
		/* do nothing */
		fmt::print("TSNGranularAudioProcessor::loadAudioFile: already had valid analysis for {}", f.getFullPathName().toStdString());
	}
	else if (!loadAnalysisFileFromState())	// try to load analysis from state; if it fails then do fresh analysis
	{
		askForAnalysis();
	}
	writeToLog("TSN: loadAudioFile exiting");
}

void TSNGranularAudioProcessor::setReadBoundsFromChosenPoint() {
	auto const pIndices = _timbreSpace.getCurrentPointIndices();
	auto const onsetOpt = _timbreSpace.getOnsets();

	if (!onsetOpt.has_value() || (onsetOpt.value().size() == 0)){
		return;
	}
	if (auto *const tsn_synth_juce = dynamic_cast<TSNGranularSynthesizer *const>(_granularSynth.get())){
		// could check if it's ready for processing here to avoid chance of assigning events beyond numOnsets
		tsn_synth_juce->setWaveEvents(pIndices);
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
	
	auto const settingsVT = apvts.state.getChildWithName("Settings");
	jassert (settingsVT.getParent().hasProperty("sampleRate"));
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
	juce::String sampleFilePath = apvts.state.getProperty("sampleFilePath");
	float sr = 	apvts.state.getProperty("sampleRate");

	std::vector<float> onsetsTmp = onsetsOpt.value();

	nvs::analysis::denormalizeOnsets(onsetsTmp, nvs::analysis::getLengthInSeconds(wave.size(), sr));
	
	nvs::analysis::RunLoopStatus rls;
	nvs::analysis::ShouldExitFn shouldExitFn = [](){return false;};
	nvs::analysis::writeEventsToWav(wave, onsetsTmp, sampleFilePath.toStdString(), _analyzer.getAnalyzer(), rls, shouldExitFn);
}

void TSNGranularAudioProcessor::actionListenerCallback(juce::String const &message) {
	if (message.compare("reportAvailability") == 0){
		auto onsetsOpt = _timbreSpace.getOnsets();
		if (onsetsOpt.has_value() && onsetsOpt->size()){
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
