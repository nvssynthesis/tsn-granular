/*
  ==============================================================================

    ThreadedAnalyzer.cpp
    Created: 1 Nov 2023 3:36:32pm
    Author:  Nicholas Solem

  ==============================================================================
*/

#include "Analysis/ThreadedAnalyzer.h"
#include "Analysis/OnsetAnalysis/OnsetProcessing.h"
#include "fmt/core.h"

namespace nvs::analysis {

ThreadedAnalyzer::ThreadedAnalyzer()
	:	juce::Thread("Analyzer")
{}
ThreadedAnalyzer::~ThreadedAnalyzer(){
	stopThread(100);
}

void ThreadedAnalyzer::updateStoredAudio(std::span<float const> wave, const juce::String &audioFileAbsPath){
	_inputWave.assign(wave.begin(), wave.end());
	_audioFileAbsPath = audioFileAbsPath;
	_analysisResult.reset();
}

std::optional<ThreadedAnalyzer::AnalysisResult> ThreadedAnalyzer::stealTimbreSpaceRepresentation() {
	return std::exchange(_analysisResult, std::nullopt);
}

void ThreadedAnalyzer::updateSettings(const juce::ValueTree &settingsTree){
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
	if (!(_inputWave.data() && _inputWave.size())){
		return;
	}
	_rls.set(0.0);

	try {
		// let any sub-step know if we’ve been asked to exit:
		auto shouldExit = [this]() {
			const bool retval = threadShouldExit();
			if (retval){
				DBG("ThreadedAnalyzer: exit requested");
			}
			return retval;;
		};

		// perform onset analysis
		_rls.set("Calculating Onsets...");
		const auto onsetOpt = _analyzer.calculateOnsetsInSeconds(_inputWave, _rls, shouldExit);
		jassert (onsetOpt.has_value());
		if (onsetOpt.value().empty()){
			DBG("Threaded Analyzer: zero onsets... returning");
			sendChangeMessage();
			return;
		}

	    auto onsets = onsetOpt.value();

		auto const sr = _analyzer.getAnalyzedFileSampleRate();
		auto lengthInSeconds = getLengthInSeconds(_inputWave.size(), sr);

		filterOnsets(onsets, lengthInSeconds);
	    forceMinimumOnsets(onsets, 4, lengthInSeconds);

		// perform onsetwise BFCC analysis
		_rls.set("Calculating Onsetwise TimbreSpace...");
		auto timbreMeasurementsOpt = _analyzer.calculateOnsetwiseTimbreSpace(_inputWave, onsets, _rls, shouldExit);
		jassert (timbreMeasurementsOpt.has_value());

		normalizeOnsets(onsets, lengthInSeconds);

		_analysisResult.emplace(
		    AnalysisResult{
		        .onsets = onsets,
			    .timbreMeasurements = timbreMeasurementsOpt.value(),
			    .audioHash = nvs::util::hashAudioData(_inputWave),
			    .audioFileAbsPath = _audioFileAbsPath
		    }
		);
		// only NOW do we send change message, and its a single message which should properly cause ALL data to be visualized etc.
		sendChangeMessage();
	} catch (const essentia::EssentiaException& e) {
		DBG("Essentia exception: " << e.what());
		sendChangeMessage(); // Let GUI know something changed
		return;
	}
	catch (const std::exception& e) {
		DBG("Standard exception in analysis thread: " << e.what());
		sendChangeMessage(); // Let GUI know something changed
		return;
	} catch (...) {
		DBG("Unknown exception in analysis thread");
		sendChangeMessage(); // Let GUI know something changed
		return;
	}
}

}
