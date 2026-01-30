//
// Created by Nicholas Solem on 11/2/25.
//

#include "SegmentedWaveformComponent.h"
#include "TsnGranularPluginProcessor.h"
#include "StringAxiom.h"

SegmentedWaveformComponent::SegmentedWaveformComponent(TSNGranularAudioProcessor &proc, const int sourceSamplesPerThumbnailSample)
:	WaveformComponent(proc, sourceSamplesPerThumbnailSample)
,   _tsn_proc(proc)
{
    retrieveOnsets();
    paintOnsetMarkers();
    _tsn_proc.getAnalyzer().addChangeListener(this);    // paint my own onset markers immediately when made available by ThreadedAnalyzer
}
SegmentedWaveformComponent::~SegmentedWaveformComponent() {
    _tsn_proc.getAnalyzer().removeChangeListener(this);
}

void SegmentedWaveformComponent::setThumbnailSource(const juce::AudioBuffer<float> *newSource, double sampleRate, juce::int64 hashCode) {
    WaveformComponent::setThumbnailSource(newSource, sampleRate, hashCode);
    retrieveOnsets();
}

void SegmentedWaveformComponent::retrieveOnsets() {
    const auto hash = WaveformComponent::getHashCode();

#pragma message("truly, we want to validate both the wave AND the onset settings match")
    if (const auto tsOnsets = _tsn_proc.getTimbreSpace().shareOnsets();
        tsOnsets != nullptr && tsOnsets->waveformHash.hash() == hash)
    {
        _onsetAnalysis = tsOnsets;
    } else if (const auto aOnsets = _tsn_proc.getAnalyzer().shareOnsetAnalysis();
        aOnsets != nullptr && aOnsets->waveformHash.hash() == hash)
    {
        _onsetAnalysis = aOnsets;
    } else {
        _onsetAnalysis = nullptr;
    }
}

void SegmentedWaveformComponent::paintOnsetMarkers() {
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
    if (auto *a = dynamic_cast<nvs::analysis::ThreadedAnalyzer*>(source)) {
        _onsetAnalysis = a->shareOnsetAnalysis();
        paintOnsetMarkers();
    }
    WaveformComponent::changeListenerCallback(source);
}
void SegmentedWaveformComponent::actionListenerCallback(const String &message) {
    if (message == nvs::axiom::tsn::onsetsAvailable) { // comes from TimbreSpace
        _onsetAnalysis = _tsn_proc.getTimbreSpace().shareOnsets();
        paintOnsetMarkers();
    }
}