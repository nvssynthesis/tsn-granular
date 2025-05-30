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
	void updateWave(std::span<float const> wave, size_t eventualFilenameHash);
	void run() override;
	//===============================================================================
	inline std::optional<std::vector<FeatureContainer<EventwiseStatistics<Real>>>> stealTimbreSpaceRepresentation()
	{
		auto ret = std::move(_outputOnsetwiseTimbreMeasurements);
		_outputOnsetwiseTimbreMeasurements.reset();
		return ret;
	}

	inline std::optional<vecReal> getOnsets() const {
		return _outputOnsets;
	}
	//===============================================================================
	inline void updateSettings(juce::ValueTree settingsTree){
		jassert( settingsTree.hasType("Settings") );
		_analyzer.updateSettings(settingsTree);
	}
	//===============================================================================
	Analyzer &getAnalyzer() {
		return _analyzer;
	}
	//===============================================================================
	size_t getFilenameHash() const {
		return _filenameHash;
	}
	//===============================================================================
	void stopAnalysis() { signalThreadShouldExit(); }
	RunLoopStatus &getStatus() noexcept { return rls; }
private:
	Analyzer _analyzer;
	
	vecReal _inputWave;
	
	std::optional<vecReal> _outputOnsets;
	std::optional<std::vector<FeatureContainer<EventwiseStatistics<Real>>>> _outputOnsetwiseTimbreMeasurements;
	
	size_t _filenameHash;
	size_t _eventualFilenameHash;
	
	RunLoopStatus rls;
};

}	// namespace analysis
}	// namespace nvs
