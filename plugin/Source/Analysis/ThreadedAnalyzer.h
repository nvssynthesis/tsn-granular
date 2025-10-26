/*
  ==============================================================================

    ThreadedAnalyzer.h
    Created: 1 Nov 2023 3:36:32pm
    Author:  Nicholas Solem

  ==============================================================================
*/

#pragma once
#include "Analysis/Analyzer.h"
#include <JuceHeader.h>

namespace nvs {
namespace analysis {

class ThreadedAnalyzer	:	public juce::Thread
,							public juce::ChangeBroadcaster
{
public:
	//===============================================================================
	struct AnalysisResult {
		vecReal onsets;
		std::vector<FeatureContainer<EventwiseStatistics<Real>>> timbreMeasurements;
		
		juce::String audioHash {};
		juce::String audioFileAbsPath {};
	};
	//===============================================================================
	ThreadedAnalyzer();
	~ThreadedAnalyzer() override;
	//===============================================================================
	void updateStoredAudio(std::span<float const> wave, const juce::String &audioFileAbsPath);
	void updateSettings(const juce::ValueTree &settingsTree);
	//===============================================================================
	void run() override;
	void stopAnalysis() { signalThreadShouldExit(); }
	//===============================================================================
	std::optional<AnalysisResult> stealTimbreSpaceRepresentation();
	//===============================================================================
	Analyzer &getAnalyzer() { return _analyzer; }
	RunLoopStatus &getStatus() noexcept { return _rls; }
	juce::String getSettingsHash() const noexcept { return _analyzer.getSettingsHash(); }
	//===============================================================================
private:
	Analyzer _analyzer;
	
	vecReal _inputWave;
	
	std::optional<AnalysisResult> _analysisResult;
	juce::String _audioFileAbsPath {};

	RunLoopStatus _rls;
};

}	// namespace analysis
}	// namespace nvs
