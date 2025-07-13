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
	};
	//===============================================================================
	ThreadedAnalyzer(juce::ChangeListener *listener);
	~ThreadedAnalyzer();
	//===============================================================================
	void updateWave(std::span<float const> wave);
	void updateSettings(juce::ValueTree settingsTree);
	//===============================================================================
	void run() override;
	void stopAnalysis() { signalThreadShouldExit(); }
	//===============================================================================
	inline std::optional<AnalysisResult> stealTimbreSpaceRepresentation() {
		return std::exchange(_analysisResult, std::nullopt);
	}
	//===============================================================================
	Analyzer &getAnalyzer() { return _analyzer; }
	RunLoopStatus &getStatus() noexcept { return _rls; }
	//===============================================================================
private:
	Analyzer _analyzer;
	
	vecReal _inputWave;
	
	std::optional<AnalysisResult> _analysisResult;
	juce::String _settingsHash {};
	
	RunLoopStatus _rls;
};

}	// namespace analysis
}	// namespace nvs
