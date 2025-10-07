#include "TsnGranularPluginProcessor.h"
#include "TsnGranularPluginEditor.h"
#include "fmt/core.h"
#include "Analysis/Settings.h"

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

	_analyzer.addChangeListener(&_tsnGranularSynth->getTimbreSpace());
}
TSNGranularAudioProcessor::~TSNGranularAudioProcessor() {
	_analyzer.removeChangeListener(&_tsnGranularSynth->getTimbreSpace());
}
//==============================================================================
void TSNGranularAudioProcessor::initSynth() {
    _granularSynth = std::make_unique<TSNGranularSynth>(apvts);
    _tsnGranularSynth = static_cast<TSNGranularSynth*>(_granularSynth.get());
}
juce::AudioProcessorEditor* TSNGranularAudioProcessor::createEditor() {
    const auto ed = new TsnGranularAudioProcessorEditor (*this);
	return ed;
}

void TSNGranularAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
	std::unique_ptr<juce::XmlElement> xmlState(getXmlFromBinary(data, sizeInBytes));
	if (xmlState == nullptr || !xmlState->hasTagName("PLUGIN_STATE")) {
		return;
	}

	const juce::ValueTree root = juce::ValueTree::fromXml(*xmlState);

	apvts.replaceState(root);
	
	ensureSettingsStructure();
	loadAnalysisFileFromState();
	
	writeToLog("setStateInformation fully successful\n");
}
void TSNGranularAudioProcessor::saveAnalysisToFile(const juce::String& filePath, std::function<void(bool)> resultCallback) const {
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
	
	_tsnGranularSynth->getTimbreSpace().setAudioPaths(f.getFullPathName());

	if (_tsnGranularSynth->getTimbreSpace().hasValidAnalysisFor(sampleManagementGuts.getHash())) {
		/* do nothing */
		fmt::print("TSNGranularAudioProcessor::loadAudioFile: already had valid analysis for {}\n", f.getFullPathName().toStdString());
	}
	else if (!loadAnalysisFileFromState())	// try to load analysis from state; if it fails then do fresh analysis
	{
		askForAnalysis();
	}
	writeToLog("TSN: loadAudioFile exiting");
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
	auto const par = settingsVT.getParent();
	jassert (par.getChildWithName("FileInfo").hasProperty("sampleRate"));
	_analyzer.updateSettings(settingsVT);
	
	if (_analyzer.startThread(juce::Thread::Priority::high)){	// only entry point to analysis
		writeToLog("analyzer onset thread started");
	}
}

void TSNGranularAudioProcessor::writeEvents(){
	auto const onsetsOpt = _tsnGranularSynth->getTimbreSpace().getOnsets();
	if (!onsetsOpt.has_value()){
		writeToLog("writeEvents failed: onsets optional does not contain value");
		return;
	}
	auto const buffer = sampleManagementGuts.getSampleBuffer();
	auto const waveSpan = std::span<float const>(buffer.getReadPointer(0), buffer.getNumSamples());
	std::vector<float> wave(waveSpan.size());
	wave.assign(waveSpan.begin(), waveSpan.end());
	
	// any reason to use getPropertyAsValue instead?
    const juce::String sampleFilePath = apvts.state.getProperty("sampleFilePath");
    const float sr = 	apvts.state.getProperty("sampleRate");

	std::vector<float> onsetsTmp = onsetsOpt.value();

	nvs::analysis::denormalizeOnsets(onsetsTmp, nvs::analysis::getLengthInSeconds(wave.size(), sr));
	
	nvs::analysis::RunLoopStatus rls;
    const nvs::analysis::ShouldExitFn shouldExitFn = [](){return false;};
	nvs::analysis::writeEventsToWav(wave, onsetsTmp, sampleFilePath.toStdString(), _analyzer.getAnalyzer(), rls, shouldExitFn);
}

void TSNGranularAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages) {
	if (!_tsnGranularSynth->getTimbreSpace().hasValidAnalysisFor(sampleManagementGuts.getHash())) {
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
    if (const auto settings = apvts.state.getChildWithName("Settings");
        !nvs::analysis::verifySettingsStructure(settings))
    {
        juce::ValueTree settingsVT = apvts.state.getOrCreateChildWithName("Settings", nullptr);
        nvs::analysis::initializeSettingsBranches(settingsVT);
    }
}
bool TSNGranularAudioProcessor::loadAnalysisFileFromState()
    // TODO: Return more particular failure/success and handle each case. E.g. there could be auto-search in designated directory, manual find, and give up (just perform fresh analysis)
    {
    const auto fileInfo = apvts.state.getChildWithName("FileInfo");
    if (!fileInfo.isValid())
        return false;

    juce::String analysisFilePath = fileInfo.getProperty("analysisFile", {});
    if (analysisFilePath.isEmpty())
        return false;

    const juce::File file(analysisFilePath);
    auto fstream = juce::FileInputStream(file);

    if (fstream.failedToOpen()) {
        writeToLog(fstream.getStatus().getErrorMessage());
        // TODO: Give popup opportunity for user to find the file
        return false;
    }

    const auto analysisFileStream = juce::ValueTree::readFromStream(fstream);

    if (const auto analysisFileTree = analysisFileStream.getChildWithName("TimbreAnalysis");
        analysisFileTree.isValid())
    {
        fmt::print("setting via setStateInformation\n");
        _tsnGranularSynth->getTimbreSpace().setTimbreSpaceTree(analysisFileTree);
    }
    return true;
    }

//==============================================================================
// This creates new instances of the plugin..
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
	return SlicerGranularAudioProcessor::create<TSNGranularAudioProcessor>();
}
