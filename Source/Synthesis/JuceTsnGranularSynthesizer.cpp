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


// TODO:
// make generic function that can apply incoming arguments to generic function of tsnGuts of all voices, regardless of number of args


JuceTsnGranularSynthesizer::JuceTsnGranularSynthesizer()
{
	clearVoices();
	{
		_synth_shared_state._settings._center_position_at_env_peak = false;
		_synth_shared_state._settings._duration_pitch_compensation = 0.f;
	}
	unsigned long seed = 1234567890UL;
	for (int i = 0; i < num_voices; ++i) {
		auto voice = new GranularVoice(std::make_unique<nvs::gran::TsnGranular>(&_synth_shared_state, i, seed));
		addVoice(voice);
		++seed;
	}
	clearSounds();
	auto sound = new GranularSound;
	addSound(sound);
}

void JuceTsnGranularSynthesizer::loadOnsets(const std::span<float> onsets) {
	auto const numVoices = getNumVoices();
	for (int voiceIdx = 0; voiceIdx < numVoices; ++voiceIdx){
		if (GranularVoice* granularVoice = dynamic_cast<GranularVoice*>(getVoice(voiceIdx))){
			
			if (nvs::gran::TsnGranular* tsnGuts = dynamic_cast<nvs::gran::TsnGranular*>( granularVoice->getGranularSynthGuts() )){
				tsnGuts->loadOnsets(onsets);
			}
		}
	}
}

void JuceTsnGranularSynthesizer::setWaveEvent(size_t index) {
	auto const numVoices = getNumVoices();
	for (int voiceIdx = 0; voiceIdx < numVoices; ++voiceIdx){
		if (GranularVoice* granularVoice = dynamic_cast<GranularVoice*>(getVoice(voiceIdx))){
			
			if (nvs::gran::TsnGranular* tsnGuts = dynamic_cast<nvs::gran::TsnGranular*>( granularVoice->getGranularSynthGuts() )){
				tsnGuts->setWaveEvent(index);
			}
		}
	}
	currentIdx = index;
	sendChangeMessage();	// tell WaveformComponent to highlight event
}

void JuceTsnGranularSynthesizer::setWaveEvents(std::array<size_t, 4> indices, std::array<float, 4> weights)
{
	auto const numVoices = getNumVoices();
	for (int voiceIdx = 0; voiceIdx < numVoices; ++voiceIdx){
		if (GranularVoice* granularVoice = dynamic_cast<GranularVoice*>(getVoice(voiceIdx))){
			
			if (nvs::gran::TsnGranular* tsnGuts = dynamic_cast<nvs::gran::TsnGranular*>( granularVoice->getGranularSynthGuts() )){
				tsnGuts->setWaveEvents(indices, weights);
			}
		}
	}
}
