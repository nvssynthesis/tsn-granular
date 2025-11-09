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
#include "Navigation/NavigationManager.h"

namespace nvs::gran {
class TSNGranularSynthesizer final
:	public GranularSynthesizer
,   private ActionListener
{
public:
    using WeightedIdx = nvs::util::WeightedIdx;
    using TimbreSpace = timbrespace::TimbreSpace;
    using SharedOnsets = std::shared_ptr<nvs::analysis::ThreadedAnalyzer::OnsetAnalysisResult>;

    explicit TSNGranularSynthesizer(juce::AudioProcessorValueTreeState &apvts);
    ~TSNGranularSynthesizer() override;

    void loadOnsets(SharedOnsets onsets);

    TimbreSpace &getTimbreSpace() {
        return _timbreSpace;
    }
    void setCurrentPlaybackSampleRate(double newSampleRate) override;
    void processBlock(juce::AudioBuffer<float> &buffer, juce::MidiBuffer &midi) override;
private:
    //==============================================================================
    using NavigationManager = timbrespace::NavigationManager<timbrespace::Timbre5DPoint>;
    NavigationManager _navigator;
    TimbreSpace _timbreSpace;

    //==============================================================================
    void actionListenerCallback(const String &message) override;
    //==============================================================================
    void setReadBoundsFromChosenPoint();
};
}   // nvs::gran