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
	void run() override;

	void updateWave(std::span<float> const wave);
	
	inline std::vector<float> getOnsetsInSeconds() const {
		return outputOnsetsInSeconds;
	}
	inline vecVecReal getOnsetwiseBFCCs() const {
		return outputOnsetwiseBFCCs;
	}
	inline vecVecReal getPCA() const {
		return outputPCA;
	}
	inline void setAnalysisSettings(analysisSettings settings){
		_analyzer._analysisSettings = settings;
	}
	inline void setOnsetSettings(onsetSettings settings){
		_analyzer._onsetSettings = settings;
	}
	inline void setSplitSettings(splitSettings settings){
		_analyzer._splitSettings = settings;
	}
	inline void setBFCCSettings(bfccSettings settings){
		_analyzer._bfccSettings = settings;
	}
	analysisSettings getAnalysisSettings() const {
		return _analyzer._analysisSettings;
	}
	onsetSettings getOnsetSettings() const {
		return _analyzer._onsetSettings;
	}
	splitSettings getSplitSettings() const {
		return _analyzer._splitSettings;
	}
	bfccSettings getBFCCSettings() const {
		return _analyzer._bfccSettings;
	}
	Analyzer &getAnalyzer() {
		return _analyzer;
	}
private:
	Analyzer _analyzer;
	
	std::span<float> inputWave;
	
	std::vector<float> outputOnsetsInSeconds;
	vecVecReal outputOnsetwiseBFCCs;
	vecVecReal outputPCA;
};

}	// namespace analysis
}	// namespace nvs
