//
// Created by Nicholas Solem on 11/2/25.
//

#pragma once
#include "../slicer_granular/Source/Gui/WaveformComponent.h"
#include "TsnGranularPluginProcessor.h"

class SegmentedWaveformComponent final
:   public WaveformComponent
,   public juce::ActionListener
{
public:
    explicit SegmentedWaveformComponent(TSNGranularAudioProcessor &proc, int sourceSamplesPerThumbnailSample=512);
    ~SegmentedWaveformComponent() override;

    void paintOnsetMarkers();

    //========================================================================
    void changeListenerCallback (juce::ChangeBroadcaster* source) override;
    //========================================================================
    void actionListenerCallback(const String &message) override;
    void setThumbnailSource(const juce::AudioBuffer<float> *newSource, double sampleRate, juce::int64 hashCode) override;

private:
    TSNGranularAudioProcessor &_tsn_proc;
    std::shared_ptr<nvs::analysis::OnsetAnalysisResult> _onsetAnalysis;

    void retrieveOnsets();
};


