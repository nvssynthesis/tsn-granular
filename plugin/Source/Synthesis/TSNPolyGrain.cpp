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

namespace nvs::gran {

TSNPolyGrain::TSNPolyGrain(GranularSynthSharedState *const synth_shared_state, GranularVoiceSharedState *const voice_shared_state)
:   PolyGrain(synth_shared_state, voice_shared_state)
, _fundamentalsTrio()
{
    _weightedReadBoundsTrio.reserve(3); // this will be storing 3 weights
}

//====================================================================================
void TSNPolyGrain::setNeededData(SharedOnsets onsets, const std::vector<float> &fundamentals) { // NOLINT: intentional copy for shared ownership
    if (onsets == nullptr || onsets->onsets.empty()) {
        return;
    }
    jassert (onsets->onsets.size() == fundamentals.size());
    _onsets = onsets;
    _fundamentals = fundamentals;
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
    if (_onsets == nullptr || _onsets->onsets.empty() || _fundamentals.empty()){
        DBG("TSNPolyGrain: onsets not ready, returning\n");
        return;
    }

    jassert(weightedIndices.size() <= 3);
    jassert(_weightedReadBoundsTrio.capacity() >= weightedIndices.size());
    jassert(_fundamentals.size() == _onsets->onsets.size());

    _weightedReadBoundsTrio.clear();
    for (size_t i = 0; i < 3; ++i){
        const auto &wi = weightedIndices[i];
        const auto index = wi.idx;
        if (index >= static_cast<int>(_onsets->onsets.size())){
            return; // invalid
        }
        auto const nextIdx = (index + 1) % _onsets->onsets.size();

        _weightedReadBoundsTrio.emplace_back(
                ReadBounds {
                        .begin = _onsets->onsets[index],
                        .end = _onsets->onsets[nextIdx]
                }, wi.weight);
        _fundamentalsTrio[i] = _fundamentals[index];
    }
    setEvents(_weightedReadBoundsTrio, _fundamentalsTrio);
}

}
