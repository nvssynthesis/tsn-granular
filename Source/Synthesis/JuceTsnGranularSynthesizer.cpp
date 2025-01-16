/*
  ==============================================================================

    JuceTsaraGranularSynthesizer.cpp
    Created: 7 May 2024 11:55:11am
    Author:  Nicholas Solem

  ==============================================================================
*/

#include "JuceTsnGranularSynthesizer.h"
#include "../Synthesis/TsnGranularSynth.h"
#include "../../slicer_granular/Source/Synthesis/JuceGranularSynthSound.h"

JuceTsnGranularSynthesizer::JuceTsnGranularSynthesizer(unsigned int num_voices)
{
	clearVoices();
	unsigned long seed = 1234567890UL;
	for (int i = 0; i < num_voices; ++i) {
		auto voice = new GranularVoice(std::make_unique<nvs::gran::TsnGranular>(seed));
		addVoice(voice);
		++seed;
	}
	clearSounds();
	auto sound = new GranularSound;
	addSound(sound);
}

void JuceTsnGranularSynthesizer::loadOnsets(const std::span<float> onsetsInSeconds) {
	auto const numVoices = getNumVoices();
	for (int voiceIdx = 0; voiceIdx < numVoices; ++voiceIdx){
		juce::SynthesiserVoice *voice = getVoice(voiceIdx);
		if (GranularVoice* granularVoice = dynamic_cast<GranularVoice*>(voice)){
			auto *granularSynthGuts = granularVoice->getGranularSynthGuts();
			if (nvs::gran::TsnGranular * tsnGuts = dynamic_cast<nvs::gran::TsnGranular*>(granularSynthGuts)){
				tsnGuts->loadOnsets(onsetsInSeconds);
			}
		}
	}
}
