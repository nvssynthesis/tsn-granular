#include "TsnGranularPluginProcessor.h"

#include "StringAxiom.h"
#include "TsnGranularPluginEditor.h"
#include "fmt/core.h"
#include "Settings.h"
#include "OnsetAnalysis/OnsetProcessing.h"
#include "TSNValueTreeUtilities.h"

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
	
	ValueTree settingsVT = apvts.state.getOrCreateChildWithName("Settings", nullptr);
	nvs::analysis::initializeSettingsBranches(settingsVT, false);

    _analyzer.addChangeListener(this);

	// initSynth only gets called AFTER construction for reasons,
	// so do not call anything here that requires _tsnGranularSynth to be valid (or likewise,
	// anything that uses _granularSynth in any way).
}
TSNGranularAudioProcessor::~TSNGranularAudioProcessor() {
	_analyzer.removeChangeListener(&_tsnGranularSynth->getTimbreSpace());
    _analyzer.removeChangeListener(this);
    _analyzer.stopAnalysis();
}
//==============================================================================
void TSNGranularAudioProcessor::initSynth() {
    _granularSynth = std::make_unique<TSNGranularSynth>(apvts);
    _tsnGranularSynth = static_cast<TSNGranularSynth*>(_granularSynth.get());
	_analyzer.addChangeListener(&_tsnGranularSynth->getTimbreSpace());
}
AudioProcessorEditor* TSNGranularAudioProcessor::createEditor() {
    const auto ed = new TsnGranularAudioProcessorEditor (*this);
	return ed;
}

void TSNGranularAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
	const std::unique_ptr<XmlElement> xmlState(getXmlFromBinary(data, sizeInBytes));
	if (xmlState == nullptr || !xmlState->hasTagName("PLUGIN_STATE")) {
		return;
	}

	const ValueTree root = ValueTree::fromXml(*xmlState);
	apvts.replaceState(root);	// also causes _presetManager, a ValueTreeListener, to call loadAnalysisFileFromState via valueTreeRedirected

	ensureSettingsStructure();

	writeToLog("setStateInformation fully successful\n");
}

void TSNGranularAudioProcessor::saveAnalysisToFile(const String& filePath, std::function<void(bool)> resultCallback) const {
	// inform plugin state of what the associated analysis file will be
	auto fileInfo = apvts.state.getChildWithName("FileInfo");
	if (!fileInfo.isValid()) return;
	fileInfo.setProperty("analysisFile", filePath, nullptr);

    const auto tsSuperTree = _tsnGranularSynth->getTimbreSpace().getTimbreSpaceSuperTree();

    DBG(fmt::format("tree being SAVED: {}", nvs::util::valueTreeToXmlStringSafe(tsSuperTree).toStdString()));
	bool success = [vt=tsSuperTree, filePath](bool useBinary){
		File const file(filePath);
		if (useBinary){
			return nvs::util::saveValueTreeToBinary(vt, file);
		}
		else {
			return nvs::util::saveValueTreeToJSON(vt, file);
		}
	}(true);
	MessageManager::callAsync([resultCallback, success](){
		resultCallback(success);
	});
}
//==============================================================================
void TSNGranularAudioProcessor::loadAudioFileAndUpdateState(File const f, bool notifyEditor){
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
	if (_analyzer.isThreadRunning()){
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
	
	auto settingsVT = apvts.state.getChildWithName("Settings");
	auto const par = settingsVT.getParent();
	jassert (par.getChildWithName("FileInfo").hasProperty("sampleRate"));
	_analyzer.updateSettings(settingsVT, true);
	
	if (_analyzer.startThread(Thread::Priority::high)){	// only entry point to analysis
		writeToLog("analyzer onset thread started");
	}
}
void TSNGranularAudioProcessor::changeListenerCallback (ChangeBroadcaster *source) {
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

    auto settingsVT = apvts.state.getChildWithName(nvs::axiom::tsn::Settings);
    _analyzer.updateSettings(settingsVT, true);

	auto const buffer = sampleManagementGuts.getSampleBuffer();
	auto const waveSpan = std::span(buffer.getReadPointer(0), static_cast<size_t>(buffer.getNumSamples()));
	std::vector<float> wave(waveSpan.size());
	wave.assign(waveSpan.begin(), waveSpan.end());
	
	// any reason to use getPropertyAsValue instead?

	std::vector<float> onsetsTmp = sharedOnsets->onsets;

    auto const par = settingsVT.getParent();
    const auto fileInfoTree = par.getChildWithName(nvs::axiom::tsn::FileInfo);
    jassert (fileInfoTree.hasProperty(nvs::axiom::tsn::sampleRate));

    const auto sampleFilePath = [fileInfoTree, sharedOnsets]()-> std::string {
        jassert (fileInfoTree.hasProperty(nvs::axiom::tsn::sampleFilePath));
	    const String sfp = fileInfoTree.getProperty(nvs::axiom::tsn::sampleFilePath);
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

    const float sr = fileInfoTree.getProperty(nvs::axiom::tsn::sampleRate);
	nvs::analysis::denormalizeOnsets(onsetsTmp, nvs::analysis::getLengthInSeconds(wave.size(), sr));
	
	nvs::analysis::RunLoopStatus rls;
    const nvs::analysis::ShouldExitFn shouldExitFn = [](){return false;};
	nvs::analysis::writeEventsToWav(wave, onsetsTmp, sampleFilePath, _analyzer.getAnalyzer(), rls, shouldExitFn);
}

void TSNGranularAudioProcessor::processBlock (AudioBuffer<float>& buffer, MidiBuffer& midiMessages) {
	if (!_tsnGranularSynth->getTimbreSpace().hasValidAnalysisFor(sampleManagementGuts.getWaveformHash())) {
		ScopedNoDenormals noDenormals;	// probably not necessary at this point but also doesnt hurt
		for (auto i = getTotalNumInputChannels(); i < getTotalNumOutputChannels(); ++i){
			buffer.clear (i, 0, buffer.getNumSamples());
		}
		
		logRateLimited("TsnGranularAudioProcessor::processBlock: analysis not current, exiting early", 1200);
		return;
	}
	SlicerGranularAudioProcessor::processBlock (buffer, midiMessages);
}

void TSNGranularAudioProcessor::ensureSettingsStructure() {
    if (const auto settings = apvts.state.getChildWithName(nvs::axiom::tsn::Settings);
        !nvs::analysis::verifySettingsStructure(settings))
    {
        ValueTree settingsVT = apvts.state.getOrCreateChildWithName(nvs::axiom::tsn::Settings, nullptr);
        nvs::analysis::initializeSettingsBranches(settingsVT);
    }
}
bool TSNGranularAudioProcessor::loadAnalysisFileFromState() {
    // TODO: Return more particular failure/success and handle each case. E.g. there could be auto-search in
    /// designated directory, manual find, and give up (just perform fresh analysis)
    const auto fileInfo = apvts.state.getChildWithName(nvs::axiom::tsn::FileInfo);
	writeToLog(fmt::format("FileInfo: {}", nvs::util::valueTreeToXmlStringSafe(fileInfo).toStdString()));
    if (!fileInfo.isValid()) {
		writeToLog("file info value tree invalid\n");
    	return false;
    }
    const String analysisFilePath = fileInfo.getProperty(nvs::axiom::tsn::analysisFile, {});
	writeToLog(fmt::format("analysisFilePath: {}", analysisFilePath.toStdString()));

    if (analysisFilePath.isEmpty()) {
    	writeToLog("analysisFilePath is empty");
	    return false;
    }
    const ValueTree analysisSuperVT = nvs::util::loadValueTreeFromBinary(File(analysisFilePath));

    jassert(analysisSuperVT.hasType(nvs::axiom::tsn::super));

    if (!analysisSuperVT.isValid()) {
        writeToLog("analysis file tree invalid");
        return false; // TODO: Give popup opportunity for user to find the file
    }
    if (auto metadataTree = analysisSuperVT.getChildWithName(nvs::axiom::tsn::Metadata);
        metadataTree.isValid() &&
        nvs::util::getAndMigrateAudioHash(metadataTree) == getAudioHash())
    {
        const auto analysisVT = analysisSuperVT.getChildWithName(nvs::axiom::tsn::TimbreAnalysis);
        if (!analysisVT.isValid()) {
            writeToLog("analysis file tree invalid");
            return false;
        }
     	writeToLog("setting via setStateInformation");
     	_tsnGranularSynth->getTimbreSpace().setTimbreSpaceSuperTree(analysisSuperVT);
     	return true;
    }
    writeToLog("analysis file tree metadata mismatch");
    return false;
}

//==============================================================================
AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
	return SlicerGranularAudioProcessor::create<TSNGranularAudioProcessor>();
}
