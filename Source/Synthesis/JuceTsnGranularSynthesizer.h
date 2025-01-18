/*
  ==============================================================================

    JuceTsnGranularSynthesizer.h
    Created: 7 May 2024 11:55:11am
    Author:  Nicholas Solem

  ==============================================================================
*/

#pragma once
//#include <JuceHeader.h>
//#include "../../slicer_granular/Source/Synthesis/JuceGranularSynthSound.h"
//#include "../../slicer_granular/Source/Synthesis/JuceGranularSynthVoice.h"
//#include "TsnGranularSynth.h"
#include "../../slicer_granular/Source/Synthesis/JuceGranularSynthesizer.h"
#include <span>

class JuceTsnGranularSynthesizer	:	public GranularSynthesizer
{
public:
	JuceTsnGranularSynthesizer();
	void loadOnsets(const std::span<float> onsetsInSeconds);
};
