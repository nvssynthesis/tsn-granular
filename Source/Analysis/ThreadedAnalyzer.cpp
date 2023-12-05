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

ThreadedAnalyzer::ThreadedAnalyzer(juce::ChangeListener *listener)
:	juce::Thread("Analyzer")
{
	addChangeListener(listener);
}

void ThreadedAnalyzer::run() {
	switch (_analysisType) {
		case (analysisType_e::onset):
			fmt::print("onset analysis\n");
			if (inputWave){
				auto onsetOpt = _analyzer.calculateOnsets(*inputWave, [&](){
					fmt::print("calculating onsets!\n");
					if (threadShouldExit()){
						return false;
					}
					return true;
				});
				
				if (onsetOpt.has_value()){
					outputOnsetsInSeconds = onsetOpt.value();
					sendChangeMessage();
				}
			}
			break;
		case (analysisType_e::onsetwise_bfcc):
			fmt::print("bfcc analysis\n");
			if (inputWave && inputOnsetsInSeconds){
				auto bfccOpt = _analyzer.calculateOnsetwiseBFCCs(*inputWave, *inputOnsetsInSeconds);
				if (bfccOpt.has_value()){
					outputOnsetwiseBFCCs = bfccOpt.value();
					sendChangeMessage();
				}
			}
			break;
		case (analysisType_e::pca):
			fmt::print("pca analysis\n");
			if (inputOnsetwiseBFCCs){
				auto PCAopt = _analyzer.calculatePCA(*inputOnsetwiseBFCCs);
				if (PCAopt.has_value()){
					outputPCA = PCAopt.value();
					sendChangeMessage();
				}
			}
			break;
	}
	
}
}	// namespace analysis
}	// namespace nvs
