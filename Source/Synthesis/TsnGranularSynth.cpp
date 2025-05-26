/*
  ==============================================================================

    TsnGranularSynth.cpp
    Created: 4 Sep 2023 1:54:00am
    Author:  Nicholas Solem

  ==============================================================================
*/

#include "TsnGranularSynth.h"
#include "fmt/core.h"
#include "../slicer_granular/Source/algo_util.h"
#include "../slicer_granular/Source/misc_util.h"

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
	assert(index < _onsetsNormalized.size());
	auto const nextIdx = (index + 1) % _onsetsNormalized.size();
	ReadBounds const bounds
	{
		.begin = _onsetsNormalized[index],
		.end =  _onsetsNormalized[nextIdx]
	};
	setReadBounds(bounds);
}
void TsnGranular::setWaveEvents(WeightedIndices weightedIndices) {
	if (!_onsetsNormalized.size()){
		return;
	}
	
	std::vector<WeightedReadBounds> wrbs;
	wrbs.reserve(weightedIndices.size());
	for (auto wi : weightedIndices){
		auto index = wi.idx;
		assert(index < (int)_onsetsNormalized.size());
		auto const nextIdx = (index + 1) % _onsetsNormalized.size();
		
		wrbs.push_back(WeightedReadBounds{
			.bounds {
				.begin = _onsetsNormalized[index],
				.end = _onsetsNormalized[nextIdx]
			},
			.weight = wi.weight
		});
	}
	setMultiReadBounds(wrbs);
}

//====================================================================================


}	// namespace gran
}	// namespace nvs
