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
#include "TimbreSpace/TimbreSpacePointSelector.h"

namespace nvs::gran {
class TSNGranularSynthesizer final
:	public GranularSynthesizer
,   private ActionListener
{
public:
    using WeightedIdx = nvs::util::WeightedIdx;
    using TimbreSpace = timbrespace::TimbreSpace;
    using TimbreSpacePointSelector = timbrespace::TimbreSpacePointSelector;
    using SharedOnsets = std::shared_ptr<nvs::analysis::ThreadedAnalyzer::OnsetAnalysisResult>;

    explicit TSNGranularSynthesizer(juce::AudioProcessorValueTreeState &apvts);
    ~TSNGranularSynthesizer() override;

    void loadOnsets(SharedOnsets onsets);

    [[deprecated("theory: the only valid reasons to get timbreSpace from here would be saving, writing, and validation. create helper methods instead.")]]
    TimbreSpace &getTimbreSpace() {
        return _timbreSpace;
    }
    TimbreSpacePointSelector &getTimbreSpacePointSelector() {
        return _timbreSpacePointSelector;
    }
    void setCurrentPlaybackSampleRate(double newSampleRate) override;
    void processBlock(juce::AudioBuffer<float> &buffer, juce::MidiBuffer &midi) override;
private:
    //==============================================================================
    using NavigationManager = timbrespace::NavigationManager<timbrespace::Timbre5DPoint>;
    NavigationManager _navigator;
    TimbreSpace _timbreSpace;
    TimbreSpacePointSelector _timbreSpacePointSelector;

    //==============================================================================
    void actionListenerCallback(const String &message) override;
    //==============================================================================
    void setReadBoundsFromChosenPoint();
};
}   // nvs::gran