//
// Created by Nicholas Solem on 1/16/26.
//

#pragma once

namespace nvs::analysis {

struct TimbreAnalysisResult {
    TimbreAnalysisResult(std::vector<FeatureContainer<EventwiseStatistics<Real>>> timbreMeasurements_, juce::String hash_, juce::String path_)
    :   timbreMeasurements(std::move(timbreMeasurements_)), waveformHash(std::move(hash_)), audioFileAbsPath(std::move(path_)) {}

    std::vector<FeatureContainer<EventwiseStatistics<Real>>> timbreMeasurements;

    String waveformHash {};
    String audioFileAbsPath {};
};

} // namespace nvs::analysis
