/*
  ==============================================================================

    TsaraGranularSynth.h
    Created: 4 Sep 2023 1:54:00am
    Author:  Nicholas Solem

  ==============================================================================
*/

#pragma once
#include "EssentiaSetup.h"
#include "GranularSynthesis.h"
#include "onsetAnalysis/OnsetAnalysis.h"
#include "Analyzer.h"
#include <JuceHeader.h>


/** TODO:
	-bake in some way to be sure that the currently held onsets match the current span
	
 */
//void writeEventsToWav(std::vector<float> wave, std::vector<float> onsetsInSeconds, std::string_view ogPath, essentia::streaming::AlgorithmFactory const &factory, nvs::analysis::splitSettings settings);

namespace nvs	{
namespace gran	{
class TsaraGranular		:	public genGranPoly1
{
public:
	TsaraGranular(double const &sampleRate, std::span<float> const &wavespan, size_t nGrains);

	void loadOnsets(std::span<float const> const onsetsInSeconds){
		_onsetsInSeconds = onsetsInSeconds;
	}
#if 0
	void writeEventsToWav(std::string_view ogPath, nvs::analysis::Analyzer &analyzer) {
		if (!_wavespan.data() | !_onsetsInSeconds.data()){
			std::cerr << "TsaraGranular::writeEventsToWav attempt using null data\n";
			return;
		}
		std::vector<float> wave(_wavespan.size());
		wave.assign(_wavespan.begin(), _wavespan.end());
		
		std::vector<float> onsets(_onsetsInSeconds.size());
		onsets.assign(_onsetsInSeconds.begin(), _onsetsInSeconds.end());
		nvs::analysis::writeEventsToWav(wave, onsets, ogPath, analyzer.ess_hold.factory, analyzer._splitSettings);
	}
#endif
private:
	std::span<float const> _onsetsInSeconds;
};

}	// namespace gran
}	// namespace nvs
