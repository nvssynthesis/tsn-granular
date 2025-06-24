/*
  ==============================================================================

    JuceTsnGranularSynthesizer.h
    Created: 7 May 2024 11:55:11am
    Author:  Nicholas Solem

  ==============================================================================
*/

#pragma once
#include <span>
#include "../../slicer_granular/Source/Synthesis/GranularSynthesizer.h"
#include "../../slicer_granular/Source/misc_util.h"

class TSNGranularSynthesizer
:	public GranularSynthesizer
,	public juce::ChangeBroadcaster
{
public:
	using WeightedIdx = nvs::util::WeightedIdx;
	
	TSNGranularSynthesizer(juce::AudioProcessorValueTreeState &apvts);
	bool readyForProcess() const {
		return false;
	}
	void loadOnsets(const std::span<float> onsets);
	void setWaveEvents(std::vector<WeightedIdx> points);
	std::vector<WeightedIdx> getCurrentIndices() const {
		return currentIndices;
	}
private:
	std::vector<WeightedIdx> currentIndices; // easy way out for the time being (to solve issue of carrying this data to WaveformComponent, which is a listener of this)
};

