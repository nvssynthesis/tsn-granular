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
,	navigator{ [this](const std::vector<double>& v) -> void
	{
		// set navigator of timbre space component
		auto const p2 = juce::Point<float>(v[0], v[1]);
		// also attract to nearest point
		auto p5 = nvs::util::Timbre5DPoint {	// needs to handle arbitrary dimensions and just attract based on provided dims
			._p2D{p2},
			._p3D{0.f, 0.f, 0.f}
		};
		auto paramName = getParamName(params_e::nav_selection_sharpness);
		
		double const sharpness = (double) *(apvts.getRawParameterValue(paramName));
		
		_timbreSpaceHolder.setProbabilisticPointFromTarget(p5, 8, sharpness);
		setReadBoundsFromChosenPoint();	// needs to affect processor but has final effect on gui
	},
	std::in_place_type<nvs::nav::LFO2D>, apvts, 40.0 }
,	_timbreSpaceNeededData(_timbreSpacialSettings)
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
	
	getNonAutomatableState().addListener(&_timbreSpaceNeededData);
	_analyzer.addChangeListener(&_timbreSpaceNeededData);
}
TsnGranularAudioProcessor::~TsnGranularAudioProcessor() {
	getNonAutomatableState().removeListener(&_timbreSpaceNeededData);
	_analyzer.removeChangeListener(&_timbreSpaceNeededData);
}
//==============================================================================
juce::AudioProcessorEditor* TsnGranularAudioProcessor::createEditor() {
	TsnGranularAudioProcessorEditor* ed = new TsnGranularAudioProcessorEditor (*this);
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

void TsnGranularAudioProcessor::setReadBoundsFromChosenPoint() {
	auto const pIndices = getTimbreSpaceHolder().getCurrentPointIndices();
	auto const onsetOpt = getAnalyzer().getOnsets();

	if (onsetOpt.has_value() and (onsetOpt.value().size() != 0)){
		if (auto *const tsn_synth_juce = dynamic_cast<JuceTsnGranularSynthesizer *const>(_granularSynth.get())){
			tsn_synth_juce->setWaveEvents(pIndices);
		}
	}
}

void TsnGranularAudioProcessor::askForAnalysis(){
	if (_analyzer.Thread::isThreadRunning()){
		_analyzer.stopAnalysis();
	}
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
	juce::String sampleFilePath = nonAutomatableState.getChildWithName("PresetInfo").getProperty("sampleFilePath");
	float sr = 	nonAutomatableState.getChildWithName("PresetInfo").getProperty("sampleRate");

	std::vector<float> onsetsTmp = onsetsOpt.value();
	auto analyzer = _analyzer.getAnalyzer();

	nvs::analysis::denormalizeOnsets(onsetsTmp, nvs::analysis::getLengthInSeconds(wave.size(), sr));
	
	
	nvs::analysis::RunLoopStatus rls;
	nvs::analysis::ShouldExitFn shouldExitFn = [](){return false;};
	nvs::analysis::writeEventsToWav(wave, onsetsTmp, sampleFilePath.toStdString(), analyzer, rls, shouldExitFn);
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


//=======================================================-TimbreSpaceNeededData-=======================================================================
namespace {
std::vector<float> getHistoEqualizationVec(std::vector<float> const &points){
	std::vector<float> allX, vecOut;
	allX.reserve (points.size());
	vecOut.reserve (points.size());
	for (auto& p : points){
		allX.push_back (p);
	}
	std::sort (allX.begin(), allX.end());
	
	for (auto const& p : points)
	{
		// find first index ≥ p.x
		auto idx = (int)(std::lower_bound (allX.begin(),
										   allX.end(),
										   p)
						 - allX.begin());
		float quantile = (float) idx / (float)(allX.size() - 1);
		
		// optional: you can still gamma‑tweak the quantile
		float gamma    = 0.8f;             // <1 stretches mid‑values
		float t        = std::pow (quantile, gamma);
		vecOut.push_back(t);
	}
	return vecOut;
}
}	// end anonymous namespace

void TsnGranularAudioProcessor::TimbreSpaceNeededData::valueTreePropertyChanged (juce::ValueTree &alteredTree, const juce::Identifier &) {
	std::cout << "value tree changed!!!!!\n";
	if (alteredTree.hasType("TimbreSpace")){
		std::cout << "tree changed! redrawing points...\n";
		spacialSettings.histogramEqualization = (double)alteredTree.getProperty("HistogramEqualization");
		auto xs = (juce::String)alteredTree.getProperty("xAxis");
		auto ys = ((juce::String)alteredTree.getProperty("yAxis"));
		std::cout << "x: " << xs << " y: " << ys << '\n';
		spacialSettings.dimensionWisefeatures[0] = nvs::analysis::toFeature(xs);
		spacialSettings.dimensionWisefeatures[1] = nvs::analysis::toFeature(ys);
		extract(spacialSettings.dimensionWisefeatures);
		updateTimbreSpacePoints();
		sendChangeMessage();	// tell editor to repaint timbre points
	}
	else {
		std::cout << "what tree?\n";
	}
}
void TsnGranularAudioProcessor::TimbreSpaceNeededData::changeListenerCallback(juce::ChangeBroadcaster* source) {
	if (auto *a = dynamic_cast<nvs::analysis::ThreadedAnalyzer*>(source)){
		auto onsetsOpt = a->getOnsets();
		if (onsetsOpt.has_value() and onsetsOpt->size()) {
			fullTimbreSpace = a->stealTimbreSpaceRepresentation();
			extract(spacialSettings.dimensionWisefeatures);
			updateTimbreSpacePoints();
		}
		sendChangeMessage();	// THIS should now communicate to editor to paint points
	}
}
void TsnGranularAudioProcessor::TimbreSpaceNeededData::extract(std::vector<nvs::analysis::Features> featuresToExtract) {
	if (!fullTimbreSpace.has_value()){
		std::cerr << "TsnGranularAudioProcessorEditor::TimbreSpaceNeededData::extract: timbre space empty, early exit\n";
		return;
	}
	eventwiseExtractedTimbrePoints.clear();
	eventwiseExtractedTimbrePoints.reserve(fullTimbreSpace->size());
	
	for (auto const &t : fullTimbreSpace.value()) {
		std::vector<float> v = nvs::analysis::extractFeatures(t, featuresToExtract, nvs::analysis::Statistic::Median);
		eventwiseExtractedTimbrePoints.push_back(v);
	}
}
void TsnGranularAudioProcessor::TimbreSpaceNeededData::updateTimbreSpacePoints()
{	/** to be called when the actual analyzed timbre space changes  */
	if (eventwiseExtractedTimbrePoints.size() == 0){
		std::cout << "updateAndDrawTimbreSpacePoints: timbreSpaceNeededData empty, returning...\n";
		return;
	}
	auto &timbreSpaceRepresentation = eventwiseExtractedTimbrePoints;
	{
		ranges.clear();
		ranges.reserve(timbreSpaceRepresentation.size());
		for (size_t i = 0; i < timbreSpaceRepresentation.size(); ++i) {
			ranges.push_back(nvs::analysis::calculateRangeOfDimension(timbreSpaceRepresentation, i));
		}
	}
	{
		std::vector<float> allDim0, allDim1;
		allDim0.reserve(timbreSpaceRepresentation.size());
		allDim1.reserve(timbreSpaceRepresentation.size());
		for (auto const& frame : timbreSpaceRepresentation){
			allDim0.push_back(frame[0]);	// e.g. bfcc1
			allDim1.push_back(frame[1]);	// e.g. bfcc2
		}
		histoEqualizedD0 = getHistoEqualizationVec(allDim0);
		histoEqualizedD1 = getHistoEqualizationVec(allDim1);
	}
}

//==============================================================================
// This creates new instances of the plugin..
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
	return new TsnGranularAudioProcessor();
}
