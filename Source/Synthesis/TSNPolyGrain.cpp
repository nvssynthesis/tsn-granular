/*
  ==============================================================================

	TSNPolyGrain.cpp
    Created: 4 Sep 2023 1:54:00am
    Author:  Nicholas Solem

  ==============================================================================
*/

#include "TSNPolyGrain.h"
#include "fmt/core.h"
#include "../slicer_granular/Source/algo_util.h"
#include "../slicer_granular/Source/misc_util.h"

namespace nvs	{
namespace gran	{

TSNPolyGrain::TSNPolyGrain(GranularSynthSharedState *const synth_shared_state, GranularVoiceSharedState *const voice_shared_state)
:	PolyGrain(synth_shared_state, voice_shared_state)
{}
//====================================================================================

void TSNPolyGrain::loadOnsets(std::span<float> const normalizedOnsets){
	_onsetsNormalized.assign(normalizedOnsets.begin(), normalizedOnsets.end());
}

void TSNPolyGrain::setWaveEvent(size_t index) {
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
void TSNPolyGrain::setWaveEvents(std::vector<WeightedIdx> weightedIndices) {
	if (!_onsetsNormalized.size()){
		return;
	}
	
	std::vector<WeightedReadBounds> wrbs;
	wrbs.reserve(weightedIndices.size());
	for (auto wi : weightedIndices){
		auto index = wi.idx;
		if (index >= (int)_onsetsNormalized.size()){
			return; // invalid
		}
		auto const nextIdx = (index + 1) % _onsetsNormalized.size();
		
		wrbs.emplace_back(
			 ReadBounds {
				.begin = _onsetsNormalized[index],
				.end = _onsetsNormalized[nextIdx]
			 }, wi.weight);
	}
	setMultiReadBounds(wrbs);
}

//====================================================================================


}	// namespace gran
}	// namespace nvs
