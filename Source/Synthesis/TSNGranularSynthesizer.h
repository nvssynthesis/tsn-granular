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
	using WeightedIndex = nvs::util::TimbreSpaceHolder::WeightedIdx;
	using WeightedIndices = nvs::util::TimbreSpaceHolder::WeightedPoints;
	
	TSNGranularSynthesizer(juce::AudioProcessorValueTreeState &apvts);
	bool readyForProcess() const {
		return false;
	}
	void loadOnsets(const std::span<float> onsets);
	void setWaveEvents(WeightedIndices points);
	WeightedIndices getCurrentIndices() const {
		return currentIndices;
	}
private:
	WeightedIndices currentIndices; // easy way out for the time being (to solve issue of carrying this data to WaveformComponent, which is a listener of this)
};

