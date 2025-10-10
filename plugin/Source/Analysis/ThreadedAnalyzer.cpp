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

ThreadedAnalyzer::ThreadedAnalyzer()
:	juce::Thread("Analyzer")
{}
ThreadedAnalyzer::~ThreadedAnalyzer(){
	stopThread(100);
}

void ThreadedAnalyzer::updateWave(std::span<float const> wave){
	_inputWave.assign(wave.begin(), wave.end());
	_analysisResult.reset();
}

void ThreadedAnalyzer::updateSettings(juce::ValueTree settingsTree){
	jassert( settingsTree.hasType("Settings") );

	if (_analyzer.updateSettings(settingsTree)){
		_settingsHash = util::hashValueTree(settingsTree);
	}
	else {
		jassertfalse;
	}
}

void ThreadedAnalyzer::run() {
	// first, clear everything so that if any analysis is terminated early, we don't have garbage leftover
	_analysisResult.reset();
	analysisError.store(false); // Reset at start
	if (!(_inputWave.data() && _inputWave.size())){
		return;
	}
	_rls.set(0.0);

	try {
		// let any sub-step know if we’ve been asked to exit:
		auto shouldExit = [this]() {
			bool retval = threadShouldExit();
			if (retval){
				DBG("ThreadedAnalyzer: exit requested");
			}
			return retval;;
		};

		// perform onset analysis
		_rls.set("Calculating Onsets...");
		auto onsetOpt = _analyzer.calculateOnsetsInSeconds(_inputWave, _rls, shouldExit);
		jassert (onsetOpt.has_value());

		if (onsetOpt.value().empty()){
			DBG("Threaded Analyzer: zero onsets... returning");
			sendChangeMessage();
			return;
		}

		auto const sr = _analyzer.getAnalyzedFileSampleRate();
		auto lengthInSeconds = getLengthInSeconds(_inputWave.size(), sr);

		filterOnsets(onsetOpt.value(), lengthInSeconds);

		// perform onsetwise BFCC analysis
		_rls.set("Calculating Onsetwise TimbreSpace...");
		auto timbreMeasurementsOpt = _analyzer.calculateOnsetwiseTimbreSpace(_inputWave, *onsetOpt, _rls, shouldExit);
		jassert (timbreMeasurementsOpt.has_value());


		normalizeOnsets(onsetOpt.value(), lengthInSeconds);

		_analysisResult.emplace(AnalysisResult{
			.onsets = onsetOpt.value(),
			.timbreMeasurements = timbreMeasurementsOpt.value(),
			.audioHash = nvs::util::hashAudioData(_inputWave)
		});
		// only NOW do we send change message, and its a single message which should properly cause ALL data to be visualized etc.
		sendChangeMessage();
	} catch (const essentia::EssentiaException& e) {
		DBG("Essentia exception: " << e.what());
		analysisError.store(true);  // Signal error occurred
		sendChangeMessage(); // Let GUI know something changed
		return;
	}
	 catch (const std::exception& e) {
		DBG("Standard exception in analysis thread: " << e.what());
		analysisError.store(true);
	 	sendChangeMessage(); // Let GUI know something changed
		return;
	} catch (...) {
		DBG("Unknown exception in analysis thread");
		analysisError.store(true);
		sendChangeMessage(); // Let GUI know something changed
		return;
	}
}

}	// namespace analysis
}	// namespace nvs
