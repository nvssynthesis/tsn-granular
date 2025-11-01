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
	stopThread(5000);
}

void ThreadedAnalyzer::updateStoredAudio(std::span<float const> wave, const juce::String &audioFileAbsPath){
	_inputWave.assign(wave.begin(), wave.end());
	_audioFileAbsPath = audioFileAbsPath;
    _onsetAnalysisResult.reset();
    _timbreAnalysisResult.reset();
}
auto ThreadedAnalyzer::shareOnsetAnalysis() -> std::shared_ptr<OnsetAnalysisResult> {
    return _onsetAnalysisResult;
}
auto ThreadedAnalyzer::stealTimbreSpaceRepresentation() -> std::optional<TimbreAnalysisResult>{
	return std::exchange(_timbreAnalysisResult, std::nullopt);
}

void ThreadedAnalyzer::updateSettings(const juce::ValueTree &settingsTree){
	jassert( settingsTree.hasType("Settings") );

	if (!_analyzer.updateSettings(settingsTree))
	{
	    DBG("updateSettings failed");
	    jassertfalse;
	}
}

void ThreadedAnalyzer::run() {
	// first, clear everything so that if any analysis is terminated early, we don't have garbage leftover
    _onsetAnalysisResult.reset();
    _timbreAnalysisResult.reset();
	if (!(_inputWave.data() && !_inputWave.empty())){
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
	    const String audioHash = util::hashAudioData(_inputWave);

	    const auto unnormalizedOnsets = [this, shouldExit, audioHash]()-> vecReal {
	        const auto onsetOpt = _analyzer.calculateOnsetsInSeconds(_inputWave, _rls, shouldExit);
		    jassert(onsetOpt.has_value());
		    if (onsetOpt.value().empty()) {
		        DBG("Threaded Analyzer: zero onsets... returning");
		        sendChangeMessage();
		        return {};
		    }

		    _onsetAnalysisResult = std::make_shared<OnsetAnalysisResult>(
                OnsetAnalysisResult{
                    .onsets = onsetOpt.value(),
                    .audioHash = audioHash,
                    .audioFileAbsPath = _audioFileAbsPath
                }
            );

		    auto const sr = _analyzer.getAnalyzedFileSampleRate();
		    const auto lengthInSeconds = getLengthInSeconds(_inputWave.size(), sr);

		    filterOnsets(_onsetAnalysisResult->onsets, lengthInSeconds);
		    forceMinimumOnsets(_onsetAnalysisResult->onsets, 4, lengthInSeconds);

		    const auto retval = _onsetAnalysisResult->onsets;
		    normalizeOnsets(_onsetAnalysisResult->onsets, lengthInSeconds);
		    sendChangeMessage();
	        return retval;
	    }();
	    if (unnormalizedOnsets.empty()) {
	        DBG("Threaded Analyzer: zero onsets... returning");
	        return;
	    }

        // perform onsetwise BFCC analysis
		_rls.set("Calculating Onsetwise TimbreSpace...");
	    {
	        const auto timbreMeasurementsOpt = _analyzer.calculateOnsetwiseTimbreSpace(_inputWave, unnormalizedOnsets, _rls, shouldExit);
		    if (!timbreMeasurementsOpt.has_value()) {
		        DBG("no timbre measurement accomplished, likely due to early exit");
		        sendChangeMessage();
		        return;
		    }

		    _timbreAnalysisResult.emplace(
                TimbreAnalysisResult {
                    .timbreMeasurements = timbreMeasurementsOpt.value(),
                    .audioHash = audioHash,
                    .audioFileAbsPath = _audioFileAbsPath
                }
            );
		    // only NOW do we send change message, and its a single message which should properly cause ALL data to be visualized etc.
		    sendChangeMessage();
	    }
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
