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

TsnGranular::TsnGranular(GranularSynthSharedState *const synth_shared_state, int voice_id, unsigned long seed)
:	genGranPoly1(synth_shared_state, voice_id, seed)
{}
//====================================================================================

void TsnGranular::loadOnsets(std::span<float> const onsetsInSeconds){
	assert( std::is_sorted(onsetsInSeconds.begin(), onsetsInSeconds.end()) );
	double const lengthInSeconds = static_cast<double>(_synth_shared_state->_buffer._wave_block.getNumSamples()) / _synth_shared_state->_buffer._file_sample_rate;
	// starting at end, count onsets exceeding lengthInSeconds
	size_t numProperOnsets = onsetsInSeconds.size();
	for (auto it = onsetsInSeconds.rbegin(); it != onsetsInSeconds.rend(); ++it){
		if (*it > lengthInSeconds){
			--numProperOnsets;
		}
		else {	// since the vector is sorted, we know that there are no more exceeding the proper length
			break;
		}
	}
	_onsetsNormalized.resize(numProperOnsets);
	for (auto f : _onsetsNormalized){
		fmt::print("{}\n", f);
	}
	std::transform(onsetsInSeconds.begin(), onsetsInSeconds.begin() + numProperOnsets,
				   _onsetsNormalized.begin(), [=](float f)
				{
					double res = f / lengthInSeconds;
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
		auto const res = nvs::util::get_left(positionNormalized, _onsetsNormalized);
		if (res){
			_currentEventInfo.start_pos = static_cast<double>(res.value().second);
			size_t const nextIdx = res.value().first + 1;
			_currentEventInfo.end_pos = nextIdx < _onsetsNormalized.size() ? _onsetsNormalized[nextIdx] : 1.0;
		}
	}
#pragma message("here needs to set position within bounds of 'event'")
	this->genGranPoly1::doSetPosition(_currentEventInfo.start_pos);
}
void TsnGranular::doSetPositionRandomness(double rand){
	this->genGranPoly1::doSetPositionRandomness(rand);
}


void TsnGranular::doSetDuration(double dur){
	// make duration based on current event
	this->genGranPoly1::doSetDuration(dur);
}
void TsnGranular::doSetDurationRandomness(double rand){
	this->genGranPoly1::doSetDurationRandomness(rand);
}

}	// namespace gran
}	// namespace nvs
