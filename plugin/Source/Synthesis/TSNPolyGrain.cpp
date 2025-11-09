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

void TSNPolyGrain::loadOnsets(const SharedOnsets onsets){ // NOLINT: intentional copy for shared ownership
	_onsets = onsets;
}

void TSNPolyGrain::setWaveEvent(const size_t index) {
	if (_onsets == nullptr || _onsets->onsets.empty()){
	    DBG("TSNPolyGrain: onsets not ready, returning\n");
		return;
	}
	auto const nextIdx = (index + 1) % _onsets->onsets.size();
	ReadBounds const bounds
	{
		.begin = _onsets->onsets[index],
		.end =  _onsets->onsets[nextIdx]
	};
	setReadBounds(bounds);
}
void TSNPolyGrain::setWaveEvents(const std::vector<WeightedIdx> &weightedIndices) {
    if (_onsets == nullptr || _onsets->onsets.empty()){
        DBG("TSNPolyGrain: onsets not ready, returning\n");
        return;
    }
	
	std::vector<WeightedReadBounds> wrbs;
	wrbs.reserve(weightedIndices.size());
	for (auto wi : weightedIndices){
		const auto index = wi.idx;
		if (index >= static_cast<int>(_onsets->onsets.size())){
			return; // invalid
		}
		auto const nextIdx = (index + 1) % _onsets->onsets.size();
		
		wrbs.emplace_back(
			 ReadBounds {
				.begin = _onsets->onsets[index],
				.end = _onsets->onsets[nextIdx]
			 }, wi.weight);
	}
	setMultiReadBounds(wrbs);
}

}	// namespace gran
}	// namespace nvs
