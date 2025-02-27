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

void TsnGranular::loadOnsets(std::span<float> const normalizedOnsets){
	_onsetsNormalized.assign(normalizedOnsets.begin(), normalizedOnsets.end());
}

void TsnGranular::setWaveEvent(size_t index) {
	if (!_onsetsNormalized.size()){
		return;
	}
	auto const nextIdx = index + 1;
	ReadBounds const bounds
	{
		.begin = _onsetsNormalized[index],
		.end = nextIdx < _onsetsNormalized.size() ? _onsetsNormalized[nextIdx] : 1.0
	};
	setReadBounds(bounds);
}
void TsnGranular::setWaveEvents(std::array<size_t, 4> indices, std::array<float, 4> weights) {
	assert (false);
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
