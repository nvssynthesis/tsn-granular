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

TsaraGranular::TsaraGranular(double const &sampleRate, std::span<float> const &wavespan, size_t nGrains)
:	genGranPoly1(sampleRate, wavespan, nGrains)
{}

void TsaraGranular::doSetPosition(double positionNormalized) {
	this->genGranPoly1::doSetPosition(positionNormalized);
}
void TsaraGranular::doSetPositionRandomness(double rand){
	this->genGranPoly1::doSetPositionRandomness(rand);
}

void TsaraGranular::doSetDuration(double dur){
	this->genGranPoly1::doSetDuration(dur);
}
void TsaraGranular::doSetDurationRandomness(double rand){
	this->genGranPoly1::doSetDurationRandomness(rand);
}

}	// namespace gran
}	// namespace nvs
