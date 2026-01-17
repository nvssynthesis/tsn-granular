//
// Created by Nicholas Solem on 1/16/26.
//

#pragma once
#include "Statistics.h"
#include <JuceHeader.h>

namespace nvs::analysis {

inline String toString(const Statistic stat) {
    switch (stat) {
        case Statistic::Mean:
            return "Mean";
        case Statistic::Median:
            return "Median";
        case Statistic::Variance:
            return "Variance";
        case Statistic::Skewness:
            return "Skewness";
        case Statistic::Kurtosis:
            return "Kurtosis";
        default:
            return "";
    }
}

inline juce::StringArray getStatisticsStringArray() {
    static const juce::StringArray ret = []() {
        juce::StringArray a;
        for (auto const &i : statisticIterator()) {
            a.add(toString(i));
        }
        return a;
    }();
    return ret;
}

} // namespace nvs::analysis
