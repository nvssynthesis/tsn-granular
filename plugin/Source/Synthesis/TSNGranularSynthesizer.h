/*
  ==============================================================================

    TSNGranularSynthesizer.h
    Created: 7 May 2024 11:55:11am
    Author:  Nicholas Solem

  ==============================================================================
*/

#pragma once
#include <span>
#include "../../slicer_granular/Source/Synthesis/GranularSynthesizer.h"
#include "../../slicer_granular/Source/misc_util.h"
#include "Navigation/Navigator.h"

namespace nvs::gran {
class TSNGranularSynthesizer final
:	public GranularSynthesizer
,   private juce::AudioProcessorValueTreeState::Listener
,   private juce::ActionListener
{
public:
    using WeightedIdx = nvs::util::WeightedIdx;
    using TimbreSpace = timbrespace::TimbreSpace;

    explicit TSNGranularSynthesizer(juce::AudioProcessorValueTreeState &apvts);
    ~TSNGranularSynthesizer() override;

    void loadOnsets(const std::span<float> onsets);

    TimbreSpace &getTimbreSpace() {
        return _timbreSpace;
    }
    void actionListenerCallback(juce::String const &message) override;
    void setCurrentPlaybackSampleRate(double newSampleRate) override;
    void processBlock(juce::AudioBuffer<float> &buffer, juce::MidiBuffer &midi) override;
private:
    //==============================================================================
    using Navigator = timbrespace::Navigator<nvs::timbrespace::Timbre5DPoint>;
    Navigator _navigator;
    TimbreSpace _timbreSpace;

    //==============================================================================
    void parameterChanged(const String &parameterID, float newValue) override;  // AudioProcessorValueTreeState::Listener
    //==============================================================================

    void setReadBoundsFromChosenPoint();
};
}   // nvs::gran