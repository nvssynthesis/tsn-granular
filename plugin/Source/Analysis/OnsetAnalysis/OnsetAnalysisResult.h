//
// Created by Nicholas Solem on 1/16/26.
//

#pragma once
#include <JuceHeader.h>

namespace nvs::analysis {

struct OnsetAnalysisResult {
    OnsetAnalysisResult(std::vector<float> onsets_, juce::String hash_, juce::String path_)
    :   onsets(std::move(onsets_)), waveformHash(std::move(hash_)), audioFileAbsPath(std::move(path_)) {}

    std::vector<float> onsets;

    String waveformHash {};
    String audioFileAbsPath {};
};

} // namespace nvs::analysis