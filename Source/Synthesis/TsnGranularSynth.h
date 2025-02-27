/*
  ==============================================================================

    TsnGranularSynth.h
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
class TsnGranular		:		public genGranPoly1
{
public: 
	TsnGranular(GranularSynthSharedState *const synth_shared_state, int voice_id, unsigned long seed = 1234567890UL);
	virtual ~TsnGranular() = default;
	void loadOnsets(std::span<float> const normalizedOnsets);
	
	//================================================================================================================================================
	void setWaveEvent(size_t index);
	void setWaveEvents(std::array<size_t, 4> indices,
					   std::array<float, 4> weights);
	//================================================================================================================================================

	
	bool readyForProcess() const {
		return _onsetsNormalized.size() > 0;
	}
private:
	std::vector<double> _onsetsNormalized;
	EventInfo _currentEventInfo;
	
	void doSetPosition(double position) override;
	void doSetPositionRandomness(double rand) override;
	void doSetDuration(double dur) override;
	void doSetDurationRandomness(double rand) override;
};

}	// namespace gran
}	// namespace nvs

