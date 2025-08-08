/*
  ==============================================================================

    JuceTsaraGranularSynthesizer.cpp
    Created: 7 May 2024 11:55:11am
    Author:  Nicholas Solem

  ==============================================================================
*/

#include "TSNGranularSynthesizer.h"
#include "../Synthesis/TSNPolyGrain.h"
#include "../../slicer_granular/Source/Synthesis/GranularSound.h"


// TODO:
// make generic function that can apply incoming arguments to generic function of tsnGuts of all voices, regardless of number of args



// definition for TSN specialization needed here because GranularVoice.h does not need to know about TSNPolyGrain
template<>
void GranularVoice::initSynthGuts<nvs::gran::TSNPolyGrain>() {
	granularSynthGuts = std::make_unique<nvs::gran::TSNPolyGrain>(_synth_shared_state, &_voice_shared_state);
}

TSNGranularSynthesizer::TSNGranularSynthesizer(juce::AudioProcessorValueTreeState &apvts)
	:	GranularSynthesizer(apvts)
{
	clearVoices();
	{
		_synth_shared_state._settings._center_position_at_env_peak = false;
		_synth_shared_state._settings._duration_pitch_compensation = 0.f;
	}
	unsigned long seed = 1234567890UL;
	for (int i = 0; i < num_voices; ++i) {
		auto voice = GranularVoice::create<nvs::gran::TSNPolyGrain>(&_synth_shared_state, seed, i);
		addVoice(voice);
		++seed;
	}
	clearSounds();
	addSound(new GranularSound);
}

void TSNGranularSynthesizer::loadOnsets(const std::span<float> onsets) {
	auto const numVoices = getNumVoices();
	for (int voiceIdx = 0; voiceIdx < numVoices; ++voiceIdx){
		if (GranularVoice* granularVoice = dynamic_cast<GranularVoice*>(getVoice(voiceIdx))){
			
			if (nvs::gran::TSNPolyGrain* tsnGuts = dynamic_cast<nvs::gran::TSNPolyGrain*>( granularVoice->getGranularSynthGuts() )){
				tsnGuts->loadOnsets(onsets);
			}
		}
	}
}
void TSNGranularSynthesizer::setWaveEvents(std::vector<WeightedIdx> indices)
/*
 needs to happen AFTER proper onsets are loaded; otherwise the indices could be out of bounds
 However, since setWaveEvents happens based on a separate timer, the processing currently just exits early if the weighted indices exceed the numOnsets
*/
{
	auto const numVoices = getNumVoices();
	for (int voiceIdx = 0; voiceIdx < numVoices; ++voiceIdx){
		if (GranularVoice* granularVoice = dynamic_cast<GranularVoice*>(getVoice(voiceIdx))){
			
			if (nvs::gran::TSNPolyGrain* tsnGuts = dynamic_cast<nvs::gran::TSNPolyGrain*>( granularVoice->getGranularSynthGuts() )){
				tsnGuts->setWaveEvents(indices);
			}
		}
	}
	currentIndices = indices;
	sendChangeMessage();
}
