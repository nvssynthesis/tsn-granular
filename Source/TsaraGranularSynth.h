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
#include <JuceHeader.h>

namespace nvs	{
namespace gran	{
class TsaraGranular		:	public genGranPoly1
{
public:
	TsaraGranular(float const &sampleRate, std::span<float> const &wavespan, size_t nGrains);

	void getOnsets();
	void writeEventsToWav(std::string_view defName);

private:
	std::vector<float> onsetsSeconds;
	
	nvs::ess::EssentiaInitializer ess_init;
	nvs::ess::EssentiaHolder ess_hold;
	analysis::analysisSettings _analysisSettings;
	analysis::onsetSettings _onsetSettings;
	analysis::splitSettings _splitSettings;
	
};

}	// namespace gran
}	// namespace nvs
