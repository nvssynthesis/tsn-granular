/*
  ==============================================================================

    ThreadedAnalyzer.cpp
    Created: 1 Nov 2023 3:36:32pm
    Author:  Nicholas Solem

  ==============================================================================
*/

#include "ThreadedAnalyzer.h"
#include "fmt/core.h"
#if 0
namespace nvs {
namespace analysis {

ThreadedAnalyzer::ThreadedAnalyzer()
:	juce::Thread("Analyzer")
{}

void ThreadedAnalyzer::run() {
	switch (_analysisType) {
		case (analysisType_e::onset):
			fmt::print("onset analysis\n");
			calculateOnsets(<#std::vector<float> wave#>)
			break;
		case (analysisType_e::bfcc):
			fmt::print("bfcc analysis\n");
			break;
		case (analysisType_e::pca):
			fmt::print("pca analysis\n");
			break;
	}
}
}	// namespace analysis
}	// namespace nvs
#endif
