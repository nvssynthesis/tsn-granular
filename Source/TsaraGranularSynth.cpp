/*
  ==============================================================================

    TsaraGranularSynth.cpp
    Created: 4 Sep 2023 1:54:00am
    Author:  Nicholas Solem

  ==============================================================================
*/

#include "TsaraGranularSynth.h"
namespace nvs	{
namespace gran	{

	TsaraGranular::TsaraGranular(float const &sampleRate, std::span<float> const &wavespan, size_t nGrains)
	:	genGranPoly1(sampleRate, wavespan, nGrains)
	{}

	

}	// namespace gran
}	// namespace nvs
