/*
  ==============================================================================

	TSNPolyGrain.h
    Created: 4 Sep 2023 1:54:00am
    Author:  Nicholas Solem

  ==============================================================================
*/

#pragma once
#include "Analysis/EssentiaSetup.h"
#include "../slicer_granular/Source/Synthesis/GranularSynthesis.h"
#include "Analysis/OnsetAnalysis/OnsetAnalysis.h"
#include "Analysis/Analyzer.h"
#include <JuceHeader.h>


/** TODO:
	-bake in some way to be sure that the currently held onsets match the current span
*/
struct EventInfo {
	double start_pos;
	double end_pos;
};

namespace nvs	{
namespace gran	{
class TSNPolyGrain		:		public PolyGrain
{
public:
	using WeightedIdx = nvs::util::WeightedIdx;
	
	TSNPolyGrain(GranularSynthSharedState *const synth_shared_state, GranularVoiceSharedState *const voice_shared_state);
	virtual ~TSNPolyGrain() = default;
	void loadOnsets(std::span<float> const normalizedOnsets);
	
	//================================================================================================================================================
	void setWaveEvent(size_t index);
	void setWaveEvents(std::vector<WeightedIdx> weightedIndices);
	//================================================================================================================================================

	
	bool readyForProcess() const {
		return _onsetsNormalized.size() > 0;
	}
private:
	std::vector<double> _onsetsNormalized;
	EventInfo _currentEventInfo;
};

}	// namespace gran
}	// namespace nvs

