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
	ThreadedAnalyzer(juce::ChangeListener *listener);
	~ThreadedAnalyzer(){
		stopThread(100);
	}
	void updateWave(std::span<float const> wave);
	void run() override;
	//===============================================================================
	inline std::optional<std::vector<FeatureContainer<EventwiseStatistics<Real>>>> stealTimbreSpaceRepresentation()
	{
		auto ret = std::move(_outputOnsetwiseTimbreMeasurements);
		_outputOnsetwiseTimbreMeasurements.reset();
		return ret;
	}

	inline std::optional<vecReal> stealOnsets() {
//		auto ret = std::move(_outputOnsets);
//		_outputOnsets.reset();
//		return ret;
		return std::exchange(_outputOnsets, std::nullopt);
	}
	//===============================================================================
	void updateSettings(juce::ValueTree settingsTree);
	bool isAnalysisCurrent() const {
		return _analysisIsCurrent.load();
	}
	//===============================================================================
	Analyzer &getAnalyzer() {
		return _analyzer;
	}
	//===============================================================================
	void stopAnalysis() { signalThreadShouldExit(); }
	RunLoopStatus &getStatus() noexcept { return rls; }
private:
	Analyzer _analyzer;
	
	vecReal _inputWave;
	
	std::optional<vecReal> _outputOnsets;
	std::optional<std::vector<FeatureContainer<EventwiseStatistics<Real>>>> _outputOnsetwiseTimbreMeasurements;
	
	std::atomic<bool> _analysisIsCurrent;
	
	RunLoopStatus rls;
};

}	// namespace analysis
}	// namespace nvs
