/*
  ==============================================================================

    JuceTsnGranularSynthesizer.h
    Created: 7 May 2024 11:55:11am
    Author:  Nicholas Solem

  ==============================================================================
*/

#pragma once
#include "../../slicer_granular/Source/Synthesis/JuceGranularSynthesizer.h"
#include <span>

class JuceTsnGranularSynthesizer	:	public GranularSynthesizer
{
public:
	JuceTsnGranularSynthesizer();
	bool readyForProcess() const {
		return false;
	}
	void loadOnsets(const std::span<float> onsetsInSeconds);
};

