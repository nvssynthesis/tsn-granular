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

void ThreadedAnalyzer::updateWave(std::span<float> const wave){
	inputWave = wave;
}
void ThreadedAnalyzer::run() {
	// first, clear everything so that if any analysis is terminated early, we don't have garbage leftover
	outputOnsetsInSeconds.clear();
	outputOnsetwiseBFCCs.clear();
	outputPCA.clear();
	if (!(inputWave.data() && inputWave.size())){
		return;
	}
	
	// perform onset analysis
	fmt::print("ThreadedAnalyzer: performing onset analysis\n");
	std::vector<float> v;
	v.assign(inputWave.begin(), inputWave.end());
	auto onsetOpt = _analyzer.calculateOnsets(v, [&](){
		if (threadShouldExit()){
			fmt::print("ONSET CALCULATION EXITED EARLY\n");
			return false;
		}
		return true;
	});
	jassert (onsetOpt.has_value());

	// perform onsetwise BFCC analysis
	fmt::print("ThreadedAnalyzer: performing bfcc analysis\n");
	auto bfccOpt = _analyzer.calculateOnsetwiseBFCCs(v, *onsetOpt);
	jassert (bfccOpt.has_value());
	
	// perform PCA analysis
	fmt::print("ThreadedAnalyzer: performing pca analysis\n");
	auto PCAopt = _analyzer.calculatePCA(*bfccOpt);
	jassert(PCAopt.has_value());
	
	outputOnsetsInSeconds = onsetOpt.value();
	outputOnsetwiseBFCCs = bfccOpt.value();
	outputPCA = PCAopt.value();
	// only NOW do we send change message, and its a single message which should properly cause ALL data to be visualized etc.
	sendChangeMessage();
}
	
}	// namespace analysis
}	// namespace nvs
