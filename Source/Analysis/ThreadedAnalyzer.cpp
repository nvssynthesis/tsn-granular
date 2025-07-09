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
	if (!_outputOnsets){
		_outputOnsets.emplace();    // default‑construct an empty vector
	}
	else {
		_outputOnsets->clear();
	}
	
	if (!_outputOnsetwiseTimbreMeasurements){
		_outputOnsetwiseTimbreMeasurements.emplace();    // default‑construct empty vector
	}
	else {
		_outputOnsetwiseTimbreMeasurements->clear();
	}
	
	if (!(_inputWave.data() && _inputWave.size())){
		return;
	}
	rls.set(0.0);
	// let any sub-step know if we’ve been asked to exit:
	auto shouldExit = [this]() {
		bool retval = threadShouldExit();
		if (retval){
			std::cout << "ThreadedAnalyzer: exit requested \n";
		}
		return retval;;
	};

	
	// perform onset analysis
	rls.set("Calculating Onsets...");
	auto onsetOpt = _analyzer.calculateOnsetsInSeconds(_inputWave, rls, shouldExit);
	jassert (onsetOpt.has_value());

	if (!onsetOpt.value().size()){
		fmt::print("Threaded Analyzer: zero onsets... returning\n");
		sendChangeMessage();
		return;
	}
	auto lengthInSeconds = getLengthInSeconds(_inputWave.size(), _analyzer.getAnalyzedFileSampleRate());
	
	filterOnsets(onsetOpt.value(), lengthInSeconds);

	// perform onsetwise BFCC analysis
	rls.set("Calculating Onsetwise TimbreSpace...");
	auto timbreMeasurementsOpt = _analyzer.calculateOnsetwiseTimbreSpace(_inputWave, *onsetOpt, rls, shouldExit);
	jassert (timbreMeasurementsOpt.has_value());
	

	normalizeOnsets(onsetOpt.value(), lengthInSeconds);
	
	_outputOnsets = onsetOpt;
	_outputOnsetwiseTimbreMeasurements = timbreMeasurementsOpt;
	_filenameHash = _eventualFilenameHash;	// successful completion of each field updates the effective hash to that corresponding to the filepath used.
	// only NOW do we send change message, and its a single message which should properly cause ALL data to be visualized etc.
	sendChangeMessage();
}

}	// namespace analysis
}	// namespace nvs
