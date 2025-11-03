//
// Created by Nicholas Solem on 11/2/25.
//

#pragma once
#include "../slicer_granular/Source/Gui/WaveformComponent.h"
#include "TsnGranularPluginProcessor.h"

class SegmentedWaveformComponent final
:   public WaveformComponent
{
public:
    explicit SegmentedWaveformComponent(TSNGranularAudioProcessor &proc, int sourceSamplesPerThumbnailSample=512);
    ~SegmentedWaveformComponent() override;

    void paintOnsetMarkers();

    //========================================================================
    void changeListenerCallback (juce::ChangeBroadcaster* source) override;

private:
    TSNGranularAudioProcessor &_tsn_proc;
    std::shared_ptr<nvs::analysis::ThreadedAnalyzer::OnsetAnalysisResult> _onsetAnalysis;
};


