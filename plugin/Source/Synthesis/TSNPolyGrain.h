/*
  ==============================================================================

	TSNPolyGrain.h
    Created: 4 Sep 2023 1:54:00am
    Author:  Nicholas Solem

  ==============================================================================
*/

#pragma once
#include "ThreadedAnalyzer.h"
#include "OnsetAnalysis/OnsetAnalysisResult.h"
#include "../slicer_granular/Source/Synthesis/GranularSynthesis.h"
#include "../slicer_granular/Source/IndexTypes.h"

/** TODO:
	-bake in some way to be sure that the currently held onsets match the current span
*/

namespace nvs::gran {
class TSNPolyGrain final :		public PolyGrain
{
public:
    using WeightedIdx = timbrespace::WeightedIdx;
    using SharedOnsets = std::shared_ptr<analysis::OnsetAnalysisResult>;
	
    TSNPolyGrain(GranularSynthSharedState *synth_shared_state, GranularVoiceSharedState *voice_shared_state);
    ~TSNPolyGrain() override = default;
    //================================================================================================================================================
    void loadOnsets(SharedOnsets onsets);
    void setWaveEvent(size_t index);
    void setWaveEvents(const std::vector<WeightedIdx> &weightedIndices);
    //================================================================================================================================================
private:
    SharedOnsets _onsets;

    std::vector<WeightedReadBounds> _weightedReadBounds;    // member to avoid reallocations
};

}   // namespace nvs::gran
