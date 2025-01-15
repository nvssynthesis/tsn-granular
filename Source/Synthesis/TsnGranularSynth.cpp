/*
  ==============================================================================

    TsnGranularSynth.cpp
    Created: 4 Sep 2023 1:54:00am
    Author:  Nicholas Solem

  ==============================================================================
*/

#include "TsnGranularSynth.h"
#include "../slicer_granular/Source/algo_util.h"
#include "fmt/core.h"

namespace nvs	{
namespace gran	{

TsnGranular::TsnGranular(double const &sampleRate, std::span<float> const &wavespan,
														double const &fileSampleRate, unsigned long seed)
:	genGranPoly1(sampleRate, wavespan, fileSampleRate, seed)
{}
//====================================================================================
void TsnGranular::loadOnsets(std::span<float> const onsetsInSeconds){
	assert( std::is_sorted(onsetsInSeconds.begin(), onsetsInSeconds.end()) );
	float const lengthInSeconds = static_cast<float>(_wavespan.size()) / _fileSampleRate;
	// starting at end, count onsets exceeding lengthInSeconds
	size_t properOnsets = onsetsInSeconds.size();
	for (auto it = onsetsInSeconds.rbegin(); it != onsetsInSeconds.rend(); ++it){
		if (*it > lengthInSeconds){
			--properOnsets;
		}
		else {	// since the vector is sorted, we know that there are no more exceeding the proper length
			break;
		}
	}

	_onsetsNormalized.resize(properOnsets);
	for (auto f : _onsetsNormalized){
		fmt::print("{}\n", f);
	}
	std::transform(onsetsInSeconds.begin(), onsetsInSeconds.begin() + properOnsets,
				   _onsetsNormalized.begin(), [=](float f)
				{
					float res = f / lengthInSeconds;
					assert(res <= 1.f);
					return res;
				});
	for (auto f : _onsetsNormalized){
		fmt::print("{}\n", f);
	}
}
//====================================================================================
void TsnGranular::doSetPosition(double positionNormalized) {
	if (_onsetsNormalized.size()){
		auto res = nvs::util::get_left(static_cast<float>(positionNormalized), _onsetsNormalized);
		if (res){
			positionNormalized = static_cast<double>(res.value().second);
		}
	}
	this->genGranPoly1::doSetPosition(positionNormalized);
}
void TsnGranular::doSetPositionRandomness(double rand){
	this->genGranPoly1::doSetPositionRandomness(rand);
}

void TsnGranular::doSetDuration(double dur){
	this->genGranPoly1::doSetDuration(dur);
}
void TsnGranular::doSetDurationRandomness(double rand){
	this->genGranPoly1::doSetDurationRandomness(rand);
}

}	// namespace gran
}	// namespace nvs
