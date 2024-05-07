/*
  ==============================================================================

    JuceTsaraGranularSynthesizer.h
    Created: 7 May 2024 11:55:11am
    Author:  Nicholas Solem

  ==============================================================================
*/

#pragma once
#include <JuceHeader.h>
#include "../../slicer_granular/Source/Synthesis/JuceGranularSynthSound.h"
#include "../../slicer_granular/Source/Synthesis/JuceGranularSynthVoice.h"
#include "TsaraGranularSynth.h"

class JuceTsaraGranularSynthesizer	:	public juce::Synthesiser
{
public:
	JuceTsaraGranularSynthesizer(const double &sampleRate, const std::span<float> &wavespan, const double &fileSampleRate, unsigned int num_voices)
	{
		clearVoices();
		unsigned long seed = 1234567890UL;
		for (int i = 0; i < num_voices; ++i) {

			auto voice = new GranularVoice(std::make_unique<nvs::gran::TsaraGranular>(sampleRate, wavespan, fileSampleRate, seed));
			addVoice(voice);
			++seed;
		}
		
		clearSounds();
		auto sound = new GranularSound;
		addSound(sound);
	}
	
	
	void loadOnsets(const std::span<float> onsetsInSeconds) {
		auto const numVoices = getNumVoices();
		for (int voiceIdx = 0; voiceIdx < numVoices; ++voiceIdx){
			juce::SynthesiserVoice *voice = getVoice(voiceIdx);
			if (GranularVoice* granularVoice = dynamic_cast<GranularVoice*>(voice)){
				auto *granularSynthGuts = granularVoice->getGranularSynthGuts();
				if (nvs::gran::TsaraGranular * tsaraGuts = dynamic_cast<nvs::gran::TsaraGranular*>(granularSynthGuts)){
					tsaraGuts->loadOnsets(onsetsInSeconds);
				}
			}
		}
	}
	
	
	
	template <auto Start, auto End>
	constexpr void granularMainParamSet(juce::AudioProcessorValueTreeState &apvts){

		if constexpr (Start < End){
			
			juce::SynthesiserVoice* voice = getVoice(Start);
			if (GranularVoice* granularVoice = dynamic_cast<GranularVoice*>(voice)){
				granularVoice->granularMainParamSet<0, static_cast<int>(params_e::count_main_granular_params)>(apvts);
			}
			else {
				jassert (false);
			}
			granularMainParamSet<Start + 1, End>(apvts);
		}
	}
	
	template <auto Start, auto End>
	constexpr void envelopeParamSet(juce::AudioProcessorValueTreeState &apvts){
		if constexpr (Start < End){
			juce::SynthesiserVoice* voice = getVoice(Start);
			if (GranularVoice* granularVoice = dynamic_cast<GranularVoice*>(voice)){
				granularVoice->envelopeParamSet<static_cast<int>(params_e::count_main_granular_params)+1,
												static_cast<int>(params_e::count_envelope_params)>
												(apvts);
			}
			else {
				jassert (false);
			}
			envelopeParamSet<Start + 1, End>(apvts);
		}
	}
private:
};
