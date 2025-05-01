/*
  ==============================================================================

    ThreadedAnalyzer.cpp
    Created: 1 Nov 2023 3:36:32pm
    Author:  Nicholas Solem

  ==============================================================================
*/

#include "Analysis/ThreadedAnalyzer.h"
#include "fmt/core.h"

namespace nvs {
namespace analysis {

ThreadedAnalyzer::ThreadedAnalyzer(juce::ChangeListener *listener)
:	juce::Thread("Analyzer")
{
	addChangeListener(listener);
}

void ThreadedAnalyzer::updateWave(std::span<float const> wave, size_t eventualFilenameHash){
	_inputWave.assign(wave.begin(), wave.end());
	_eventualFilenameHash = eventualFilenameHash;
}

void ThreadedAnalyzer::run() {
	// first, clear everything so that if any analysis is terminated early, we don't have garbage leftover
	_outputOnsets.clear();
	_outputOnsetwiseTimbreMeasurements.clear();
	_outputPCA.clear();
	if (!(_inputWave.data() && _inputWave.size())){
		return;
	}
	
	// perform onset analysis
	fmt::print("ThreadedAnalyzer: performing onset analysis\n");
	auto onsetOpt = _analyzer.calculateOnsetsInSeconds(_inputWave, [&](){
		if (threadShouldExit()){
			fmt::print("ONSET CALCULATION EXITED EARLY\n");
			return false;
		}
		return true;
	});
	jassert (onsetOpt.has_value());

	if (!onsetOpt.value().size()){
		fmt::print("Threaded Analyzer: zero onsets... returning\n");
		sendChangeMessage();
		return;
	}
	// perform onsetwise BFCC analysis
	fmt::print("ThreadedAnalyzer: performing bfcc analysis\n");
	auto timbreMeasurementsOpt = _analyzer.calculateOnsetwiseTimbreSpace(_inputWave, *onsetOpt);
	jassert (timbreMeasurementsOpt.has_value());
	
	if (timbreMeasurementsOpt.value().size() <= 1){
		fmt::print("Threaded Analyzer: too few onsetwise BFCCs to calculate PCA... returning\n");
		sendChangeMessage();
		return;
	}
	// perform PCA analysis
	fmt::print("ThreadedAnalyzer: performing pca analysis\n");
	vecVecReal eventwiseBFCCs;
	eventwiseBFCCs.reserve(timbreMeasurementsOpt->size());

	auto PCAopt = _analyzer.calculatePCA(*timbreMeasurementsOpt, bfccSet, Statistic::Median);
	jassert(PCAopt.has_value());
	
	auto lengthInSeconds = getLengthInSeconds(_inputWave.size(), _analyzer._analysisSettings.sampleRate);
	// filter onsets here
	filterOnsets(onsetOpt.value(), lengthInSeconds);
	// normalize onsets here
	normalizeOnsets(onsetOpt.value(), lengthInSeconds);
	
	_outputOnsets = onsetOpt.value();
	_outputOnsetwiseTimbreMeasurements = timbreMeasurementsOpt.value();
	_outputPCA = PCAopt.value();
	_filenameHash = _eventualFilenameHash;	// successful completion of each field updates the effective hash to that corresponding to the filepath used.
	// only NOW do we send change message, and its a single message which should properly cause ALL data to be visualized etc.
	sendChangeMessage();
}
	
std::vector<FeatureContainer<EventwiseStatistics<Real>>> ThreadedAnalyzer::getTimbreSpaceRepresentation() const {
	switch (_analyzer._analysisSettings.dimensionalityReduction) {
			
		case analysisSettings::PCA:
			jassertfalse;	// need to engineer good way to do this
			return {};
			
		case analysisSettings::noReduction:
			return _outputOnsetwiseTimbreMeasurements;
		
		default:
			jassertfalse;
			return {};
	}
}

}	// namespace analysis
}	// namespace nvs
