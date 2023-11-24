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

#if 0
namespace nvs {
namespace analysis {

class ThreadedAnalyzer	:	public Analyzer
,							public juce::Thread
{
public:
	ThreadedAnalyzer();
	void run() override;
	
	enum analysisType_e {
		onset,
		bfcc,
		pca
	};
	void setAnalysisType(analysisType_e a) {
		_analysisType = a;
	}
	
private:
	analysisType_e _analysisType { analysisType_e::onset };
	
};

}	// namespace analysis
}	// namespace nvs
#endif
