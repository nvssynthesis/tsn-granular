//
// Created by Nicholas Solem on 11/2/25.
//

#include "SegmentedWaveformComponent.h"

#include "TsnGranularPluginProcessor.h"

SegmentedWaveformComponent::SegmentedWaveformComponent(TSNGranularAudioProcessor &proc, const int sourceSamplesPerThumbnailSample)
:	WaveformComponent(proc, sourceSamplesPerThumbnailSample)
,   _tsn_proc(proc)
{
    paintOnsetMarkers();
    _tsn_proc.getAnalyzer().addChangeListener(this);    // paint my own onset markers immediately when made available by ThreadedAnalyzer
}
SegmentedWaveformComponent::~SegmentedWaveformComponent() {
    _tsn_proc.getAnalyzer().removeChangeListener(this);
}

void SegmentedWaveformComponent::paintOnsetMarkers() {
    _onsetAnalysis = _tsn_proc.getAnalyzer().shareOnsetAnalysis();
    if (_onsetAnalysis == nullptr) {
        DBG("WaveformComponent: OnsetAnalysis is null, returning...\n");
        return;
    }

    DBG("WaveformComponent: drawing onsets...\n");
    removeMarkers(WaveformComponent::MarkerType::Onset);
    for (const auto onset : _onsetAnalysis->onsets) {
        addMarker(onset);
    }
    repaint();
}


void SegmentedWaveformComponent::changeListenerCallback (juce::ChangeBroadcaster* source) {
    if (dynamic_cast<nvs::analysis::ThreadedAnalyzer*>(source)) {
        paintOnsetMarkers();
    }
    WaveformComponent::changeListenerCallback(source);
}