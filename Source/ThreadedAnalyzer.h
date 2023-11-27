/*
  ==============================================================================

    ThreadedAnalyzer.h
    Created: 1 Nov 2023 3:36:32pm
    Author:  Nicholas Solem

  ==============================================================================
*/

#pragma once
#include "Analyzer.h"
#include <JuceHeader.h>

namespace nvs {
namespace analysis {

class ThreadedAnalyzer	:	public juce::Thread
{
public:
	ThreadedAnalyzer();
	void run() override;
	
	enum analysisType_e {
		onset,
		onsetwise_bfcc,
		pca
	};
	void setAnalysisType(analysisType_e a) {
		_analysisType = a;
	}
	void updateWave(vecReal &wave){
		inputWave = &wave;
	}
	void updateOnsets(std::vector<float> &onsetsInSeconds){
		inputOnsetsInSeconds = &onsetsInSeconds;
	}
	void updateOnsetwiseBFCCs(vecVecReal &onsetwiseBFCCs){
		inputOnsetwiseBFCCs = &onsetwiseBFCCs;
	}
	
	inline std::vector<float> getOnsetsInSeconds() const {
		return outputOnsetsInSeconds;
	}
	inline vecVecReal getOnsetwiseBFCCs() const {
		return outputOnsetwiseBFCCs;
	}
	inline vecVecReal getPCA() const {
		return outputPCA;
	}
private:
	Analyzer _analyzer;
	analysisType_e _analysisType { analysisType_e::onset };
	
	vecReal *inputWave {nullptr};
	std::vector<float> *inputOnsetsInSeconds {nullptr};
	vecVecReal *inputOnsetwiseBFCCs {nullptr};
	
	std::vector<float> outputOnsetsInSeconds;
	vecVecReal outputOnsetwiseBFCCs;
	vecVecReal outputPCA;
};

}	// namespace analysis
}	// namespace nvs
