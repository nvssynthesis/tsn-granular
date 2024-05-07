/*
  ==============================================================================

    TsaraGranularSynth.h
    Created: 4 Sep 2023 1:54:00am
    Author:  Nicholas Solem

  ==============================================================================
*/

#pragma once
#include "Analysis/EssentiaSetup.h"
#include "../slicer_granular/Source/Synthesis/GranularSynthesis.h"
#include "Analysis/OnsetAnalysis/OnsetAnalysis.h"
#include "Analysis/Analyzer.h"
#include <JuceHeader.h>


/** TODO:
	-bake in some way to be sure that the currently held onsets match the current span
*/
static constexpr size_t N_GRAINS =
#if defined(DEBUG_BUILD) | defined(DEBUG) | defined(_DEBUG)
								15;
#else
								30;
#endif

namespace nvs	{
namespace gran	{
class TsaraGranular		:	public genGranPoly1
{
public: 
	TsaraGranular(double const &sampleRate, std::span<float> const &wavespan,
											double const&fileSampleRate, unsigned long seed = 1234567890UL);
	virtual ~TsaraGranular() = default;
	void loadOnsets(std::span<float> const onsetsInSeconds);
private:
	std::vector<float> _onsetsNormalized;
	
	void doSetPosition(double position) override;
	void doSetPositionRandomness(double rand) override;
	void doSetDuration(double dur) override;
	void doSetDurationRandomness(double rand) override;
};

}	// namespace gran
}	// namespace nvs
