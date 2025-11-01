/*
  ==============================================================================

    ThreadedAnalyzer.h
    Created: 1 Nov 2023 3:36:32pm
    Author:  Nicholas Solem

  ==============================================================================
*/

#pragma once
#include "Analysis/Analyzer.h"
#include <JuceHeader.h>

namespace nvs::analysis {

class ThreadedAnalyzer final :	public Thread
,							    public ChangeBroadcaster
{
public:
    //===============================================================================
    struct OnsetAnalysisResult {
        vecReal onsets;

        String audioHash {};
        String audioFileAbsPath {};
    };
    struct TimbreAnalysisResult {
        std::vector<FeatureContainer<EventwiseStatistics<Real>>> timbreMeasurements;

        String audioHash {};
        String audioFileAbsPath {};
    };
    //===============================================================================
    ThreadedAnalyzer();
    ~ThreadedAnalyzer() override;
    //===============================================================================
    void updateStoredAudio(std::span<float const> wave, const juce::String &audioFileAbsPath);
    void updateSettings(const juce::ValueTree &settingsTree);
    //===============================================================================
    void run() override;
    void stopAnalysis() { signalThreadShouldExit(); }
    //===============================================================================
    bool onsetsReady() const {
        return _onsetAnalysisResult != nullptr;
    }
    bool timbreAnalysisReady() const {
        return _timbreAnalysisResult.has_value();
    }
    //===============================================================================
    std::shared_ptr<OnsetAnalysisResult> shareOnsetAnalysis();
    std::optional<TimbreAnalysisResult> stealTimbreSpaceRepresentation();
    //===============================================================================
    Analyzer &getAnalyzer() { return _analyzer; }
    RunLoopStatus &getStatus() noexcept { return _rls; }
    String getSettingsHash() const noexcept { return _analyzer.getSettingsHash(); }
    //===============================================================================
private:
    Analyzer _analyzer;
    vecReal _inputWave;
    std::shared_ptr<OnsetAnalysisResult> _onsetAnalysisResult;
    std::optional<TimbreAnalysisResult> _timbreAnalysisResult;

    String _audioFileAbsPath {};

    RunLoopStatus _rls;
};

}
