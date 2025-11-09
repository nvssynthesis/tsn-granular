#include "TsnGranularPluginProcessor.h"

#include "StringAxiom.h"
#include "TsnGranularPluginEditor.h"
#include "fmt/core.h"
#include "Analysis/Settings.h"
#include "Analysis/OnsetAnalysis/OnsetProcessing.h"

//==============================================================================

TSNGranularAudioProcessor::TSNGranularAudioProcessor()
:	SlicerGranularAudioProcessor()
,	_analyzer()
{
#if defined(DEBUG_BUILD) | defined(DEBUG) | defined(_DEBUG)
	writeToLog("TsnGranularAudioProcessor DEBUG MODE");
#else
	writeToLog("TsnGranularAudioProcessor RELEASE MODE\n");
#endif
	
	juce::ValueTree settingsVT = apvts.state.getOrCreateChildWithName("Settings", nullptr);
	nvs::analysis::initializeSettingsBranches(settingsVT, false);

    _analyzer.addChangeListener(this);

	// initSynth only gets called AFTER construction for reasons,
	// so do not call anything here that requires _tsnGranularSynth to be valid (or likewise,
	// anything that uses _granularSynth in any way).
}
TSNGranularAudioProcessor::~TSNGranularAudioProcessor() {
	_analyzer.removeChangeListener(&_tsnGranularSynth->getTimbreSpace());
    _analyzer.removeChangeListener(this);
}
//==============================================================================
void TSNGranularAudioProcessor::initSynth() {
    _granularSynth = std::make_unique<TSNGranularSynth>(apvts);
    _tsnGranularSynth = static_cast<TSNGranularSynth*>(_granularSynth.get());
	_analyzer.addChangeListener(&_tsnGranularSynth->getTimbreSpace());
}
juce::AudioProcessorEditor* TSNGranularAudioProcessor::createEditor() {
    const auto ed = new TsnGranularAudioProcessorEditor (*this);
	return ed;
}

void TSNGranularAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
	const std::unique_ptr<juce::XmlElement> xmlState(getXmlFromBinary(data, sizeInBytes));
	if (xmlState == nullptr || !xmlState->hasTagName("PLUGIN_STATE")) {
		return;
	}

	const juce::ValueTree root = juce::ValueTree::fromXml(*xmlState);
	apvts.replaceState(root);	// also causes _presetManager, a ValueTreeListener, to call loadAnalysisFileFromState via valueTreeRedirected

	ensureSettingsStructure();

	writeToLog("setStateInformation fully successful\n");
}
void TSNGranularAudioProcessor::saveAnalysisToFile(const juce::String& filePath, std::function<void(bool)> resultCallback) const {
	// inform plugin state of what the associated analysis file will be
	auto fileInfo = apvts.state.getChildWithName("FileInfo");
	if (!fileInfo.isValid()) return;
	fileInfo.setProperty("analysisFile", filePath, nullptr);

	juce::ValueTree analysisVT("super");

	/* metadata needs:
     -audio sample absolute path (for loading audio file when analysis is imported)
     -audio file sample rate?
     -settings hash (to quickly confirm that analysis has/has not been done for a given analysisSettings on a given
     audio file) -audio wave hash (for confirming that the analysis is definitely relevant for a given audio file (e.g.
     if the audio gets analyzed, but then is later edited, this will require new analysis)) -later: maybe the settings
     themselves, which would allow to load analysis file and populate the settings of the plugin instance?
    */
    const auto tsTree = _tsnGranularSynth->getTimbreSpace().getTimbreSpaceTree();
	auto timbreSpaceMetaDataTree = tsTree.getChildWithName(nvs::axiom::Metadata);
	timbreSpaceMetaDataTree.setProperty(nvs::axiom::sampleFilePath, apvts.state.getProperty(nvs::axiom::sampleFilePath), nullptr);
	timbreSpaceMetaDataTree.setProperty(nvs::axiom::sampleRate, apvts.state.getProperty(nvs::axiom::sampleRate), nullptr);
	timbreSpaceMetaDataTree.setProperty(nvs::axiom::audioHash, sampleManagementGuts.getWaveformHash(), nullptr);
	timbreSpaceMetaDataTree.setProperty(nvs::axiom::settingsHash, _analyzer.getSettingsHash(), nullptr);
	analysisVT.addChild(tsTree, 1, nullptr);

    DBG(fmt::format("tree being SAVED: {}", nvs::util::valueTreeToXmlStringSafe(analysisVT).toStdString()));
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
void TSNGranularAudioProcessor::loadAudioFileAndUpdateState(juce::File const f, bool notifyEditor){
	writeToLog("TSN: loadAudioFileAndUpdateState\n");
	// this used to have just copied and pasted code from slicer. it seems to work properly simply by manually calling the base function like so:
	SlicerGranularAudioProcessor::loadAudioFileAndUpdateState(f, notifyEditor);	// has async call to set value tree prop "sampleRate"

	if (_tsnGranularSynth->getTimbreSpace().hasValidAnalysisFor(sampleManagementGuts.getWaveformHash())) {
		/* do nothing */
		writeToLog(fmt::format("TSNGranularAudioProcessor already had valid analysis for {}\n", f.getFullPathName().toStdString()));
	}
	else if (!loadAnalysisFileFromState())	// try to load analysis from state; if it fails then do fresh analysis
	{
		askForAnalysis();
	}
	writeToLog("TSN: loadAudioFileAndUpdateState exiting");
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
	_analyzer.updateStoredAudio(std::span(buffer.getReadPointer(0), static_cast<size_t>(buffer.getNumSamples())),
		getSampleFilePath());
	
	auto const settingsVT = apvts.state.getChildWithName("Settings");
	auto const par = settingsVT.getParent();
	jassert (par.getChildWithName("FileInfo").hasProperty("sampleRate"));
	_analyzer.updateSettings(settingsVT);
	
	if (_analyzer.startThread(juce::Thread::Priority::high)){	// only entry point to analysis
		writeToLog("analyzer onset thread started");
	}
}
void TSNGranularAudioProcessor::changeListenerCallback (juce::ChangeBroadcaster *source) {
    if (source == &_analyzer) {
        if (!_analyzer.onsetsReady()) {
            DBG("TSNGranularAudioProcessor: onsets not ready, returning\n");
            return;
        }
        if (const auto onsetsResult = _analyzer.shareOnsetAnalysis();
            onsetsResult->waveformHash == sampleManagementGuts.getWaveformHash())
        {
            _tsnGranularSynth->loadOnsets(onsetsResult);
        } else {
            DBG("TSNGranularAudioProcessor: Hash mismatch between onsets and current sample, returning\n");
            return;
        }
    }
    SlicerGranularAudioProcessor::changeListenerCallback(source);
}
void TSNGranularAudioProcessor::writeEvents(){
	auto const sharedOnsets = _tsnGranularSynth->getTimbreSpace().shareOnsets();
	if (sharedOnsets == nullptr){
		DBG("TSNGranularAudioProcessor::writeEvents failed: shared onsets null, returning early\n");
		return;
	}

    auto const settingsVT = apvts.state.getChildWithName(nvs::axiom::Settings);
    _analyzer.updateSettings(settingsVT);

	auto const buffer = sampleManagementGuts.getSampleBuffer();
	auto const waveSpan = std::span(buffer.getReadPointer(0), static_cast<size_t>(buffer.getNumSamples()));
	std::vector<float> wave(waveSpan.size());
	wave.assign(waveSpan.begin(), waveSpan.end());
	
	// any reason to use getPropertyAsValue instead?

	std::vector<float> onsetsTmp = sharedOnsets->onsets;

    auto const par = settingsVT.getParent();
    const auto fileInfoTree = par.getChildWithName(nvs::axiom::FileInfo);
    jassert (fileInfoTree.hasProperty(nvs::axiom::sampleRate));

    const auto sampleFilePath = [fileInfoTree, sharedOnsets]()-> std::string {
        jassert (fileInfoTree.hasProperty(nvs::axiom::sampleFilePath));
	    const juce::String sfp = fileInfoTree.getProperty(nvs::axiom::sampleFilePath);
	    jassert(!sfp.isEmpty());
	    if (sharedOnsets->audioFileAbsPath != sfp) {
	        DBG("TSNGranularAudioProcessor::writeEvents failed: file path mismatch, returning early\n");
	        return "";
	    }
        return sfp.toStdString();
    }();
    if (sampleFilePath == "") {
        return; // file path mismatch
    }

    const float sr = fileInfoTree.getProperty(nvs::axiom::sampleRate);
	nvs::analysis::denormalizeOnsets(onsetsTmp, nvs::analysis::getLengthInSeconds(wave.size(), sr));
	
	nvs::analysis::RunLoopStatus rls;
    const nvs::analysis::ShouldExitFn shouldExitFn = [](){return false;};
	nvs::analysis::writeEventsToWav(wave, onsetsTmp, sampleFilePath, _analyzer.getAnalyzer(), rls, shouldExitFn);
}

void TSNGranularAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages) {
	if (!_tsnGranularSynth->getTimbreSpace().hasValidAnalysisFor(sampleManagementGuts.getWaveformHash())) {
		juce::ScopedNoDenormals noDenormals;	// probably not necessary at this point but also doesnt hurt
		for (auto i = getTotalNumInputChannels(); i < getTotalNumOutputChannels(); ++i){
			buffer.clear (i, 0, buffer.getNumSamples());
		}
		
		logRateLimited("TsnGranularAudioProcessor::processBlock: analysis not current, exiting early", 1200);
		return;
	}
	SlicerGranularAudioProcessor::processBlock (buffer, midiMessages);
}

void TSNGranularAudioProcessor::ensureSettingsStructure() {
    if (const auto settings = apvts.state.getChildWithName(nvs::axiom::Settings);
        !nvs::analysis::verifySettingsStructure(settings))
    {
        juce::ValueTree settingsVT = apvts.state.getOrCreateChildWithName(nvs::axiom::Settings, nullptr);
        nvs::analysis::initializeSettingsBranches(settingsVT);
    }
}
bool TSNGranularAudioProcessor::loadAnalysisFileFromState() {
    // TODO: Return more particular failure/success and handle each case. E.g. there could be auto-search in
    /// designated directory, manual find, and give up (just perform fresh analysis)
    const auto fileInfo = apvts.state.getChildWithName(nvs::axiom::FileInfo);
	writeToLog(fmt::format("FileInfo: {}", nvs::util::valueTreeToXmlStringSafe(fileInfo).toStdString()));
    if (!fileInfo.isValid()) {
		writeToLog("file info value tree invalid\n");
    	return false;
    }
    const juce::String analysisFilePath = fileInfo.getProperty(nvs::axiom::analysisFile, {});
	writeToLog(fmt::format("analysisFilePath: {}", analysisFilePath.toStdString()));

    if (analysisFilePath.isEmpty()) {
    	writeToLog("analysisFilePath is empty");
	    return false;
    }
    const juce::File analysisFile(analysisFilePath);
    auto analysisFileInputStream = juce::FileInputStream(analysisFile);

    if (analysisFileInputStream.failedToOpen()) {
        writeToLog(fmt::format("failed to open {}; error message: {}",
            analysisFilePath.toStdString(),
            analysisFileInputStream.getStatus().getErrorMessage().toStdString()));
        // TODO: Give popup opportunity for user to find the file
        return false;
    }

    const auto analysisFileValueTree = juce::ValueTree::readFromStream(analysisFileInputStream);

    if (const auto analysisFileTree = analysisFileValueTree.getChildWithName(nvs::axiom::TimbreAnalysis);
        analysisFileTree.isValid())
    {
        DBG(fmt::format("tree being set: {}", nvs::util::valueTreeToXmlStringSafe(analysisFileTree).toStdString()));
    	// we need to know if we even SHOULD load the analysisFile pointed to by the state
    	if (auto metadataTree = analysisFileTree.getChildWithName(nvs::axiom::Metadata);
			metadataTree.isValid() &&
			nvs::util::getAndMigrateAudioHash(metadataTree) == getAudioHash())
    	{
    		writeToLog("setting via setStateInformation");
    		_tsnGranularSynth->getTimbreSpace().setTimbreSpaceTree(analysisFileTree);
    		return true;
    	}
    }
    writeToLog("analysis file tree invalid");
    return false;
}

//==============================================================================
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
	return SlicerGranularAudioProcessor::create<TSNGranularAudioProcessor>();
}
